#include "../PlotDataBuffer.h"

#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    PlotDataBuffer buffer;
    if (!buffer.append(0, 0.0, 1.0)
        || !buffer.append(0, 1.0, 3.0)
        || !buffer.append(1, 0.5, -2.0)
        || !buffer.append(2, 0.25, 10.0)) {
        std::cerr << "FAILED: valid points were rejected\n";
        return 1;
    }
    if (buffer.append(3, 0.0, 1.0)
        || buffer.append(0, 0.0, std::numeric_limits<double>::quiet_NaN())) {
        std::cerr << "FAILED: invalid point was accepted\n";
        return 1;
    }

    buffer.append(0, 2.0, 5.0, false);
    if (buffer.points(0).size() != 2
        || buffer.statistics(0).count != 3
        || buffer.statistics(0).minimum != 1.0
        || buffer.statistics(0).maximum != 5.0) {
        std::cerr << "FAILED: statistics and retained points diverged\n";
        return 1;
    }

    const std::array<bool, 3> visible{{ true, true, false }};
    const PlotDataRange range = buffer.range(visible);
    if (!range.hasX || !range.hasLeftAxis || range.hasRightAxis
        || range.xMinimum != 0.0 || range.xMaximum != 1.0
        || range.leftMinimum != -2.0 || range.leftMaximum != 3.0) {
        std::cerr << "FAILED: visible range calculation mismatch\n";
        return 1;
    }

    buffer.trim(1);
    if (buffer.points(0).size() != 1 || buffer.points(0).first().y() != 3.0) {
        std::cerr << "FAILED: buffer trimming did not keep the latest point\n";
        return 1;
    }

    buffer.clear();
    if (!buffer.points(0).isEmpty() || buffer.statistics(0).hasValue) {
        std::cerr << "FAILED: clear did not reset state\n";
        return 1;
    }

    std::cout << "PlotDataBuffer tests passed.\n";
    return 0;
}
