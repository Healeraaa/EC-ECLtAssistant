#include "../CsvRecorder.h"
#include "../RealtimeSignalFilter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>
#include <iostream>

namespace {

void appendFilteredRows(
    CsvRecorder* recorder,
    const std::vector<RealtimeFilteredSample>& rows)
{
    for (const RealtimeFilteredSample& row : rows) {
        recorder->appendFiltered(
            row.raw.timeSeconds,
            row.filtered.voltage,
            row.filtered.current,
            row.filtered.optical,
            row.filtered.filterValid,
            row.filtered.gpciEvent);
    }
}

} // namespace

int main()
{
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    CsvRecorder recorder;
    QString errorMessage;
    const QString filePath = directory.filePath("realtime.csv");
    if (!recorder.start(filePath, true, &errorMessage)) {
        std::cerr << "FAILED: " << errorMessage.toStdString() << '\n';
        return 1;
    }

    RealtimeSignalFilter filter;
    constexpr int sampleCount = 300;
    for (int index = 0; index < sampleCount; ++index) {
        const double time = index * 0.01;
        const double voltage = index >= 200 && index <= 205 ? 1.3 : -0.00048;
        const double current = index == 200 ? 8.0 : (index == 205 ? -9.0 : -0.009);
        const double optical = index >= 201 && index <= 205
            ? 5000.0
            : 350.0 + 20.0 * std::sin(index * 1.5);
        recorder.appendIv(time, voltage, current);
        appendFilteredRows(&recorder, filter.appendIv(time, voltage, current));
        recorder.appendLight(time, optical);
        appendFilteredRows(&recorder, filter.appendLight(time, optical));
        if ((index % 100) == 0 && !recorder.flush(&errorMessage)) return 1;
    }
    appendFilteredRows(&recorder, filter.flush());
    if (!recorder.flush(&errorMessage) || !recorder.stop(&errorMessage)) {
        std::cerr << "FAILED: " << errorMessage.toStdString() << '\n';
        return 1;
    }

    QFile output(filePath);
    if (!output.open(QIODevice::ReadOnly | QIODevice::Text)) return 1;
    QTextStream stream(&output);
    const QStringList lines = stream.readAll().trimmed().split('\n');
    if (lines.size() != sampleCount + 1) {
        std::cerr << "FAILED: row count " << lines.size() - 1
            << " != " << sampleCount << '\n';
        return 1;
    }
    for (int line = 1; line < lines.size(); ++line) {
        const QStringList columns = lines.at(line).split(',');
        if (columns.size() != 9 || columns.at(4).isEmpty()
            || columns.at(5).isEmpty() || columns.at(6).isEmpty()) {
            std::cerr << "FAILED: filtered columns missing at row " << line << '\n';
            return 1;
        }
    }
    if (lines.last().split(',').at(7) != "1") {
        std::cerr << "FAILED: flushed tail was not marked valid\n";
        return 1;
    }

    std::cout << "Realtime CSV pipeline tests passed.\n";
    return 0;
}
