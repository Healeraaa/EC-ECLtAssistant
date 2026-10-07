#include "../CsvPlaybackData.h"
#include "../PlotDataBuffer.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>
#include <iostream>

int main()
{
    QElapsedTimer timer;
    timer.start();

    PlotDataBuffer plotBuffer;
    constexpr int samplesPerChannel = 250000;
    for (int index = 0; index < samplesPerChannel; ++index) {
        const double time = index * 0.0001;
        plotBuffer.append(0, time, std::sin(index * 0.001));
        plotBuffer.append(1, time, std::cos(index * 0.001));
        plotBuffer.append(2, time, index == 123456 ? 5000.0 : 0.5);
    }
    plotBuffer.trim(50000);
    for (int channel = 0; channel < PlotDataBuffer::ChannelCount; ++channel) {
        const QVector<QPointF> display = plotBuffer.decimatedPoints(channel, 4000);
        if (plotBuffer.pointCount(channel) != 50000 || display.size() > 4000) {
            std::cerr << "FAILED: stress buffer limits were not maintained\n";
            return 1;
        }
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;
    const QString csvPath = temporaryDirectory.filePath("large.csv");
    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream stream(&csvFile);
    stream << "Time(s),Voltage(V),Current(A),OpticalSignal\n";
    constexpr int csvRows = 100000;
    for (int index = 0; index < csvRows; ++index) {
        stream << QString::number(index * 0.001, 'f', 6)
               << ",1.0,2.0,3.0\n";
    }
    csvFile.close();

    const CsvPlaybackLoadResult loadResult = loadCsvPlaybackFile(csvPath);
    if (!loadResult.succeeded || loadResult.data.rows().size() != csvRows) {
        std::cerr << "FAILED: large CSV load mismatch\n";
        return 1;
    }

    std::cout << "Data pipeline stress tests passed in "
              << timer.elapsed() << " ms.\n";
    return 0;
}
