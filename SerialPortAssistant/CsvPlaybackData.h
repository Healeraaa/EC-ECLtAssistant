#ifndef CSVPLAYBACKDATA_H
#define CSVPLAYBACKDATA_H

#include <QString>
#include <QVector>

struct CsvPlaybackRow
{
    double timeSeconds = 0.0;
    double voltage = 0.0;
    double current = 0.0;
    double optical = 0.0;
    double filteredVoltage = 0.0;
    double filteredCurrent = 0.0;
    double filteredOptical = 0.0;
    bool hasVoltage = false;
    bool hasCurrent = false;
    bool hasOptical = false;
    bool hasFiltered = false;
    bool filterValid = false;
    bool gpciEvent = false;
};

class CsvPlaybackData
{
public:
    bool load(const QString& filePath, QString* errorMessage = nullptr);
    void clear();

    const QVector<CsvPlaybackRow>& rows() const { return m_rows; }
    QString filePath() const { return m_filePath; }
    bool isEmpty() const { return m_rows.isEmpty(); }
    bool hasFilteredData() const { return m_hasFilteredData; }
    double firstTime() const;
    double lastTime() const;
    double duration() const;

private:
    QVector<CsvPlaybackRow> m_rows;
    QString m_filePath;
    bool m_hasFilteredData = false;
};

struct CsvPlaybackLoadResult
{
    bool succeeded = false;
    CsvPlaybackData data;
    QString errorMessage;
};

CsvPlaybackLoadResult loadCsvPlaybackFile(const QString& filePath);

#endif // CSVPLAYBACKDATA_H
