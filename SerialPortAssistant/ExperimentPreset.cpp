#include "ExperimentPreset.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>

namespace {

constexpr int kPresetVersion = 1;

bool readFiniteNumber(
    const QJsonObject& object,
    const char* key,
    double defaultValue,
    double* result)
{
    const QJsonValue value = object.value(key);
    if (!value.isUndefined() && !value.isDouble()) return false;
    const double number = value.isUndefined() ? defaultValue : value.toDouble(defaultValue);
    if (!std::isfinite(number)) return false;
    *result = number;
    return true;
}

}

bool ExperimentPresetStore::save(
    const QString& filePath,
    const ExperimentPresetData& preset,
    QString* errorMessage)
{
    QJsonObject root;
    root["version"] = kPresetVersion;
    root["mode"] = preset.mode;
    root["channel"] = preset.channel;
    root["range"] = preset.range;

    QJsonArray modes;
    for (const std::array<double, 6>& modeValues : preset.modeValues) {
        QJsonArray parameters;
        for (double value : modeValues) parameters.append(value);
        modes.append(parameters);
    }
    root["modes"] = modes;

    QJsonObject plot;
    plot["displayPoints"] = preset.displayPoints;
    plot["xMin"] = preset.xMinimum;
    plot["xMax"] = preset.xMaximum;
    plot["yMin"] = preset.yMinimum;
    plot["yMax"] = preset.yMaximum;
    plot["yRightMin"] = preset.rightMinimum;
    plot["yRightMax"] = preset.rightMaximum;
    plot["autoScale"] = preset.autoScale;
    QJsonArray visibility;
    for (bool visible : preset.channelVisible) visibility.append(visible);
    plot["visibility"] = visibility;
    root["plot"] = plot;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

bool ExperimentPresetStore::load(
    const QString& filePath,
    ExperimentPresetData* preset,
    QString* errorMessage)
{
    if (!preset) {
        if (errorMessage) *errorMessage = QString::fromUtf8("预设输出对象为空。");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) *errorMessage = QString::fromUtf8("预设 JSON 格式无效。");
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonArray modes = root.value("modes").toArray();
    if (root.value("version").toInt() != kPresetVersion || modes.size() != 4) {
        if (errorMessage) *errorMessage = QString::fromUtf8("不支持的预设版本或参数数量。");
        return false;
    }

    ExperimentPresetData loaded;
    loaded.mode = root.value("mode").toInt();
    loaded.channel = root.value("channel").toInt();
    loaded.range = root.value("range").toInt();
    for (int mode = 0; mode < 4; ++mode) {
        const QJsonArray parameters = modes.at(mode).toArray();
        if (parameters.size() != 6) {
            if (errorMessage) *errorMessage = QString::fromUtf8("预设参数数量错误。");
            return false;
        }
        for (int parameter = 0; parameter < 6; ++parameter) {
            const QJsonValue parameterValue = parameters.at(parameter);
            if (!parameterValue.isDouble()) {
                if (errorMessage) *errorMessage = QString::fromUtf8("预设中包含非数值参数。");
                return false;
            }
            const double value = parameterValue.toDouble();
            if (!std::isfinite(value)) {
                if (errorMessage) *errorMessage = QString::fromUtf8("预设中包含无效数值。");
                return false;
            }
            loaded.modeValues[mode][parameter] = value;
        }
    }

    const QJsonObject plot = root.value("plot").toObject();
    loaded.displayPoints = plot.value("displayPoints").toInt(50000);
    if (!readFiniteNumber(plot, "xMin", 0.0, &loaded.xMinimum)
        || !readFiniteNumber(plot, "xMax", 100.0, &loaded.xMaximum)
        || !readFiniteNumber(plot, "yMin", -2.0, &loaded.yMinimum)
        || !readFiniteNumber(plot, "yMax", 2.0, &loaded.yMaximum)
        || !readFiniteNumber(plot, "yRightMin", -2.0, &loaded.rightMinimum)
        || !readFiniteNumber(plot, "yRightMax", 2.0, &loaded.rightMaximum)) {
        if (errorMessage) *errorMessage = QString::fromUtf8("预设坐标轴范围无效。");
        return false;
    }
    loaded.autoScale = plot.value("autoScale").toBool(false);
    const QJsonArray visibility = plot.value("visibility").toArray();
    for (int channel = 0; channel < 3 && channel < visibility.size(); ++channel) {
        loaded.channelVisible[channel] = visibility.at(channel).toBool(true);
    }

    *preset = loaded;
    return true;
}
