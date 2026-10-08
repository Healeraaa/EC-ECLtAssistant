#include "../SignalFilterPipeline.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {

double standardDeviation(
    const std::vector<double>& values,
    std::size_t begin,
    std::size_t end)
{
    double mean = 0.0;
    for (std::size_t index = begin; index < end; ++index) mean += values[index];
    mean /= static_cast<double>(end - begin);
    double sum = 0.0;
    for (std::size_t index = begin; index < end; ++index) {
        const double delta = values[index] - mean;
        sum += delta * delta;
    }
    return std::sqrt(sum / static_cast<double>(end - begin - 1));
}

bool nearlyEqual(double left, double right, double tolerance)
{
    return std::fabs(left - right) <= tolerance;
}

} // namespace

int main()
{
    std::vector<SignalFilterSample> samples(1000);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        SignalFilterSample& sample = samples[index];
        sample.timeSeconds = static_cast<double>(index) * 0.01;
        sample.voltage = (index % 5 == 0) ? -0.000326 : -0.000480;
        sample.current = (index % 7 == 0) ? -0.000285 : -0.000331;
        sample.optical = 350.0
            + 19.0 * std::sin(static_cast<double>(index) * 1.91)
            + 7.0 * std::sin(static_cast<double>(index) * 2.73);
    }

    for (std::size_t index = 300; index <= 305; ++index) {
        samples[index].voltage = 1.3;
        samples[index].current = index == 300 ? 8.5 : (index == 305 ? -9.8 : 0.5);
    }
    const double opticalPulse[] = { 350.0, 2600.0, 5400.0, 6100.0, 5200.0, 4300.0, 350.0 };
    for (std::size_t offset = 0; offset < 7; ++offset) {
        samples[300 + offset].optical = opticalPulse[offset];
    }

    SignalFilterPipeline pipeline;
    const SignalFilterBatchResult result = pipeline.process(samples);
    if (result.samples.size() != samples.size()) {
        std::cerr << "FAILED: sample count changed\n";
        return 1;
    }
    if (result.pulses.size() != 1 || !result.pulses.front().valid) {
        std::cerr << "FAILED: GPCI pulse detection\n";
        return 1;
    }
    if (result.samples[63].filterValid || !result.samples[64].filterValid) {
        std::cerr << "FAILED: 0.64 s validity boundary\n";
        return 1;
    }
    if (!result.samples[302].gpciEvent) {
        std::cerr << "FAILED: pulse protection mask\n";
        return 1;
    }

    std::vector<double> rawOptical(samples.size());
    std::vector<double> filteredOptical(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        rawOptical[index] = samples[index].optical;
        filteredOptical[index] = result.samples[index].optical;
    }
    const double rawNoise = standardDeviation(rawOptical, 100, 250);
    const double filteredNoise = standardDeviation(filteredOptical, 100, 250);
    if (!(filteredNoise < rawNoise * 0.60)) {
        std::cerr << "FAILED: ECL noise reduction is insufficient: "
            << rawNoise << " -> " << filteredNoise << '\n';
        return 1;
    }

    const SignalPulseSummary& pulse = result.pulses.front();
    const double areaError = std::fabs((pulse.filteredArea - pulse.rawArea) / pulse.rawArea);
    const double peakError = std::fabs((pulse.filteredPeak - pulse.rawPeak) / pulse.rawPeak);
    if (areaError > 0.02 || peakError > 0.03) {
        std::cerr << "FAILED: pulse fidelity, area=" << areaError
            << ", peak=" << peakError << '\n';
        return 1;
    }

    std::vector<SignalFilterSample> constantSamples(256);
    for (std::size_t index = 0; index < constantSamples.size(); ++index) {
        constantSamples[index] = {
            static_cast<double>(index) * 0.01,
            0.25,
            -0.5,
            123.0
        };
    }
    const SignalFilterBatchResult constantResult = pipeline.process(constantSamples);
    for (const SignalFilterResult& sample : constantResult.samples) {
        if (!nearlyEqual(sample.voltage, 0.25, 1e-9)
            || !nearlyEqual(sample.current, -0.5, 1e-9)
            || !nearlyEqual(sample.optical, 123.0, 1e-9)) {
            std::cerr << "FAILED: constant signal changed\n";
            return 1;
        }
    }

    std::cout << "SignalFilterPipeline tests passed. Noise "
        << rawNoise << " -> " << filteredNoise
        << ", area error " << areaError * 100.0 << "%\n";
    return 0;
}
