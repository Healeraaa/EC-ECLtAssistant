#include "SerialPortAssistant.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>
#include <cmath>

namespace {

QString pulseQualityReason(
    const PulseAreaMeasurement& pulse,
    const PulseQualityThresholds& thresholds)
{
    if (!pulse.valid) return "InvalidPulse";
    if (!pulse.qualityEvaluated) return "NotEvaluated";

    QStringList reasons;
    if (std::fabs(pulse.areaDifferencePercent)
        > thresholds.maximumAreaDifferencePercent) {
        reasons.append("AreaDistortion");
    }
    if (pulse.noiseReductionPercent < thresholds.minimumNoiseReductionPercent) {
        reasons.append("NoiseReductionLow");
    }
    if (pulse.snrImprovementDb < thresholds.minimumSnrImprovementDb) {
        reasons.append("SnrImprovementLow");
    }
    return reasons.isEmpty() ? QString("OK") : reasons.join('|');
}

} // namespace

void SerialPortAssistant::resetPulseAreaResults()
{
    m_pulseAreaAnalyzer.setQualityThresholds(currentPulseQualityThresholds());
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
    if (m_csvRecorder.isRecording()
        && incomplete.index >= m_recordingFirstPulseIndex) {
        m_recordedPulseSummaries.append(incomplete);
    }
    updatePulseAreaStatus();
}

void SerialPortAssistant::captureRecordingIncompletePulse()
{
    if (!m_csvRecorder.isRecording() || !m_realtimeFilter.isEnabled()) return;
    PulseAreaMeasurement incomplete;
    if (!m_pulseAreaAnalyzer.snapshotIncomplete(&incomplete)
        || incomplete.index < m_recordingFirstPulseIndex) {
        return;
    }
    if (!m_recordedPulseSummaries.isEmpty()
        && m_recordedPulseSummaries.last().index == incomplete.index) {
        m_recordedPulseSummaries.last() = incomplete;
    }
    else {
        m_recordedPulseSummaries.append(incomplete);
    }
}

PulseQualityThresholds SerialPortAssistant::currentPulseQualityThresholds() const
{
    PulseQualityThresholds thresholds;
    if (Spin_MaxAreaDifference) {
        thresholds.maximumAreaDifferencePercent = Spin_MaxAreaDifference->value();
    }
    if (Spin_MinNoiseReduction) {
        thresholds.minimumNoiseReductionPercent = Spin_MinNoiseReduction->value();
    }
    if (Spin_MinSnrImprovement) {
        thresholds.minimumSnrImprovementDb = Spin_MinSnrImprovement->value();
    }
    return thresholds;
}

void SerialPortAssistant::applyPulseQualityThresholds()
{
    const PulseQualityThresholds thresholds = currentPulseQualityThresholds();
    m_pulseAreaAnalyzer.setQualityThresholds(thresholds);
    for (PulseAreaMeasurement& pulse : m_pulseSummaries) {
        PulseAreaAnalyzer::applyQualityThresholds(&pulse, thresholds);
    }
    for (PulseAreaMeasurement& pulse : m_recordedPulseSummaries) {
        PulseAreaAnalyzer::applyQualityThresholds(&pulse, thresholds);
    }
    updatePulseAreaStatus();
}

void SerialPortAssistant::setPulseQualityControlsEnabled(bool enabled)
{
    Spin_MaxAreaDifference->setEnabled(enabled);
    Spin_MinNoiseReduction->setEnabled(enabled);
    Spin_MinSnrImprovement->setEnabled(enabled);
}

