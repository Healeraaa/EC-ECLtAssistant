#include "SerialPortAssistant.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <cmath>

void SerialPortAssistant::resetPulseAreaResults()
{
    m_pulseAreaAnalyzer.reset();
    m_pulseSummaries.clear();
    m_recordedPulseSummaries.clear();
    updatePulseAreaStatus();
}

void SerialPortAssistant::flushPulseAreaAnalyzer()
{
    PulseAreaMeasurement incomplete;
    if (!m_pulseAreaAnalyzer.flush(&incomplete)) return;
    m_pulseSummaries.append(incomplete);
    if (m_csvRecorder.isRecording()) m_recordedPulseSummaries.append(incomplete);
    updatePulseAreaStatus();
}

void SerialPortAssistant::updatePulseAreaStatus()
{
    if (!Label_PulseAreaStatus) return;

    QVector<double> validAreas;
    validAreas.reserve(m_pulseSummaries.size());
    for (const PulseAreaMeasurement& pulse : m_pulseSummaries) {
        if (pulse.valid && std::isfinite(pulse.filteredArea)) {
            validAreas.append(pulse.filteredArea);
        }
    }
    if (validAreas.isEmpty()) {
        Label_PulseAreaStatus->setText(m_pulseSummaries.isEmpty()
            ? QString::fromUtf8("ECL 积分：暂无脉冲")
            : QString::fromUtf8("ECL 积分：尚无有效脉冲"));
        return;
    }

    double sum = 0.0;
    for (double area : validAreas) sum += area;
    const double mean = sum / validAreas.size();
    double squaredDifference = 0.0;
    for (double area : validAreas) {
        const double difference = area - mean;
        squaredDifference += difference * difference;
    }
    const double standardDeviation = validAreas.size() > 1
        ? std::sqrt(squaredDifference / (validAreas.size() - 1))
        : 0.0;
    const double coefficientOfVariation = std::fabs(mean) > 1e-18
        ? standardDeviation / std::fabs(mean) * 100.0
        : 0.0;

    Label_PulseAreaStatus->setText(
        QString::fromUtf8("ECL 积分：N=%1 | 最近=%2 | 平均=%3 | CV=%4%")
            .arg(validAreas.size())
            .arg(validAreas.last(), 0, 'g', 8)
            .arg(mean, 0, 'g', 8)
            .arg(coefficientOfVariation, 0, 'f', 2));
}

bool SerialPortAssistant::writePulseSummaryFile(
    const QString& dataFilePath,
    QString* summaryFilePath,
    QString* errorMessage) const
{
    const QFileInfo dataFile(dataFilePath);
    const QString outputPath = dataFile.dir().filePath(
        dataFile.completeBaseName() + "_pulses.csv");
    QSaveFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = output.errorString();
        return false;
    }

    QTextStream stream(&output);
    stream.setCodec("UTF-8");
    stream << "PulseIndex,StartTime(s),EndTime(s),RawBaseline,FilteredBaseline,"
        "RawPeak,FilteredPeak,RawArea,FilteredArea,AreaDifference(%),Valid\n";
    for (const PulseAreaMeasurement& pulse : m_recordedPulseSummaries) {
        const double areaDifference = std::fabs(pulse.rawArea) > 1e-18
            ? (pulse.filteredArea - pulse.rawArea) / pulse.rawArea * 100.0
            : 0.0;
        stream << pulse.index << ','
            << QString::number(pulse.startTimeSeconds, 'f', 6) << ','
            << QString::number(pulse.endTimeSeconds, 'f', 6) << ','
            << QString::number(pulse.rawBaseline, 'f', 9) << ','
            << QString::number(pulse.filteredBaseline, 'f', 9) << ','
            << QString::number(pulse.rawPeak, 'f', 9) << ','
            << QString::number(pulse.filteredPeak, 'f', 9) << ','
            << QString::number(pulse.rawArea, 'f', 9) << ','
            << QString::number(pulse.filteredArea, 'f', 9) << ','
            << QString::number(areaDifference, 'f', 6) << ','
            << (pulse.valid ? 1 : 0) << '\n';
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !output.commit()) {
        if (errorMessage) *errorMessage = output.errorString();
        return false;
    }
    if (summaryFilePath) *summaryFilePath = outputPath;
    return true;
}
