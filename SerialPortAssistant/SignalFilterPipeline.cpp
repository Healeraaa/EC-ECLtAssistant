#include "SignalFilterPipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace {

constexpr std::array<double, 8> kSym4LowPass = {
    -0.07576571478927333,
    -0.02963552764599851,
    0.49761866763201545,
    0.8037387518059161,
    0.29785779560527736,
    -0.09921954357684722,
    -0.012603967262037833,
    0.03222310060404270
};

double median(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    const double upper = values[middle];
    if ((values.size() % 2) != 0) return upper;
    std::nth_element(values.begin(), values.begin() + middle - 1, values.end());
    return 0.5 * (values[middle - 1] + upper);
}

double standardDeviation(const std::vector<double>& values)
{
    if (values.size() < 2) return 0.0;
    const double mean = std::accumulate(values.begin(), values.end(), 0.0)
        / static_cast<double>(values.size());
    double sum = 0.0;
    for (double value : values) {
        const double delta = value - mean;
        sum += delta * delta;
    }
    return std::sqrt(sum / static_cast<double>(values.size() - 1));
}

double robustSigma(const std::vector<double>& values)
{
    if (values.empty()) return 0.0;
    const double center = median(values);
    std::vector<double> deviations;
    deviations.reserve(values.size());
    for (double value : values) deviations.push_back(std::fabs(value - center));
    const double madSigma = 1.4826 * median(deviations);
    if (madSigma > std::numeric_limits<double>::epsilon()) return madSigma;

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t trim = sorted.size() / 10;
    if (trim * 2 < sorted.size()) {
        std::vector<double> central(sorted.begin() + trim, sorted.end() - trim);
        const double centralSigma = standardDeviation(central);
        if (centralSigma > std::numeric_limits<double>::epsilon()) return centralSigma;
    }
    return standardDeviation(values);
}

std::size_t reflectedIndex(long long index, std::size_t size)
{
    if (size <= 1) return 0;
    const long long period = static_cast<long long>(2 * size - 2);
    long long folded = index % period;
    if (folded < 0) folded += period;
    if (folded >= static_cast<long long>(size)) folded = period - folded;
    return static_cast<std::size_t>(folded);
}

std::array<double, kSym4LowPass.size()> highPassFilter()
{
    std::array<double, kSym4LowPass.size()> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        const double sign = (index % 2 == 0) ? 1.0 : -1.0;
        result[index] = sign * kSym4LowPass[result.size() - 1 - index];
    }
    return result;
}

struct WaveletLevel
{
    std::vector<double> detail;
    std::vector<bool> detailProtected;
};

void decomposeLevel(
    const std::vector<double>& input,
    const std::vector<bool>& inputProtected,
    std::vector<double>* approximation,
    std::vector<bool>* approximationProtected,
    WaveletLevel* level)
{
    const std::array<double, kSym4LowPass.size()> highPass = highPassFilter();
    const std::size_t outputSize = input.size() / 2;
    approximation->assign(outputSize, 0.0);
    approximationProtected->assign(outputSize, false);
    level->detail.assign(outputSize, 0.0);
    level->detailProtected.assign(outputSize, false);

    for (std::size_t outputIndex = 0; outputIndex < outputSize; ++outputIndex) {
        for (std::size_t tap = 0; tap < kSym4LowPass.size(); ++tap) {
            const std::size_t inputIndex = (2 * outputIndex + tap) % input.size();
            (*approximation)[outputIndex] += kSym4LowPass[tap] * input[inputIndex];
            level->detail[outputIndex] += highPass[tap] * input[inputIndex];
            if (inputProtected[inputIndex]) {
                (*approximationProtected)[outputIndex] = true;
                level->detailProtected[outputIndex] = true;
            }
        }
    }
}

std::vector<double> reconstructLevel(
    const std::vector<double>& approximation,
    const std::vector<double>& detail)
{
    const std::array<double, kSym4LowPass.size()> highPass = highPassFilter();
    std::vector<double> result(approximation.size() * 2, 0.0);
    for (std::size_t coefficient = 0; coefficient < approximation.size(); ++coefficient) {
        for (std::size_t tap = 0; tap < kSym4LowPass.size(); ++tap) {
            const std::size_t outputIndex = (2 * coefficient + tap) % result.size();
            result[outputIndex] += kSym4LowPass[tap] * approximation[coefficient]
                + highPass[tap] * detail[coefficient];
        }
    }
    return result;
}

