#include "CsvRecorder.h"

#include <QtMath>
#include <cmath>

namespace {
constexpr int kMaximumPendingRows = 10000;
constexpr int kRowsKeptWhenStreamLags = 5000;
}

bool CsvRecorder::start(const QString& filePath, QString* errorMessage)
{
    if (isRecording()) {
        return true;
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::NewOnly)) {
        if (errorMessage) {
            *errorMessage = m_file.errorString();
        }
        return false;
    }

    m_stream.setDevice(&m_file);
    m_stream.resetStatus();
    m_stream.setCodec("UTF-8");
    m_stream << "Time(s),Voltage(V),Current(A),OpticalSignal\n";
    m_pendingRows.clear();
    m_latestIvKey = 0;
    m_latestLightKey = 0;
    m_hasIv = false;
    m_hasLight = false;
    m_rowsWritten = 0;
    return true;
}

bool CsvRecorder::stop(QString* errorMessage)
{
    if (!isRecording()) {
        return true;
    }

    const bool writeSucceeded = writeAllRows(errorMessage);
    m_stream.flush();
    const bool streamSucceeded = m_stream.status() == QTextStream::Ok;
    m_file.flush();
    const bool fileSucceeded = m_file.error() == QFileDevice::NoError;

    if ((!streamSucceeded || !fileSucceeded) && errorMessage && errorMessage->isEmpty()) {
        *errorMessage = m_file.errorString();
    }

    m_stream.setDevice(nullptr);
    m_file.close();
    m_pendingRows.clear();
    return writeSucceeded && streamSucceeded && fileSucceeded;
}

bool CsvRecorder::flush(QString* errorMessage)
{
    if (!isRecording() || m_pendingRows.isEmpty()) {
        return true;
    }

    bool succeeded = true;
    if (m_hasIv && m_hasLight) {
        succeeded = writeRowsUpTo(qMin(m_latestIvKey, m_latestLightKey), errorMessage);
    }

    if (succeeded && m_pendingRows.size() > kMaximumPendingRows) {
        auto threshold = m_pendingRows.cbegin();
        const int rowsToWrite = m_pendingRows.size() - kRowsKeptWhenStreamLags;
        for (int index = 1; index < rowsToWrite && threshold != m_pendingRows.cend(); ++index) {
            ++threshold;
        }
        if (threshold != m_pendingRows.cend()) {
            succeeded = writeRowsUpTo(threshold.key(), errorMessage);
        }
    }

    m_stream.flush();
    if (m_stream.status() != QTextStream::Ok) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = m_file.errorString();
        }
        return false;
    }
    return succeeded;
}

void CsvRecorder::appendIv(double timeSeconds, double voltage, double current)
{
    const qint64 key = timeKey(timeSeconds);
    CsvRow& row = m_pendingRows[key];
    if (std::isfinite(voltage)) {
        row.voltage = voltage;
        row.hasVoltage = true;
    }
    if (std::isfinite(current)) {
        row.current = current;
        row.hasCurrent = true;
    }
    m_latestIvKey = qMax(m_latestIvKey, key);
    m_hasIv = true;
}

void CsvRecorder::appendLight(double timeSeconds, double opticalSignal)
{
    const qint64 key = timeKey(timeSeconds);
    CsvRow& row = m_pendingRows[key];
    if (std::isfinite(opticalSignal)) {
        row.optical = opticalSignal;
        row.hasOptical = true;
    }
    m_latestLightKey = qMax(m_latestLightKey, key);
    m_hasLight = true;
}

qint64 CsvRecorder::timeKey(double timeSeconds)
{
    return qRound64(timeSeconds * 1000000.0);
}

bool CsvRecorder::writeRowsUpTo(qint64 inclusiveKey, QString* errorMessage)
{
    auto iterator = m_pendingRows.begin();
    while (iterator != m_pendingRows.end() && iterator.key() <= inclusiveKey) {
        const CsvRow row = iterator.value();
        m_stream << QString::number(iterator.key() / 1000000.0, 'f', 6) << ',';
        if (row.hasVoltage) {
            m_stream << QString::number(row.voltage, 'f', 6);
        }
        m_stream << ',';
        if (row.hasCurrent) {
            m_stream << QString::number(row.current, 'f', 6);
        }
        m_stream << ',';
        if (row.hasOptical) {
            m_stream << QString::number(row.optical, 'f', 6);
        }
        m_stream << '\n';
        ++m_rowsWritten;
        iterator = m_pendingRows.erase(iterator);
    }

    if (m_stream.status() != QTextStream::Ok) {
        if (errorMessage) {
            *errorMessage = m_file.errorString();
        }
        return false;
    }
    return true;
}

bool CsvRecorder::writeAllRows(QString* errorMessage)
{
    if (m_pendingRows.isEmpty()) {
        return true;
    }
    return writeRowsUpTo(m_pendingRows.lastKey(), errorMessage);
}
