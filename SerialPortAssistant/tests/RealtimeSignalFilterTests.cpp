#include "../RealtimeSignalFilter.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    RealtimeSignalFilter filter;
    std::vector<RealtimeFilteredSample> output;
    for (int index = 0; index < 192; ++index) {
        const double time = index * 0.01;
        const double voltage = index >= 130 && index <= 135 ? 1.3 : -0.00048;
        const double current = index == 130 ? 8.0 : (index == 135 ? -9.0 : -0.009);
        const double optical = index >= 131 && index <= 135
            ? 5000.0
            : 350.0 + 20.0 * std::sin(index * 1.7);

        const auto ivOutput = filter.appendIv(time, voltage, current);
        output.insert(output.end(), ivOutput.begin(), ivOutput.end());
        const auto lightOutput = filter.appendLight(time, optical);
        output.insert(output.end(), lightOutput.begin(), lightOutput.end());
        if (index == 63 && output.size() != 64) {
            std::cerr << "FAILED: first block was not emitted at 0.64 s\n";
            return 1;
        }
    }
    const auto tail = filter.flush();
    output.insert(output.end(), tail.begin(), tail.end());

    if (output.size() != 192) {
        std::cerr << "FAILED: output count " << output.size() << " != 192\n";
        return 1;
    }
    for (std::size_t index = 0; index < output.size(); ++index) {
        if (std::fabs(output[index].raw.timeSeconds - index * 0.01) > 1e-9) {
            std::cerr << "FAILED: timestamp alignment at " << index << '\n';
            return 1;
        }
    }
    if (output[63].filtered.filterValid || !output[64].filtered.filterValid) {
        std::cerr << "FAILED: filter validity boundary\n";
        return 1;
    }
    if (!output[132].filtered.gpciEvent) {
        std::cerr << "FAILED: GPCI event was not preserved\n";
        return 1;
    }
    if (std::fabs(output[132].filtered.optical - output[132].raw.optical) > 1e-6) {
        std::cerr << "FAILED: protected ECL pulse changed\n";
        return 1;
    }

    filter.reset();
    filter.setEnabled(false);
    output.clear();
    for (int index = 0; index < 10; ++index) {
        const double time = index * 0.01;
        auto part = filter.appendLight(time, 100.0 + index);
        output.insert(output.end(), part.begin(), part.end());
        part = filter.appendIv(time, 1.0, 2.0);
        output.insert(output.end(), part.begin(), part.end());
    }
    if (output.size() != 10
        || output.front().filtered.optical != output.front().raw.optical) {
        std::cerr << "FAILED: bypass mode\n";
        return 1;
    }

    std::cout << "RealtimeSignalFilter tests passed.\n";
    return 0;
}