double softThreshold(double value, double threshold)
{
    if (value > threshold) return value - threshold;
    if (value < -threshold) return value + threshold;
    return 0.0;
}

std::vector<double> denoiseBlock(
    const std::vector<double>& input,
    const std::vector<bool>& protectedSamples,
    int levelCount,
    double thresholdScale)
{
    if (input.size() < 16 || levelCount <= 0 || thresholdScale <= 0.0) return input;

    std::vector<double> approximation = input;
    std::vector<bool> approximationProtected = protectedSamples;
    std::vector<WaveletLevel> levels;
    levels.reserve(static_cast<std::size_t>(levelCount));

    for (int levelIndex = 0; levelIndex < levelCount; ++levelIndex) {
        if (approximation.size() < 2 || (approximation.size() % 2) != 0) break;
        std::vector<double> nextApproximation;
        std::vector<bool> nextProtected;
        WaveletLevel level;
        decomposeLevel(
            approximation,
            approximationProtected,
            &nextApproximation,
            &nextProtected,
            &level);
        approximation.swap(nextApproximation);
        approximationProtected.swap(nextProtected);
        levels.push_back(std::move(level));
    }

    if (levels.empty()) return input;
    std::vector<double> noiseCoefficients;
    for (std::size_t index = 0; index < levels.front().detail.size(); ++index) {
        if (!levels.front().detailProtected[index]) {
            noiseCoefficients.push_back(levels.front().detail[index]);
        }
    }
    const double sigma = robustSigma(noiseCoefficients);
    if (sigma <= std::numeric_limits<double>::epsilon()) return input;

    for (std::size_t levelIndex = 0; levelIndex < levels.size(); ++levelIndex) {
        WaveletLevel& level = levels[levelIndex];
        const double levelScale = levelIndex == 0 ? 1.0 : 0.45;
        const double threshold = thresholdScale * levelScale * sigma
            * std::sqrt(2.0 * std::log(static_cast<double>(level.detail.size())));
        for (std::size_t index = 0; index < level.detail.size(); ++index) {
            if (!level.detailProtected[index]) {
                level.detail[index] = softThreshold(level.detail[index], threshold);
            }
        }
    }

    for (std::size_t reverseIndex = levels.size(); reverseIndex > 0; --reverseIndex) {
        approximation = reconstructLevel(approximation, levels[reverseIndex - 1].detail);
    }
    return approximation;
}

std::vector<double> blockWaveletDenoise(
    const std::vector<double>& input,
    const std::vector<bool>& protectedSamples,
    std::size_t blockSize,
    std::size_t hopSize,
    int levelCount,
    double thresholdScale)
{
    if (input.empty() || blockSize < 16 || hopSize == 0 || hopSize > blockSize) return input;
    if ((blockSize & (blockSize - 1)) != 0) return input;

    std::vector<double> weighted(input.size(), 0.0);
    std::vector<double> weights(input.size(), 0.0);
    const double pi = std::acos(-1.0);

    for (std::size_t start = 0; start < input.size(); start += hopSize) {
        std::vector<double> block(blockSize, 0.0);
        std::vector<bool> blockProtected(blockSize, false);
        for (std::size_t offset = 0; offset < blockSize; ++offset) {
            const std::size_t sourceIndex = reflectedIndex(
                static_cast<long long>(start) + static_cast<long long>(offset),
                input.size());
            block[offset] = input[sourceIndex];
            blockProtected[offset] = protectedSamples[sourceIndex];
        }

        const std::vector<double> denoised = denoiseBlock(
            block, blockProtected, levelCount, thresholdScale);
        for (std::size_t offset = 0; offset < blockSize; ++offset) {
            const std::size_t destination = start + offset;
            if (destination >= input.size()) break;
            const double phase = pi * (static_cast<double>(offset) + 0.5)
                / static_cast<double>(blockSize);
            const double weight = std::sin(phase) * std::sin(phase);
            weighted[destination] += denoised[offset] * weight;
            weights[destination] += weight;
        }
    }

    std::vector<double> result(input.size(), 0.0);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = weights[index] > std::numeric_limits<double>::epsilon()
            ? weighted[index] / weights[index]
            : input[index];
    }
    return result;
}

