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
    bool hasVoltage = false;
    bool hasCurrent = false;
    bool hasOptical = false;
};

class CsvPlaybackData
{
public:
    bool load(const QString& filePath, QString* errorMessage = nullptr);
    void clear();

    const QVector<CsvPlaybackRow>& rows() const { return m_rows; }
    QString filePath() const { return m_filePath; }
    bool isEmpty() const { return m_rows.isEmpty(); }
    double firstTime() const;
    double lastTime() const;
    double duration() const;

private:
    QVector<CsvPlaybackRow> m_rows;
    QString m_filePath;
};

#endif // CSVPLAYBACKDATA_H
