#ifndef SIGNALFILTERPIPELINE_H
#define SIGNALFILTERPIPELINE_H

#include <cstddef>
#include <vector>

struct SignalFilterSample
{
    double timeSeconds = 0.0;
    double voltage = 0.0;
    double current = 0.0;
    double optical = 0.0;
};

struct SignalFilterResult
{
    double voltage = 0.0;
    double current = 0.0;
    double optical = 0.0;
    bool filterValid = false;
    bool gpciEvent = false;
};

struct SignalPulseSummary
{
    int index = 0;
    double startTimeSeconds = 0.0;
    double endTimeSeconds = 0.0;
    double localBaseline = 0.0;
    double rawPeak = 0.0;
    double filteredPeak = 0.0;
    double rawArea = 0.0;
    double filteredArea = 0.0;
    bool valid = false;
};

struct SignalFilterSettings
{
    double sampleRateHz = 100.0;
    std::size_t blockSize = 128;
    std::size_t hopSize = 64;
    double gpciVoltageThreshold = 0.1;
    std::size_t baselineSamples = 50;
    std::size_t eventReleaseSamples = 2;
    double opticalKalmanGain = 0.20;
    double voltageKalmanGain = 0.30;
    double currentKalmanGain = 0.30;
    double opticalWaveletThresholdScale = 0.80;
    double currentWaveletThresholdScale = 0.60;
};

struct SignalFilterBatchResult
{
    std::vector<SignalFilterResult> samples;
    std::vector<SignalPulseSummary> pulses;
};

class SignalFilterPipeline
{
public:
    explicit SignalFilterPipeline(const SignalFilterSettings& settings = {});

    SignalFilterBatchResult process(const std::vector<SignalFilterSample>& samples) const;

private:
    SignalFilterSettings m_settings;
};

#endif // SIGNALFILTERPIPELINE_H