std::vector<double> applyGatedKalman(
    const std::vector<double>& input,
    const std::vector<bool>& protectedSamples,
    double baselineGain)
{
    if (input.empty()) return {};
    baselineGain = std::max(0.01, std::min(1.0, baselineGain));
    std::vector<double> result(input.size(), 0.0);
    double state = input.front();
    result.front() = state;
    bool resetAfterEvent = false;
    for (std::size_t index = 1; index < input.size(); ++index) {
        if (protectedSamples[index]) {
            state = input[index];
            result[index] = input[index];
            resetAfterEvent = true;
        }
        else if (resetAfterEvent) {
            state = input[index];
            result[index] = input[index];
            resetAfterEvent = false;
        }
        else {
            state += baselineGain * (input[index] - state);
            result[index] = state;
        }
    }
    return result;
}

std::vector<std::pair<std::size_t, std::size_t>> voltagePulses(
    const std::vector<double>& voltage,
    double threshold)
{
    std::vector<std::pair<std::size_t, std::size_t>> result;
    std::size_t index = 0;
    while (index < voltage.size()) {
        if (std::fabs(voltage[index]) <= threshold) {
            ++index;
            continue;
        }
        const std::size_t start = index;
        while (index + 1 < voltage.size()
            && std::fabs(voltage[index + 1]) > threshold) {
            ++index;
        }
        result.emplace_back(start, index);
        ++index;
    }
    return result;
}

void markRange(
    std::vector<bool>* mask,
    std::size_t first,
    std::size_t last)
{
    if (mask->empty() || first >= mask->size()) return;
    last = std::min(last, mask->size() - 1);
    for (std::size_t index = first; index <= last; ++index) (*mask)[index] = true;
}

std::vector<bool> buildEventMask(
    const std::vector<double>& voltage,
    const std::vector<double>& current,
    const std::vector<double>& optical,
    const std::vector<std::pair<std::size_t, std::size_t>>& pulses,
    std::size_t releaseSamples)
{
    std::vector<bool> mask(voltage.size(), false);
    if (voltage.empty()) return mask;

    const double currentBaseline = median(current);
    const double opticalBaseline = median(optical);
    const double currentSigma = std::max(robustSigma(current), 1e-12);
    const double opticalSigma = std::max(robustSigma(optical), 1e-12);
    const double currentThreshold = 6.0 * currentSigma;
    const double opticalThreshold = 6.0 * opticalSigma;

    for (const auto& pulse : pulses) {
        const std::size_t start = pulse.first > 0 ? pulse.first - 1 : 0;
        std::size_t end = std::min(
            voltage.size() - 1,
            pulse.second + std::max<std::size_t>(1, releaseSamples));
        std::size_t quietCount = 0;
        const std::size_t maximumEnd = std::min(
            voltage.size() - 1,
            pulse.second + static_cast<std::size_t>(200));
        for (std::size_t index = pulse.second + 1; index <= maximumEnd; ++index) {
            const bool currentQuiet = std::fabs(current[index] - currentBaseline) <= currentThreshold;
            const bool opticalQuiet = std::fabs(optical[index] - opticalBaseline) <= opticalThreshold;
            if (currentQuiet && opticalQuiet) {
                ++quietCount;
                if (quietCount >= std::max<std::size_t>(1, releaseSamples)) {
                    end = index;
                    break;
                }
            }
            else {
                quietCount = 0;
                end = index;
            }
        }
        markRange(&mask, start, end);
    }
    return mask;
}

double rangeMedian(
    const std::vector<double>& values,
    std::size_t begin,
    std::size_t end)
{
    if (begin >= end || end > values.size()) return 0.0;
    return median(std::vector<double>(values.begin() + begin, values.begin() + end));
}

double trapezoidArea(
    const std::vector<SignalFilterSample>& samples,
    const std::vector<double>& values,
    std::size_t begin,
    std::size_t end,
    double baseline)
{
    if (begin >= end || end >= values.size()) return 0.0;
    double area = 0.0;
    for (std::size_t index = begin; index < end; ++index) {
        const double deltaTime = samples[index + 1].timeSeconds - samples[index].timeSeconds;
        if (!(deltaTime > 0.0) || !std::isfinite(deltaTime)) continue;
        area += 0.5 * ((values[index] - baseline) + (values[index + 1] - baseline))
            * deltaTime;
    }
    return area;
}

} // namespace

