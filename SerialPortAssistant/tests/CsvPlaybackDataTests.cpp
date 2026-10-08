#include "../CsvPlaybackData.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>
#include <iostream>

int wmain(int argc, wchar_t* argv[])
{
    if (argc > 1) {
        for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
            CsvPlaybackData inspectedData;
            QString inspectedError;
            const QString inspectedPath = QString::fromWCharArray(argv[argumentIndex]);
            if (!inspectedData.load(inspectedPath, &inspectedError)) {
                std::cerr << "FAILED: " << inspectedError.toStdString() << '\n';
                return 1;
            }

            int validCount = 0;
            double areaSum = 0.0;
            double squaredDeviationSum = 0.0;
            QVector<double> validAreas;
            for (const PulseAreaMeasurement& pulse : inspectedData.pulses()) {
                if (!pulse.valid) continue;
                validAreas.append(pulse.filteredArea);
                areaSum += pulse.filteredArea;
                ++validCount;
            }
            const double mean = validCount > 0 ? areaSum / validCount : 0.0;
            for (const double area : validAreas) {
                const double deviation = area - mean;
                squaredDeviationSum += deviation * deviation;
            }
            const double standardDeviation = validCount > 1
                ? std::sqrt(squaredDeviationSum / (validCount - 1))
                : 0.0;
            const double coefficientOfVariation = std::abs(mean) > 1e-12
                ? standardDeviation / std::abs(mean) * 100.0
                : 0.0;
            const double latestArea = validAreas.isEmpty() ? 0.0 : validAreas.back();

            std::wcout << L"File: " << argv[argumentIndex] << L'\n';
            std::cout << "Rows=" << inspectedData.rows().size()
                      << ", Pulses=" << inspectedData.pulses().size()
                      << ", Valid=" << validCount
                      << ", Latest=" << latestArea
                      << ", Mean=" << mean
                      << ", CV=" << coefficientOfVariation << "%\n";
        }
        return 0;
    }

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
        || !data.rows().at(102).gpciEvent
        || data.pulses().size() != 1
        || !data.pulses().front().valid) {
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
