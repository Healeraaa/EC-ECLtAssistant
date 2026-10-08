#include "PulseAreaAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <vector>

PulseAreaAnalyzer::PulseAreaAnalyzer(
    double voltageThreshold,
    std::size_t baselineSamples)
    : m_voltageThreshold(voltageThreshold > 0.0 ? voltageThreshold : 0.1),
      m_baselineSamples(std::max<std::size_t>(1, baselineSamples))
{
}

void PulseAreaAnalyzer::reset()
{
    m_rawBaselineHistory.clear();
    m_filteredBaselineHistory.clear();
    m_current = PulseAreaMeasurement{};
    m_previousRaw = SignalFilterSample{};
    m_previousFiltered = SignalFilterResult{};
    m_active = false;
    m_nextPulseIndex = 1;
}

bool PulseAreaAnalyzer::process(
    const SignalFilterSample& raw,
    const SignalFilterResult& filtered,
    PulseAreaMeasurement* completedPulse)
{
    if (!std::isfinite(raw.timeSeconds)
        || !std::isfinite(raw.voltage)
        || !std::isfinite(raw.optical)
        || !std::isfinite(filtered.optical)) {
        return false;
    }

    const bool voltageActive = std::fabs(raw.voltage) > m_voltageThreshold;
    if (!m_active) {
        if (!voltageActive) {
            appendBaselineSample(raw.optical, filtered.optical);
            return false;
        }

        m_active = true;
        m_current = PulseAreaMeasurement{};
        m_current.index = m_nextPulseIndex++;
        m_current.startTimeSeconds = raw.timeSeconds;
        m_current.endTimeSeconds = raw.timeSeconds;
        m_current.rawBaseline = median(m_rawBaselineHistory);
        m_current.filteredBaseline = median(m_filteredBaselineHistory);
        m_current.rawBaselineNoiseRms = standardDeviation(m_rawBaselineHistory);
        m_current.filteredBaselineNoiseRms = standardDeviation(m_filteredBaselineHistory);
        m_current.rawPeak = raw.optical - m_current.rawBaseline;
        m_current.filteredPeak = filtered.optical - m_current.filteredBaseline;
        m_current.valid = m_rawBaselineHistory.size() >= m_baselineSamples
            && m_filteredBaselineHistory.size() >= m_baselineSamples
            && filtered.filterValid;
        m_previousRaw = raw;
        m_previousFiltered = filtered;
        return false;
    }

    const double deltaTime = raw.timeSeconds - m_previousRaw.timeSeconds;
    if (deltaTime > 0.0 && deltaTime <= 0.05) {
        const double previousRaw = m_previousRaw.optical - m_current.rawBaseline;
        const double currentRaw = raw.optical - m_current.rawBaseline;
        const double previousFiltered = m_previousFiltered.optical - m_current.filteredBaseline;
        const double currentFiltered = filtered.optical - m_current.filteredBaseline;
        m_current.rawArea += 0.5 * (previousRaw + currentRaw) * deltaTime;
        m_current.filteredArea += 0.5 * (previousFiltered + currentFiltered) * deltaTime;
    }
    else {
        m_current.valid = false;
    }

    m_current.rawPeak = std::max(
        m_current.rawPeak,
        raw.optical - m_current.rawBaseline);
    m_current.filteredPeak = std::max(
        m_current.filteredPeak,
        filtered.optical - m_current.filteredBaseline);
    m_current.valid = m_current.valid && filtered.filterValid;
    m_current.endTimeSeconds = raw.timeSeconds;
    m_previousRaw = raw;
    m_previousFiltered = filtered;

    if (voltageActive) return false;

    m_active = false;
    finalizeMeasurement(&m_current);
    if (completedPulse) *completedPulse = m_current;
    appendBaselineSample(raw.optical, filtered.optical);
    return true;
}

bool PulseAreaAnalyzer::flush(PulseAreaMeasurement* incompletePulse)
{
    if (!m_active) return false;
    m_current.valid = false;
    m_current.endTimeSeconds = m_previousRaw.timeSeconds;
    finalizeMeasurement(&m_current);
    if (incompletePulse) *incompletePulse = m_current;
    m_active = false;
    return true;
}

double PulseAreaAnalyzer::median(const std::deque<double>& values)
{
    if (values.empty()) return 0.0;
    std::vector<double> sorted(values.begin(), values.end());
    const std::size_t middle = sorted.size() / 2;
    std::nth_element(sorted.begin(), sorted.begin() + middle, sorted.end());
    const double upper = sorted[middle];
    if ((sorted.size() % 2) != 0) return upper;
    std::nth_element(sorted.begin(), sorted.begin() + middle - 1, sorted.end());
    return 0.5 * (sorted[middle - 1] + upper);
}

double PulseAreaAnalyzer::standardDeviation(const std::deque<double>& values)
{
    if (values.size() < 2) return 0.0;
    double sum = 0.0;
    for (const double value : values) sum += value;
    const double mean = sum / static_cast<double>(values.size());
    double squaredDifference = 0.0;
    for (const double value : values) {
        const double difference = value - mean;
        squaredDifference += difference * difference;
    }
    return std::sqrt(squaredDifference / static_cast<double>(values.size() - 1));
}

void PulseAreaAnalyzer::finalizeMeasurement(PulseAreaMeasurement* measurement)
{
    if (!measurement) return;

    if (std::fabs(measurement->rawArea) > 1e-18) {
        measurement->areaDifferencePercent =
            (measurement->filteredArea - measurement->rawArea)
            / measurement->rawArea * 100.0;
    }

    const bool areaAvailable = std::fabs(measurement->rawArea) > 1e-18;
    const bool noiseAvailable = measurement->rawBaselineNoiseRms > 1e-18
        && measurement->filteredBaselineNoiseRms > 1e-18;
    const bool peakAvailable = std::fabs(measurement->rawPeak) > 1e-18
        && std::fabs(measurement->filteredPeak) > 1e-18;
    if (areaAvailable && noiseAvailable && peakAvailable) {
        measurement->noiseReductionPercent =
            (1.0 - measurement->filteredBaselineNoiseRms
                / measurement->rawBaselineNoiseRms) * 100.0;
        const double rawSnr = std::fabs(measurement->rawPeak)
            / measurement->rawBaselineNoiseRms;
        const double filteredSnr = std::fabs(measurement->filteredPeak)
            / measurement->filteredBaselineNoiseRms;
        measurement->snrImprovementDb = 20.0 * std::log10(filteredSnr / rawSnr);
        measurement->qualityEvaluated = measurement->valid
            && std::isfinite(measurement->noiseReductionPercent)
            && std::isfinite(measurement->snrImprovementDb)
            && std::isfinite(measurement->areaDifferencePercent);
    }

    measurement->qualityWarning = !measurement->valid
        || (measurement->qualityEvaluated
            && (std::fabs(measurement->areaDifferencePercent) > 5.0
                || measurement->noiseReductionPercent < 0.0
                || measurement->snrImprovementDb < 0.0));
}

void PulseAreaAnalyzer::appendBaselineSample(
    double rawOptical,
    double filteredOptical)
{
    m_rawBaselineHistory.push_back(rawOptical);
    m_filteredBaselineHistory.push_back(filteredOptical);
    while (m_rawBaselineHistory.size() > m_baselineSamples) {
        m_rawBaselineHistory.pop_front();
    }
    while (m_filteredBaselineHistory.size() > m_baselineSamples) {
        m_filteredBaselineHistory.pop_front();
    }
}
