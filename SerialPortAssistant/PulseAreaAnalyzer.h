#ifndef PULSEAREAANALYZER_H
#define PULSEAREAANALYZER_H

#include "SignalFilterPipeline.h"

#include <cstddef>
#include <deque>

struct PulseQualityThresholds
{
    double maximumAreaDifferencePercent = 5.0;
    double minimumNoiseReductionPercent = 0.0;
    double minimumSnrImprovementDb = 0.0;
};

struct PulseAreaMeasurement
{
    int index = 0;
    double startTimeSeconds = 0.0;
    double endTimeSeconds = 0.0;
    double rawBaseline = 0.0;
    double filteredBaseline = 0.0;
    double rawPeak = 0.0;
    double filteredPeak = 0.0;
    double rawArea = 0.0;
    double filteredArea = 0.0;
    double rawBaselineNoiseRms = 0.0;
    double filteredBaselineNoiseRms = 0.0;
    double noiseReductionPercent = 0.0;
    double snrImprovementDb = 0.0;
    double areaDifferencePercent = 0.0;
    bool qualityEvaluated = false;
    bool qualityWarning = false;
    bool valid = false;
};

class PulseAreaAnalyzer
{
public:
    explicit PulseAreaAnalyzer(
        double voltageThreshold = 0.1,
        std::size_t baselineSamples = 50,
        const PulseQualityThresholds& qualityThresholds = {});

    void reset();
    void setQualityThresholds(const PulseQualityThresholds& thresholds);
    const PulseQualityThresholds& qualityThresholds() const { return m_qualityThresholds; }
    int nextPulseIndex() const { return m_nextPulseIndex; }
    bool process(
        const SignalFilterSample& raw,
        const SignalFilterResult& filtered,
        PulseAreaMeasurement* completedPulse);
    bool flush(PulseAreaMeasurement* incompletePulse);
    bool snapshotIncomplete(PulseAreaMeasurement* incompletePulse) const;
    static void applyQualityThresholds(
        PulseAreaMeasurement* measurement,
        const PulseQualityThresholds& thresholds);

private:
    static double median(const std::deque<double>& values);
    static double standardDeviation(const std::deque<double>& values);
    void finalizeMeasurement(PulseAreaMeasurement* measurement) const;
    void appendBaselineSample(double rawOptical, double filteredOptical);

    double m_voltageThreshold;
    std::size_t m_baselineSamples;
    PulseQualityThresholds m_qualityThresholds;
    std::deque<double> m_rawBaselineHistory;
    std::deque<double> m_filteredBaselineHistory;
    PulseAreaMeasurement m_current;
    SignalFilterSample m_previousRaw;
    SignalFilterResult m_previousFiltered;
    bool m_active = false;
    int m_nextPulseIndex = 1;
};

#endif // PULSEAREAANALYZER_H
