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

    std::cout << "CsvPlaybackData tests passed.\n";
    return 0;
}
