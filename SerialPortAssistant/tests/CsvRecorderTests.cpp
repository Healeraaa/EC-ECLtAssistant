#include "../CsvRecorder.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <iostream>

int main()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "FAILED: cannot create temporary directory\n";
        return 1;
    }

    const QString filePath = temporaryDirectory.filePath("recording.csv");
    CsvRecorder recorder;
    QString errorMessage;
    if (!recorder.start(filePath, &errorMessage)) {
        std::cerr << "FAILED: " << errorMessage.toStdString() << '\n';
        return 1;
    }

    recorder.appendIv(0.0, 1.0, 2.0);
    recorder.appendIv(0.01, 3.0, 4.0);
    recorder.appendLight(0.0, 10.0);
    recorder.appendLight(0.01, 20.0);
    if (!recorder.flush(&errorMessage) || !recorder.stop(&errorMessage)) {
        std::cerr << "FAILED: " << errorMessage.toStdString() << '\n';
        return 1;
    }

    QFile output(filePath);
    if (!output.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "FAILED: cannot read output\n";
        return 1;
    }
    QTextStream stream(&output);
    const QStringList lines = stream.readAll().trimmed().split('\n');
    if (lines.size() != 3
        || lines.at(1) != "0.000000,1.000000,2.000000,10.000000"
        || lines.at(2) != "0.010000,3.000000,4.000000,20.000000") {
        std::cerr << "FAILED: aligned CSV content mismatch\n";
        return 1;
    }

    const QString filteredPath = temporaryDirectory.filePath("filtered.csv");
    CsvRecorder filteredRecorder;
    if (!filteredRecorder.start(filteredPath, true, &errorMessage)) {
        std::cerr << "FAILED: " << errorMessage.toStdString() << '\n';
        return 1;
    }
    filteredRecorder.appendIv(0.0, 1.0, 2.0);
    filteredRecorder.appendLight(0.0, 10.0);
    filteredRecorder.appendFiltered(0.0, 1.1, 2.1, 9.5, false, true);
    filteredRecorder.appendIv(0.01, 3.0, 4.0);
    filteredRecorder.appendLight(0.01, 20.0);
    filteredRecorder.appendFiltered(0.01, 3.1, 4.1, 19.5, true, false);
    if (!filteredRecorder.flush(&errorMessage)
        || !filteredRecorder.stop(&errorMessage)) {
        std::cerr << "FAILED: " << errorMessage.toStdString() << '\n';
        return 1;
    }

    QFile filteredOutput(filteredPath);
    if (!filteredOutput.open(QIODevice::ReadOnly | QIODevice::Text)) return 1;
    QTextStream filteredStream(&filteredOutput);
    const QStringList filteredLines = filteredStream.readAll().trimmed().split('\n');
    if (filteredLines.size() != 3
        || filteredLines.at(0) != "Time(s),Voltage(V),Current(A),OpticalSignal,VoltageFiltered(V),CurrentFiltered(A),OpticalSignalFiltered,FilterValid,GPCIEvent"
        || filteredLines.at(1) != "0.000000,1.000000,2.000000,10.000000,1.100000000,2.100000000,9.500000000,0,1"
        || filteredLines.at(2) != "0.010000,3.000000,4.000000,20.000000,3.100000000,4.100000000,19.500000000,1,0") {
        std::cerr << "FAILED: filtered CSV content mismatch\n";
        return 1;
    }

    std::cout << "CsvRecorder tests passed.\n";
    return 0;
}