SignalFilterPipeline::SignalFilterPipeline(const SignalFilterSettings& settings)
    : m_settings(settings)
{
    if (!(m_settings.sampleRateHz > 0.0)) m_settings.sampleRateHz = 100.0;
    if (m_settings.blockSize < 16
        || (m_settings.blockSize & (m_settings.blockSize - 1)) != 0) {
        m_settings.blockSize = 128;
    }
    if (m_settings.hopSize == 0 || m_settings.hopSize > m_settings.blockSize) {
        m_settings.hopSize = m_settings.blockSize / 2;
    }
    if (!(m_settings.gpciVoltageThreshold > 0.0)) m_settings.gpciVoltageThreshold = 0.1;
}

SignalFilterBatchResult SignalFilterPipeline::process(
    const std::vector<SignalFilterSample>& samples) const
{
    SignalFilterBatchResult batch;
    if (samples.empty()) return batch;

    std::vector<double> voltage;
    std::vector<double> current;
    std::vector<double> optical;
    voltage.reserve(samples.size());
    current.reserve(samples.size());
    optical.reserve(samples.size());
    for (const SignalFilterSample& sample : samples) {
        voltage.push_back(sample.voltage);
        current.push_back(sample.current);
        optical.push_back(sample.optical);
    }

    const auto pulses = voltagePulses(voltage, m_settings.gpciVoltageThreshold);
    const std::vector<bool> eventMask = buildEventMask(
        voltage, current, optical, pulses, m_settings.eventReleaseSamples);

    const std::vector<double> voltageFiltered = applyGatedKalman(
        voltage, eventMask, m_settings.voltageKalmanGain);
    const std::vector<double> currentWavelet = blockWaveletDenoise(
        current,
        eventMask,
        m_settings.blockSize,
        m_settings.hopSize,
        1,
        m_settings.currentWaveletThresholdScale);
    const std::vector<double> currentFiltered = applyGatedKalman(
        currentWavelet, eventMask, m_settings.currentKalmanGain);
    const std::vector<double> opticalWavelet = blockWaveletDenoise(
        optical,
        eventMask,
        m_settings.blockSize,
        m_settings.hopSize,
        2,
        m_settings.opticalWaveletThresholdScale);
    const std::vector<double> opticalFiltered = applyGatedKalman(
        opticalWavelet, eventMask, m_settings.opticalKalmanGain);

    batch.samples.resize(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        SignalFilterResult& result = batch.samples[index];
        result.voltage = voltageFiltered[index];
        result.current = currentFiltered[index];
        result.optical = opticalFiltered[index];
        result.filterValid = index >= m_settings.hopSize;
        result.gpciEvent = eventMask[index];
    }

    int pulseIndex = 1;
    for (const auto& pulse : pulses) {
        SignalPulseSummary summary;
        summary.index = pulseIndex++;
        const std::size_t integrationStart = pulse.first;
        const std::size_t integrationEnd = std::min(samples.size() - 1, pulse.second + 1);
        const std::size_t baselineBegin = integrationStart > m_settings.baselineSamples
            ? integrationStart - m_settings.baselineSamples
            : 0;
        const double rawBaseline = rangeMedian(optical, baselineBegin, integrationStart);
        const double filteredBaseline = rangeMedian(
            opticalFiltered, baselineBegin, integrationStart);

        summary.startTimeSeconds = samples[integrationStart].timeSeconds;
        summary.endTimeSeconds = samples[integrationEnd].timeSeconds;
        summary.localBaseline = filteredBaseline;
        summary.rawPeak = *std::max_element(
            optical.begin() + integrationStart,
            optical.begin() + integrationEnd + 1) - rawBaseline;
        summary.filteredPeak = *std::max_element(
            opticalFiltered.begin() + integrationStart,
            opticalFiltered.begin() + integrationEnd + 1) - filteredBaseline;
        summary.rawArea = trapezoidArea(
            samples, optical, integrationStart, integrationEnd, rawBaseline);
        summary.filteredArea = trapezoidArea(
            samples, opticalFiltered, integrationStart, integrationEnd, filteredBaseline);
        summary.valid = integrationStart >= m_settings.baselineSamples
            && batch.samples[integrationStart].filterValid
            && integrationEnd > integrationStart;
        batch.pulses.push_back(summary);
    }

    return batch;
}
