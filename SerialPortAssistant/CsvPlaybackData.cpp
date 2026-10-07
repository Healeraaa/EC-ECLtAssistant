#include "CsvPlaybackData.h"

#include <QFile>
#include <QTextStream>
#include <cmath>

namespace {
constexpr int kMaximumPlaybackRows = 2000000;

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

bool CsvPlaybackData::load(const QString& filePath, QString* errorMessage)
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

    const QString header = stream.readLine().trimmed();
    if (!header.startsWith("Time(s),")) {
        if (errorMessage) *errorMessage = QString::fromUtf8("CSV 表头格式不受支持。");
        return false;
    }

    double previousTime = 0.0;
    bool hasPreviousTime = false;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;

        const QStringList columns = line.split(',', Qt::KeepEmptyParts);
        if (columns.size() < 4) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("第 %1 行列数不足。").arg(lineNumber);
            }
            return false;
        }

        bool timeOk = false;
        const double time = columns.at(0).toDouble(&timeOk);
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
        row.hasVoltage = parseOptionalNumber(columns.at(1), &row.voltage);
        row.hasCurrent = parseOptionalNumber(columns.at(2), &row.current);
        row.hasOptical = parseOptionalNumber(columns.at(3), &row.optical);
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

    m_rows.swap(loadedRows);
    m_filePath = filePath;
    return true;
}

void CsvPlaybackData::clear()
{
    m_rows.clear();
    m_filePath.clear();
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
    CsvPlaybackLoadResult result;
    result.succeeded = result.data.load(filePath, &result.errorMessage);
    return result;
}
