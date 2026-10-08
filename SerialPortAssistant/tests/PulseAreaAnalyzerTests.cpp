#include "../PulseAreaAnalyzer.h"

#include <cmath>
#include <iostream>

int main()
{
    PulseAreaAnalyzer analyzer;
    PulseAreaMeasurement completed;
    bool receivedPulse = false;
    for (int index = 0; index < 100; ++index) {
        SignalFilterSample raw;
        raw.timeSeconds = index * 0.01;
        raw.voltage = index >= 60 && index <= 65 ? 1.3 : -0.00048;
        raw.current = -0.009;
        raw.optical = index >= 61 && index <= 65 ? 5000.0 : 350.0;

        SignalFilterResult filtered;
        filtered.voltage = raw.voltage;
        filtered.current = raw.current;
        filtered.optical = raw.optical;
        filtered.filterValid = index >= 50;
        filtered.gpciEvent = index >= 60 && index <= 66;

        if (analyzer.process(raw, filtered, &completed)) receivedPulse = true;
    }

    if (!receivedPulse || !completed.valid || completed.index != 1) {
        std::cerr << "FAILED: pulse was not completed as valid\n";
        return 1;
    }
    if (std::fabs(completed.startTimeSeconds - 0.60) > 1e-9
        || std::fabs(completed.endTimeSeconds - 0.66) > 1e-9
        || std::fabs(completed.rawPeak - 4650.0) > 1e-9
        || std::fabs(completed.filteredPeak - 4650.0) > 1e-9
        || std::fabs(completed.rawArea - 232.5) > 1e-9
        || std::fabs(completed.filteredArea - 232.5) > 1e-9) {
        std::cerr << "FAILED: pulse metrics mismatch\n";
        return 1;
    }

    analyzer.reset();
    for (int index = 0; index < 10; ++index) {
        SignalFilterSample raw { index * 0.01, index >= 5 ? 1.3 : 0.0, 0.0, 10.0 };
        SignalFilterResult filtered { 0.0, 0.0, 10.0, false, index >= 5 };
        analyzer.process(raw, filtered, nullptr);
    }
    if (!analyzer.flush(&completed) || completed.valid) {
        std::cerr << "FAILED: incomplete pulse flush\n";
        return 1;
    }

    analyzer.reset();
    receivedPulse = false;
    for (int index = 0; index < 100; ++index) {
        const bool active = index >= 60 && index <= 65;
        SignalFilterSample raw { index * 0.01, active ? 1.3 : 0.0, 0.0, active ? 5000.0 : 350.0 };
        SignalFilterResult filtered { 0.0, 0.0, raw.optical, index != 63, active };
        if (analyzer.process(raw, filtered, &completed)) receivedPulse = true;
    }
    if (!receivedPulse || completed.valid) {
        std::cerr << "FAILED: invalid filtered sample must invalidate the pulse\n";
        return 1;
    }

    std::cout << "PulseAreaAnalyzer tests passed.\n";
    return 0;
}
