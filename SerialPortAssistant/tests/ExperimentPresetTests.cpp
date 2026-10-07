#include "../ExperimentPreset.h"

#include <QFile>
#include <QTemporaryDir>
#include <iostream>

int main()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    ExperimentPresetData expected;
    expected.mode = 2;
    expected.channel = 1;
    expected.range = 3;
    expected.modeValues[2][4] = -123.5;
    expected.displayPoints = 2048;
    expected.xMaximum = 42.0;
    expected.autoScale = true;
    expected.channelVisible = {{ true, false, true }};

    const QString presetPath = temporaryDirectory.filePath("preset.json");
    QString errorMessage;
    if (!ExperimentPresetStore::save(presetPath, expected, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << '\n';
        return 1;
    }

    ExperimentPresetData actual;
    if (!ExperimentPresetStore::load(presetPath, &actual, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << '\n';
        return 1;
    }
    if (actual.mode != expected.mode
        || actual.channel != expected.channel
        || actual.range != expected.range
        || actual.modeValues[2][4] != expected.modeValues[2][4]
        || actual.displayPoints != expected.displayPoints
        || actual.xMaximum != expected.xMaximum
        || actual.autoScale != expected.autoScale
        || actual.channelVisible != expected.channelVisible) {
        std::cerr << "FAILED: preset round trip mismatch\n";
        return 1;
    }

    const QString invalidPath = temporaryDirectory.filePath("invalid.json");
    QFile invalidFile(invalidPath);
    if (!invalidFile.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    invalidFile.write("{\"version\":99,\"modes\":[]}");
    invalidFile.close();
    if (ExperimentPresetStore::load(invalidPath, &actual, &errorMessage)) {
        std::cerr << "FAILED: unsupported preset version was accepted\n";
        return 1;
    }

    if (!invalidFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return 1;
    invalidFile.write(
        "{\"version\":1,\"modes\":[[\"bad\",0,0,0,0,0],"
        "[0,0,0,0,0,0],[0,0,0,0,0,0],[0,0,0,0,0,0]]}");
    invalidFile.close();
    if (ExperimentPresetStore::load(invalidPath, &actual, &errorMessage)) {
        std::cerr << "FAILED: non-numeric preset parameter was accepted\n";
        return 1;
    }

    std::cout << "ExperimentPreset tests passed.\n";
    return 0;
}
