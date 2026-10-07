#ifndef EXPERIMENTPRESET_H
#define EXPERIMENTPRESET_H

#include <QString>
#include <array>

struct ExperimentPresetData
{
    int mode = 0;
    int channel = 0;
    int range = 0;
    std::array<std::array<double, 6>, 4> modeValues{};

    int displayPoints = 50000;
    double xMinimum = 0.0;
    double xMaximum = 100.0;
    double yMinimum = -2.0;
    double yMaximum = 2.0;
    double rightMinimum = -2.0;
    double rightMaximum = 2.0;
    bool autoScale = false;
    std::array<bool, 3> channelVisible{{ true, true, true }};
};

class ExperimentPresetStore
{
public:
    static bool save(
        const QString& filePath,
        const ExperimentPresetData& preset,
        QString* errorMessage = nullptr);
    static bool load(
        const QString& filePath,
        ExperimentPresetData* preset,
        QString* errorMessage = nullptr);
};

#endif // EXPERIMENTPRESET_H
