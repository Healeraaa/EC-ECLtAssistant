#include "../CsvPlaybackData.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <iostream>

int main()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    const QString validPath = temporaryDirectory.filePath("valid.csv");
    QFile validFile(validPath);
    if (!validFile.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream validStream(&validFile);
    validStream << "Time(s),Voltage(V),Current(A),OpticalSignal\n"
                << "0.000000,1.0,2.0,10.0\n"
                << "0.010000,3.0,4.0,\n"
                << "0.020000,,,20.0\n";
    validFile.close();

    CsvPlaybackData data;
    QString errorMessage;
    if (!data.load(validPath, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << '\n';
        return 1;
    }
    if (data.rows().size() != 3
        || !data.rows().at(0).hasVoltage
        || !data.rows().at(0).hasOptical
        || data.rows().at(1).hasOptical
        || data.duration() != 0.02) {
        std::cerr << "FAILED: parsed playback content mismatch\n";
        return 1;
    }

    const CsvPlaybackLoadResult loadResult = loadCsvPlaybackFile(validPath);
    if (!loadResult.succeeded || loadResult.data.rows().size() != 3) {
        std::cerr << "FAILED: worker-friendly load result mismatch\n";
        return 1;
    }

    const QString invalidPath = temporaryDirectory.filePath("invalid.csv");
    QFile invalidFile(invalidPath);
    if (!invalidFile.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream invalidStream(&invalidFile);
    invalidStream << "Time(s),Voltage(V),Current(A),OpticalSignal\n"
                  << "1.0,1.0,,\n"
                  << "0.5,2.0,,\n";
    invalidFile.close();

    if (data.load(invalidPath, &errorMessage)) {
        std::cerr << "FAILED: decreasing timestamps must be rejected\n";
        return 1;
    }

    const QString rawGpciPath = temporaryDirectory.filePath("raw_gpci.csv");
    QFile rawGpciFile(rawGpciPath);
    if (!rawGpciFile.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream rawGpciStream(&rawGpciFile);
    rawGpciStream << "Time(s),Voltage(V),Current(A),OpticalSignal\n";
    for (int index = 0; index < 200; ++index) {
        rawGpciStream << QString::number(index * 0.01, 'f', 6) << ','
                      << (index >= 100 && index <= 105 ? "1.3" : "-0.00048") << ','
                      << (index == 100 ? "8.0" : (index == 105 ? "-9.0" : "-0.009")) << ','
                      << (index >= 101 && index <= 105 ? "5000" : "350") << '\n';
    }
    rawGpciFile.close();
    if (!data.load(rawGpciPath, &errorMessage)
        || !data.hasFilteredData()
        || !data.rows().at(102).hasFiltered
        || !data.rows().at(102).gpciEvent) {
        std::cerr << "FAILED: raw 100 Hz GPCI file was not auto-filtered\n";
        return 1;
    }

    const QString filteredPath = temporaryDirectory.filePath("filtered.csv");
    QFile filteredFile(filteredPath);
    if (!filteredFile.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream filteredStream(&filteredFile);
    filteredStream << "Time(s),Voltage(V),Current(A),OpticalSignal,"
        "VoltageFiltered(V),CurrentFiltered(A),OpticalSignalFiltered,FilterValid,GPCIEvent\n"
        "0.000000,1,2,3,1.1,2.1,3.1,1,1\n";
    filteredFile.close();
    if (!data.load(filteredPath, &errorMessage)
        || !data.hasFilteredData()
        || data.rows().front().filteredOptical != 3.1
        || !data.rows().front().filterValid
        || !data.rows().front().gpciEvent) {
        std::cerr << "FAILED: stored filtered columns were not loaded\n";
        return 1;
    }

    std::cout << "CsvPlaybackData tests passed.\n";
    return 0;
}