void SerialPortAssistant::updatePulseAreaStatus()
{
    if (!Label_PulseAreaStatus) return;

    QVector<double> validAreas;
    const PulseAreaMeasurement* latestQuality = nullptr;
    int warningCount = 0;
    validAreas.reserve(m_pulseSummaries.size());
    for (const PulseAreaMeasurement& pulse : m_pulseSummaries) {
        if (pulse.valid && std::isfinite(pulse.filteredArea)) {
            validAreas.append(pulse.filteredArea);
        }
        if (pulse.qualityEvaluated) latestQuality = &pulse;
        if (pulse.qualityWarning) ++warningCount;
    }
    if (validAreas.isEmpty()) {
        Label_PulseAreaStatus->setText(m_pulseSummaries.isEmpty()
            ? QString::fromUtf8("ECL 积分：暂无脉冲")
            : QString::fromUtf8("ECL 积分：尚无有效脉冲"));
        if (Label_FilterQualityStatus) {
            Label_FilterQualityStatus->setText(
                QString::fromUtf8("滤波质量：暂无有效脉冲"));
        }
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

    if (!Label_FilterQualityStatus) return;
    if (!latestQuality) {
        Label_FilterQualityStatus->setText(
            QString::fromUtf8("滤波质量：背景噪声不足，暂无法评价"));
        return;
    }
    const QString snrText = QString("%1%2")
        .arg(latestQuality->snrImprovementDb >= 0.0 ? "+" : "")
        .arg(latestQuality->snrImprovementDb, 0, 'f', 2);
    const QString qualityText = warningCount > 0
        ? QString::fromUtf8("有警告")
        : QString::fromUtf8("通过");
    Label_FilterQualityStatus->setText(
        QString::fromUtf8(
            "滤波质量：%1 | 最近 SNR %2 dB | 噪声下降 %3% | 面积偏差 %4% | 警告 %5/%6")
            .arg(qualityText)
            .arg(snrText)
            .arg(latestQuality->noiseReductionPercent, 0, 'f', 2)
            .arg(latestQuality->areaDifferencePercent, 0, 'f', 3)
            .arg(warningCount)
            .arg(m_pulseSummaries.size()));
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
        "RawPeak,FilteredPeak,RawArea,FilteredArea,RawNoiseRMS,FilteredNoiseRMS,"
        "NoiseReduction(%),SNRImprovement(dB),AreaDifference(%),"
        "MaxAreaDifferenceThreshold(%),MinNoiseReductionThreshold(%),"
        "MinSNRImprovementThreshold(dB),QualityEvaluated,QualityWarning,"
        "QualityReason,Valid\n";
    const PulseQualityThresholds thresholds = currentPulseQualityThresholds();
    for (const PulseAreaMeasurement& pulse : m_recordedPulseSummaries) {
        stream << pulse.index << ','
            << QString::number(pulse.startTimeSeconds, 'f', 6) << ','
            << QString::number(pulse.endTimeSeconds, 'f', 6) << ','
            << QString::number(pulse.rawBaseline, 'f', 9) << ','
            << QString::number(pulse.filteredBaseline, 'f', 9) << ','
            << QString::number(pulse.rawPeak, 'f', 9) << ','
            << QString::number(pulse.filteredPeak, 'f', 9) << ','
            << QString::number(pulse.rawArea, 'f', 9) << ','
            << QString::number(pulse.filteredArea, 'f', 9) << ','
            << QString::number(pulse.rawBaselineNoiseRms, 'f', 9) << ','
            << QString::number(pulse.filteredBaselineNoiseRms, 'f', 9) << ','
            << QString::number(pulse.noiseReductionPercent, 'f', 6) << ','
            << QString::number(pulse.snrImprovementDb, 'f', 6) << ','
            << QString::number(pulse.areaDifferencePercent, 'f', 6) << ','
            << QString::number(thresholds.maximumAreaDifferencePercent, 'f', 3) << ','
            << QString::number(thresholds.minimumNoiseReductionPercent, 'f', 3) << ','
            << QString::number(thresholds.minimumSnrImprovementDb, 'f', 3) << ','
            << (pulse.qualityEvaluated ? 1 : 0) << ','
            << (pulse.qualityWarning ? 1 : 0) << ','
            << pulseQualityReason(pulse, thresholds) << ','
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
