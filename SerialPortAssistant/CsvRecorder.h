#ifndef CSVRECORDER_H
#define CSVRECORDER_H

#include <QFile>
#include <QMap>
#include <QString>
#include <QTextStream>

class CsvRecorder
{
public:
    bool start(const QString& filePath, QString* errorMessage = nullptr);
    bool start(
        const QString& filePath,
        bool includeFilteredColumns,
        QString* errorMessage = nullptr);
    bool stop(QString* errorMessage = nullptr);
    bool flush(QString* errorMessage = nullptr);

    void appendIv(double timeSeconds, double voltage, double current);
    void appendLight(double timeSeconds, double opticalSignal);
    void appendFiltered(
        double timeSeconds,
        double voltage,
        double current,
        double opticalSignal,
        bool filterValid,
        bool gpciEvent);

    bool isRecording() const { return m_file.isOpen(); }
    QString filePath() const { return m_file.fileName(); }
    quint64 rowsWritten() const { return m_rowsWritten; }
    int pendingRows() const { return m_pendingRows.size(); }

private:
    struct CsvRow {
        double voltage = 0.0;
        double current = 0.0;
        double optical = 0.0;
        bool hasVoltage = false;
        bool hasCurrent = false;
        bool hasOptical = false;
        double filteredVoltage = 0.0;
        double filteredCurrent = 0.0;
        double filteredOptical = 0.0;
        bool filterValid = false;
        bool gpciEvent = false;
        bool hasFiltered = false;
    };

    static qint64 timeKey(double timeSeconds);
    bool writeRowsUpTo(qint64 inclusiveKey, QString* errorMessage);
    bool writeAllRows(QString* errorMessage);

    QFile m_file;
    QTextStream m_stream;
    QMap<qint64, CsvRow> m_pendingRows;
    qint64 m_latestIvKey = 0;
    qint64 m_latestLightKey = 0;
    qint64 m_latestFilteredKey = 0;
    bool m_hasIv = false;
    bool m_hasLight = false;
    bool m_hasFiltered = false;
    bool m_includeFilteredColumns = false;
    quint64 m_rowsWritten = 0;
};

#endif // CSVRECORDER_H
