#include "SerialPortAssistant.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>

void SerialPortAssistant::savePreset()
{
    saveSettings();
    const QString presetDirectory = QDir(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath("EC-ECL/Presets");
    QDir().mkpath(presetDirectory);
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8("保存实验预设"),
        QDir(presetDirectory).filePath(Combo_Mode->currentText().section(' ', 0, 0) + ".json"),
        QString::fromUtf8("EC-ECL 预设 (*.json)"));
    if (filePath.isEmpty()) return;

    ExperimentPresetData preset;
    preset.mode = Combo_Mode->currentIndex();
    preset.channel = Combo_Configs[0]->currentIndex();
    preset.range = Combo_Range->currentIndex();
    preset.modeValues = m_modeValues;
    preset.displayPoints = Edit_XRange->text().toInt();
    preset.xMinimum = Edit_XMin->text().toDouble();
    preset.xMaximum = Edit_XMax->text().toDouble();
    preset.yMinimum = Edit_YMin->text().toDouble();
    preset.yMaximum = Edit_YMax->text().toDouble();
    preset.rightMinimum = Edit_YRightMin->text().toDouble();
    preset.rightMaximum = Edit_YRightMax->text().toDouble();
    preset.autoScale = CheckBox_AutoScale->isChecked();
    for (int channel = 0; channel < 3; ++channel) {
        preset.channelVisible[channel] = CheckBox_ChannelVisible[channel]->isChecked();
    }

    QString errorMessage;
    if (!ExperimentPresetStore::save(filePath, preset, &errorMessage)) {
        QMessageBox::warning(this, QString::fromUtf8("保存失败"), errorMessage);
        return;
    }
    SerialPort_ReceiveAear->appendPlainText("[Preset] Saved: " + QDir::toNativeSeparators(filePath));
}

void SerialPortAssistant::loadPreset()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("加载实验预设"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QString::fromUtf8("EC-ECL 预设 (*.json)"));
    if (filePath.isEmpty()) return;

    ExperimentPresetData preset;
    QString errorMessage;
    if (!ExperimentPresetStore::load(filePath, &preset, &errorMessage)) {
        QMessageBox::warning(this, QString::fromUtf8("加载失败"), errorMessage);
        return;
    }

    m_modeValues = preset.modeValues;
    Combo_Configs[0]->setCurrentIndex(qBound(0, preset.channel, Combo_Configs[0]->count() - 1));
    Combo_Range->setCurrentIndex(qBound(0, preset.range, Combo_Range->count() - 1));
    const int mode = qBound(0, preset.mode, Combo_Mode->count() - 1);
    m_currentMode = -1;
    Combo_Mode->setCurrentIndex(mode);
    updateChemLabels(mode);

    Edit_XRange->setText(QString::number(qBound(1, preset.displayPoints, 50000)));
    Edit_XMin->setText(QString::number(preset.xMinimum));
    Edit_XMax->setText(QString::number(preset.xMaximum));
    Edit_YMin->setText(QString::number(preset.yMinimum));
    Edit_YMax->setText(QString::number(preset.yMaximum));
    Edit_YRightMin->setText(QString::number(preset.rightMinimum));
    Edit_YRightMax->setText(QString::number(preset.rightMaximum));
    for (int channel = 0; channel < 3; ++channel) {
        CheckBox_ChannelVisible[channel]->setChecked(preset.channelVisible[channel]);
    }
    CheckBox_AutoScale->setChecked(preset.autoScale);
    if (!preset.autoScale) applyManualAxisRanges();
    saveSettings();
    SerialPort_ReceiveAear->appendPlainText("[Preset] Loaded: " + QDir::toNativeSeparators(filePath));
}
