#include "CsvPlaybackData.h"
#include "SignalFilterPipeline.h"

#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kMaximumPlaybackRows = 2000000;
constexpr int kMaximumAutoFilterRows = 500000;

bool parseOptionalNumber(const QString& text, double* value)
{
    if (text.trimmed().isEmpty()) return false;
    bool ok = false;
    const double parsedValue = text.toDouble(&ok);
    if (!ok || !std::isfinite(parsedValue)) return false;
    *value = parsedValue;
    return true;
}
}

bool CsvPlaybackData::load(
    const QString& filePath,
    QString* errorMessage,
    const PulseQualityThresholds& qualityThresholds)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    QVector<CsvPlaybackRow> loadedRows;
    const qint64 estimatedRows = file.size() / 32;
    loadedRows.reserve(static_cast<int>(
        qBound<qint64>(10000, estimatedRows, kMaximumPlaybackRows)));
    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    QString header = stream.readLine().trimmed();
    if (!header.isEmpty() && header.front() == QChar::ByteOrderMark) header.remove(0, 1);
    const QStringList headerColumns = header.split(',', Qt::KeepEmptyParts);
    const int timeColumn = headerColumns.indexOf("Time(s)");
    const int voltageColumn = headerColumns.indexOf("Voltage(V)");
    const int currentColumn = headerColumns.indexOf("Current(A)");
    const int opticalColumn = headerColumns.indexOf("OpticalSignal");
    const int filteredVoltageColumn = headerColumns.indexOf("VoltageFiltered(V)");
    const int filteredCurrentColumn = headerColumns.indexOf("CurrentFiltered(A)");
    const int filteredOpticalColumn = headerColumns.indexOf("OpticalSignalFiltered");
    const int filterValidColumn = headerColumns.indexOf("FilterValid");
    const int gpciEventColumn = headerColumns.indexOf("GPCIEvent");
    if (timeColumn < 0 || voltageColumn < 0 || currentColumn < 0 || opticalColumn < 0) {
        if (errorMessage) *errorMessage = QString::fromUtf8("CSV 表头格式不受支持。");
        return false;
    }
    const bool hasFilteredColumns = filteredVoltageColumn >= 0
        && filteredCurrentColumn >= 0
        && filteredOpticalColumn >= 0;
    const int requiredColumn = std::max(
        { timeColumn, voltageColumn, currentColumn, opticalColumn,
          filteredVoltageColumn, filteredCurrentColumn, filteredOpticalColumn,
          filterValidColumn, gpciEventColumn });

    double previousTime = 0.0;
    bool hasPreviousTime = false;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;

        const QStringList columns = line.split(',', Qt::KeepEmptyParts);
        if (columns.size() <= requiredColumn) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("第 %1 行列数不足。").arg(lineNumber);
            }
            return false;
        }

        bool timeOk = false;
        const double time = columns.at(timeColumn).toDouble(&timeOk);
        if (!timeOk || !std::isfinite(time)) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("第 %1 行时间值无效。").arg(lineNumber);
            }
            return false;
        }
        if (hasPreviousTime && time < previousTime) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("第 %1 行时间倒退，无法回放。").arg(lineNumber);
            }
            return false;
        }

        CsvPlaybackRow row;
        row.timeSeconds = time;
        row.hasVoltage = parseOptionalNumber(columns.at(voltageColumn), &row.voltage);
        row.hasCurrent = parseOptionalNumber(columns.at(currentColumn), &row.current);
        row.hasOptical = parseOptionalNumber(columns.at(opticalColumn), &row.optical);
        if (hasFilteredColumns) {
            row.hasFiltered = parseOptionalNumber(
                columns.at(filteredVoltageColumn), &row.filteredVoltage)
                && parseOptionalNumber(columns.at(filteredCurrentColumn), &row.filteredCurrent)
                && parseOptionalNumber(columns.at(filteredOpticalColumn), &row.filteredOptical);
            if (filterValidColumn >= 0) row.filterValid = columns.at(filterValidColumn).toInt() != 0;
            if (gpciEventColumn >= 0) row.gpciEvent = columns.at(gpciEventColumn).toInt() != 0;
        }
        if (row.hasVoltage || row.hasCurrent || row.hasOptical) {
            loadedRows.append(row);
        }

        previousTime = time;
        hasPreviousTime = true;
        if (loadedRows.size() > kMaximumPlaybackRows) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("CSV 超过 200 万行，请先分割文件后再回放。");
            }
            return false;
        }
    }

    if (loadedRows.isEmpty()) {
        if (errorMessage) *errorMessage = QString::fromUtf8("CSV 中没有可回放的数据。");
        return false;
    }

    bool filteredDataAvailable = std::any_of(
        loadedRows.cbegin(), loadedRows.cend(),
        [](const CsvPlaybackRow& row) { return row.hasFiltered; });
    if (!filteredDataAvailable
        && loadedRows.size() >= 2
        && loadedRows.size() <= kMaximumAutoFilterRows) {
        const double duration = loadedRows.last().timeSeconds - loadedRows.first().timeSeconds;
        const double averageInterval = duration / static_cast<double>(loadedRows.size() - 1);
        const bool isHundredHertz = averageInterval >= 0.009 && averageInterval <= 0.011;
        const bool hasCompleteRows = std::all_of(
            loadedRows.cbegin(), loadedRows.cend(),
            [](const CsvPlaybackRow& row) {
                return row.hasVoltage && row.hasCurrent && row.hasOptical;
            });
        if (isHundredHertz && hasCompleteRows) {
            std::vector<SignalFilterSample> samples;
            samples.reserve(static_cast<std::size_t>(loadedRows.size()));
            for (const CsvPlaybackRow& row : loadedRows) {
                samples.push_back({ row.timeSeconds, row.voltage, row.current, row.optical });
            }
            const SignalFilterBatchResult filtered = SignalFilterPipeline().process(samples);
            for (int index = 0; index < loadedRows.size(); ++index) {
                CsvPlaybackRow& row = loadedRows[index];
                const SignalFilterResult& result = filtered.samples[static_cast<std::size_t>(index)];
                row.filteredVoltage = result.voltage;
                row.filteredCurrent = result.current;
                row.filteredOptical = result.optical;
                row.hasFiltered = true;
                row.filterValid = result.filterValid;
                row.gpciEvent = result.gpciEvent;
            }
            filteredDataAvailable = true;
        }
    }

    QVector<PulseAreaMeasurement> loadedPulses;
    if (filteredDataAvailable) {
        PulseAreaAnalyzer analyzer(0.1, 50, qualityThresholds);
        for (const CsvPlaybackRow& row : loadedRows) {
            if (!row.hasVoltage || !row.hasCurrent || !row.hasOptical || !row.hasFiltered) {
                analyzer.reset();
                continue;
            }
            const SignalFilterSample raw {
                row.timeSeconds,
                row.voltage,
                row.current,
                row.optical
            };
            const SignalFilterResult filtered {
                row.filteredVoltage,
                row.filteredCurrent,
                row.filteredOptical,
                row.filterValid,
                row.gpciEvent
            };
            PulseAreaMeasurement completed;
            if (analyzer.process(raw, filtered, &completed)) {
                loadedPulses.append(completed);
            }
        }
        PulseAreaMeasurement incomplete;
        if (analyzer.flush(&incomplete)) loadedPulses.append(incomplete);
    }

    m_rows.swap(loadedRows);
    m_pulses.swap(loadedPulses);
    m_filePath = filePath;
    m_hasFilteredData = filteredDataAvailable;
    return true;
}

void CsvPlaybackData::clear()
{
    m_rows.clear();
    m_filePath.clear();
    m_hasFilteredData = false;
    m_pulses.clear();
}

double CsvPlaybackData::firstTime() const
{
    return m_rows.isEmpty() ? 0.0 : m_rows.first().timeSeconds;
}

double CsvPlaybackData::lastTime() const
{
    return m_rows.isEmpty() ? 0.0 : m_rows.last().timeSeconds;
}

double CsvPlaybackData::duration() const
{
    return m_rows.isEmpty() ? 0.0 : lastTime() - firstTime();
}

CsvPlaybackLoadResult loadCsvPlaybackFile(const QString& filePath)
{
    return loadCsvPlaybackFile(filePath, PulseQualityThresholds{});
}

CsvPlaybackLoadResult loadCsvPlaybackFile(
    const QString& filePath,
    const PulseQualityThresholds& qualityThresholds)
{
    CsvPlaybackLoadResult result;
    result.succeeded = result.data.load(
        filePath,
        &result.errorMessage,
        qualityThresholds);
    return result;
}
