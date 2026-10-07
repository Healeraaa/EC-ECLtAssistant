#include "SerialPortAssistant.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

QString formatRate(double bytesPerSecond)
{
    if (bytesPerSecond >= 1024.0 * 1024.0) {
        return QString::number(bytesPerSecond / (1024.0 * 1024.0), 'f', 2) + " MB/s";
    }
    if (bytesPerSecond >= 1024.0) {
        return QString::number(bytesPerSecond / 1024.0, 'f', 1) + " KB/s";
    }
    return QString::number(bytesPerSecond, 'f', 0) + " B/s";
}

void includeValue(double value, double* minimum, double* maximum, bool* hasValue)
{
    if (!std::isfinite(value)) return;
    if (!*hasValue) {
        *minimum = value;
        *maximum = value;
        *hasValue = true;
        return;
    }
    *minimum = std::min(*minimum, value);
    *maximum = std::max(*maximum, value);
}

void setPaddedRange(QValueAxis* axis, double minimum, double maximum)
{
    if (minimum == maximum) {
        const double padding = std::max(1.0, std::fabs(minimum) * 0.05);
        axis->setRange(minimum - padding, maximum + padding);
        return;
    }
    const double padding = (maximum - minimum) * 0.05;
    axis->setRange(minimum - padding, maximum + padding);
}

}

void SerialPortAssistant::ensureSeriesCreated()
{
    while (seriesList.size() < 3) {
        QLineSeries* series = new QLineSeries();
        const int seriesIndex = seriesList.size();
        const QColor colors[] = { QColor("#00e5ff"), QColor("#ff6bc1"), QColor("#00ff88") };
        const char* names[] = { "Voltage", "Current", "ECL" };

        QPen pen(colors[seriesIndex]);
        pen.setWidth(2);
        series->setPen(pen);
        series->setName(names[seriesIndex]);
        chartView->chart()->addSeries(series);
        series->attachAxis(axisX);
        series->attachAxis(seriesIndex == 2 ? axisYRight : axisY);
        seriesList.append(series);
    }
    updateSeriesVisibility();
}

void SerialPortAssistant::applyManualAxisRanges()
{
    CheckBox_AutoScale->setChecked(false);

    const double xMinimum = Edit_XMin->text().toDouble();
    const double xMaximum = Edit_XMax->text().toDouble();
    const double yMinimum = Edit_YMin->text().toDouble();
    const double yMaximum = Edit_YMax->text().toDouble();
    const double rightMinimum = Edit_YRightMin->text().toDouble();
    const double rightMaximum = Edit_YRightMax->text().toDouble();

    if (xMinimum < xMaximum) axisX->setRange(xMinimum, xMaximum);
    if (yMinimum < yMaximum) axisY->setRange(yMinimum, yMaximum);
    if (rightMinimum < rightMaximum) axisYRight->setRange(rightMinimum, rightMaximum);
}

void SerialPortAssistant::fitChartToData()
{
    double xMinimum = 0.0;
    double xMaximum = 0.0;
    double leftMinimum = 0.0;
    double leftMaximum = 0.0;
    double rightMinimum = 0.0;
    double rightMaximum = 0.0;
    bool hasX = false;
    bool hasLeft = false;
    bool hasRight = false;

    for (int channel = 0; channel < 3; ++channel) {
        if (!CheckBox_ChannelVisible[channel]->isChecked()) continue;
        for (const QPointF& point : m_plotData[channel]) {
            includeValue(point.x(), &xMinimum, &xMaximum, &hasX);
            if (channel == 2) {
                includeValue(point.y(), &rightMinimum, &rightMaximum, &hasRight);
            }
            else {
                includeValue(point.y(), &leftMinimum, &leftMaximum, &hasLeft);
            }
        }
    }

    if (hasX) setPaddedRange(axisX, xMinimum, xMaximum);
    if (hasLeft) setPaddedRange(axisY, leftMinimum, leftMaximum);
    if (hasRight) setPaddedRange(axisYRight, rightMinimum, rightMaximum);
}

void SerialPortAssistant::updateSeriesVisibility()
{
    for (int channel = 0; channel < seriesList.size() && channel < 3; ++channel) {
        seriesList[channel]->setVisible(CheckBox_ChannelVisible[channel]->isChecked());
    }
}

void SerialPortAssistant::resetStatistics()
{
    for (ChannelStatistics& statistics : m_channelStatistics) {
        statistics = ChannelStatistics{};
    }
    if (Label_PeakStatus) Label_PeakStatus->setText("No data");
}

void SerialPortAssistant::updateChannelStatistics(int channel, double value)
{
    if (channel < 0 || channel >= 3 || !std::isfinite(value)) return;
    ChannelStatistics& statistics = m_channelStatistics[channel];
    if (!statistics.hasValue) {
        statistics.minimum = value;
        statistics.maximum = value;
        statistics.hasValue = true;
    }
    else {
        statistics.minimum = std::min(statistics.minimum, value);
        statistics.maximum = std::max(statistics.maximum, value);
    }
    ++statistics.count;
}

void SerialPortAssistant::updateStatusPanel()
{
    const qint64 elapsedMilliseconds = std::max<qint64>(1, m_dataRateTimer.restart());
    const quint64 byteDelta = m_receivedBytes - m_lastStatusBytes;
    const double bytesPerSecond = byteDelta * 1000.0 / elapsedMilliseconds;
    m_lastStatusBytes = m_receivedBytes;

    const ProtocolParserStats& parserStats = m_frameParser.stats();
    const quint64 frameDelta = parserStats.validFrames - m_lastStatusFrames;
    const double framesPerSecond = frameDelta * 1000.0 / elapsedMilliseconds;
    m_lastStatusFrames = parserStats.validFrames;

    if (serialPort->isOpen()) {
        Label_ConnectionStatus->setText(
            QString("%1 @ %2").arg(serialPort->portName()).arg(serialPort->baudRate()));
        Label_DataRate->setText(
            QString("%1 | %2 fps").arg(formatRate(bytesPerSecond)).arg(framesPerSecond, 0, 'f', 1));
    }
    else if (m_playbackActive) {
        Label_ConnectionStatus->setText("CSV Playback");
        Label_DataRate->setText("Playback " + Combo_PlaybackSpeed->currentText());
    }
    else {
        Label_ConnectionStatus->setText("Disconnected");
        Label_DataRate->setText("0 B/s");
    }

    Label_FrameStatus->setText(
        QString("Frames %1 | CRC %2 | Invalid %3 | Dropped %4 B")
            .arg(parserStats.validFrames)
            .arg(parserStats.crcErrors)
            .arg(parserStats.lengthErrors + parserStats.unknownFrames)
            .arg(parserStats.discardedBytes));
    Label_SampleStatus->setText(
        QString("IV %1 @ %2 Hz | ECL %3 @ %4 Hz")
            .arg(globalSamplePairCount)
            .arg(ivSamplingRate)
            .arg(globalOpticalSampleCount)
            .arg(lightSamplingRate));

    QStringList peakParts;
    if (m_channelStatistics[0].hasValue) {
        peakParts << QString("V [%1, %2]")
            .arg(m_channelStatistics[0].minimum, 0, 'g', 6)
            .arg(m_channelStatistics[0].maximum, 0, 'g', 6);
    }
    if (m_channelStatistics[1].hasValue) {
        peakParts << QString("I [%1, %2]")
            .arg(m_channelStatistics[1].minimum, 0, 'g', 6)
            .arg(m_channelStatistics[1].maximum, 0, 'g', 6);
    }
    if (m_channelStatistics[2].hasValue) {
        peakParts << QString("ECL max %1").arg(m_channelStatistics[2].maximum, 0, 'g', 6);
    }
    Label_PeakStatus->setText(peakParts.isEmpty() ? "No data" : peakParts.join(" | "));

    if (m_csvRecorder.isRecording()) {
        const quint64 rowCount = m_csvRecorder.rowsWritten()
            + static_cast<quint64>(m_csvRecorder.pendingRows());
        Label_CSVStatus->setText(
            QString::fromUtf8("正在记录 %1 行：%2")
                .arg(rowCount)
                .arg(QFileInfo(m_csvRecorder.filePath()).fileName()));
    }
}

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

    QJsonObject root;
    root["version"] = 1;
    root["mode"] = Combo_Mode->currentIndex();
    root["channel"] = Combo_Configs[0]->currentIndex();
    root["range"] = Combo_Range->currentIndex();

    QJsonArray modes;
    for (int mode = 0; mode < 4; ++mode) {
        QJsonArray parameters;
        for (double value : m_modeValues[mode]) parameters.append(value);
        modes.append(parameters);
    }
    root["modes"] = modes;

    QJsonObject plot;
    plot["displayPoints"] = Edit_XRange->text().toInt();
    plot["xMin"] = Edit_XMin->text().toDouble();
    plot["xMax"] = Edit_XMax->text().toDouble();
    plot["yMin"] = Edit_YMin->text().toDouble();
    plot["yMax"] = Edit_YMax->text().toDouble();
    plot["yRightMin"] = Edit_YRightMin->text().toDouble();
    plot["yRightMax"] = Edit_YRightMax->text().toDouble();
    plot["autoScale"] = CheckBox_AutoScale->isChecked();
    QJsonArray visibility;
    for (QCheckBox* checkBox : CheckBox_ChannelVisible) visibility.append(checkBox->isChecked());
    plot["visibility"] = visibility;
    root["plot"] = plot;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        QMessageBox::warning(this, QString::fromUtf8("保存失败"), file.errorString());
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

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QString::fromUtf8("加载失败"), file.errorString());
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(this, QString::fromUtf8("加载失败"), QString::fromUtf8("预设 JSON 格式无效。"));
        return;
    }

    const QJsonObject root = document.object();
    const QJsonArray modes = root.value("modes").toArray();
    if (root.value("version").toInt() != 1 || modes.size() != 4) {
        QMessageBox::warning(this, QString::fromUtf8("加载失败"), QString::fromUtf8("不支持的预设版本或参数数量。"));
        return;
    }
    for (int mode = 0; mode < 4; ++mode) {
        const QJsonArray parameters = modes.at(mode).toArray();
        if (parameters.size() != 6) {
            QMessageBox::warning(this, QString::fromUtf8("加载失败"), QString::fromUtf8("预设参数数量错误。"));
            return;
        }
        for (int parameter = 0; parameter < 6; ++parameter) {
            m_modeValues[mode][parameter] = parameters.at(parameter).toDouble();
        }
    }

    Combo_Configs[0]->setCurrentIndex(qBound(0, root.value("channel").toInt(), Combo_Configs[0]->count() - 1));
    Combo_Range->setCurrentIndex(qBound(0, root.value("range").toInt(), Combo_Range->count() - 1));
    const int mode = qBound(0, root.value("mode").toInt(), Combo_Mode->count() - 1);
    m_currentMode = -1;
    Combo_Mode->setCurrentIndex(mode);
    updateChemLabels(mode);

    const QJsonObject plot = root.value("plot").toObject();
    Edit_XRange->setText(QString::number(qBound(1, plot.value("displayPoints").toInt(50000), 50000)));
    Edit_XMin->setText(QString::number(plot.value("xMin").toDouble(0.0)));
    Edit_XMax->setText(QString::number(plot.value("xMax").toDouble(100.0)));
    Edit_YMin->setText(QString::number(plot.value("yMin").toDouble(-2.0)));
    Edit_YMax->setText(QString::number(plot.value("yMax").toDouble(2.0)));
    Edit_YRightMin->setText(QString::number(plot.value("yRightMin").toDouble(-2.0)));
    Edit_YRightMax->setText(QString::number(plot.value("yRightMax").toDouble(2.0)));
    const QJsonArray visibility = plot.value("visibility").toArray();
    for (int channel = 0; channel < 3 && channel < visibility.size(); ++channel) {
        CheckBox_ChannelVisible[channel]->setChecked(visibility.at(channel).toBool(true));
    }
    CheckBox_AutoScale->setChecked(plot.value("autoScale").toBool(false));
    if (!CheckBox_AutoScale->isChecked()) applyManualAxisRanges();
    saveSettings();
    SerialPort_ReceiveAear->appendPlainText("[Preset] Loaded: " + QDir::toNativeSeparators(filePath));
}

void SerialPortAssistant::loadCSVForPlayback()
{
    if (serialPort->isOpen()) {
        QMessageBox::information(this, QString::fromUtf8("CSV 回放"), QString::fromUtf8("请先断开串口再加载 CSV。"));
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("加载 CSV 回放文件"),
        Edit_CSVDirectory->text(),
        QString::fromUtf8("CSV 文件 (*.csv)"));
    if (filePath.isEmpty()) return;

    stopCSVPlayback();
    QString errorMessage;
    if (!m_playbackData.load(filePath, &errorMessage)) {
        QMessageBox::warning(this, QString::fromUtf8("CSV 加载失败"), errorMessage);
        return;
    }

    m_frameParser.clear();
    m_lastStatusFrames = 0;

    Label_PlaybackFile->setText(
        QString("%1 | %2 rows | %3 s")
            .arg(QFileInfo(filePath).fileName())
            .arg(m_playbackData.rows().size())
            .arg(m_playbackData.duration(), 0, 'f', 3));
    Btn_PlayPauseCSV->setEnabled(true);
    Slider_Playback->setEnabled(true);
    seekCSVPlayback(0);
    SerialPort_ReceiveAear->appendPlainText("[Playback] Loaded: " + QDir::toNativeSeparators(filePath));
}

void SerialPortAssistant::toggleCSVPlayback()
{
    if (m_playbackData.isEmpty() || serialPort->isOpen()) return;
    if (m_playbackActive) {
        stopCSVPlayback();
        return;
    }

    if (m_playbackRowIndex >= m_playbackData.rows().size()) {
        seekCSVPlayback(0);
    }
    m_playbackActive = true;
    Btn_PlayPauseCSV->setText("PAUSE");
    m_playbackTimer->start();
    updateStatusPanel();
}

void SerialPortAssistant::advanceCSVPlayback()
{
    if (!m_playbackActive || m_playbackData.isEmpty()) return;

    const double speed = Combo_PlaybackSpeed->currentData().toDouble();
    m_playbackTimeSeconds += m_playbackTimer->interval() / 1000.0 * speed;
    const QVector<CsvPlaybackRow>& rows = m_playbackData.rows();
    while (m_playbackRowIndex < rows.size()
           && rows.at(m_playbackRowIndex).timeSeconds <= m_playbackTimeSeconds) {
        appendPlaybackRow(rows.at(m_playbackRowIndex));
        ++m_playbackRowIndex;
    }

    if (CheckBox_EnablePlot->isChecked() && !CheckBox_PausePlot->isChecked()) {
        updatePlotSeries();
    }
    else if (CheckBox_EnablePlot->isChecked()) {
        trimPlotBuffers();
    }

    const int sliderValue = rows.isEmpty()
        ? 0
        : static_cast<int>((static_cast<qint64>(m_playbackRowIndex) * 1000) / rows.size());
    const QSignalBlocker blocker(Slider_Playback);
    Slider_Playback->setValue(qBound(0, sliderValue, 1000));

    if (m_playbackRowIndex >= rows.size()) {
        Slider_Playback->setValue(1000);
        stopCSVPlayback();
    }
}

void SerialPortAssistant::seekCSVPlayback(int sliderValue)
{
    if (m_playbackData.isEmpty()) return;
    stopCSVPlayback();

    const QVector<CsvPlaybackRow>& rows = m_playbackData.rows();
    const int targetIndex = qBound(
        0,
        static_cast<int>((static_cast<qint64>(rows.size() - 1) * sliderValue) / 1000),
        rows.size() - 1);
    int displayPointCount = Edit_XRange->text().toInt();
    if (displayPointCount <= 0) displayPointCount = 100;
    const int firstVisibleIndex = std::max(0, targetIndex - displayPointCount + 1);

    ensureSeriesCreated();
    for (QLineSeries* series : seriesList) series->clear();
    for (QVector<QPointF>& points : m_plotData) points.clear();
    globalSamplePairCount = 0;
    globalOpticalSampleCount = 0;
    ivSamplingRate = 0;
    lightSamplingRate = 0;
    resetStatistics();
    for (int index = firstVisibleIndex; index <= targetIndex; ++index) {
        appendPlaybackRow(rows.at(index));
    }

    m_playbackRowIndex = targetIndex + 1;
    m_playbackTimeSeconds = rows.at(targetIndex).timeSeconds;
    updatePlotSeries();
    updateStatusPanel();
}

void SerialPortAssistant::stopCSVPlayback()
{
    m_playbackTimer->stop();
    m_playbackActive = false;
    if (Btn_PlayPauseCSV) Btn_PlayPauseCSV->setText("PLAY");
}

void SerialPortAssistant::appendPlaybackRow(const CsvPlaybackRow& row)
{
    ensureSeriesCreated();
    if (row.hasVoltage) {
        m_plotData[0].append(QPointF(row.timeSeconds, row.voltage));
        updateChannelStatistics(0, row.voltage);
    }
    if (row.hasCurrent) {
        m_plotData[1].append(QPointF(row.timeSeconds, row.current));
        updateChannelStatistics(1, row.current);
    }
    if (row.hasOptical) {
        m_plotData[2].append(QPointF(row.timeSeconds, row.optical));
        updateChannelStatistics(2, row.optical);
    }
    if (row.hasVoltage || row.hasCurrent) ++globalSamplePairCount;
    if (row.hasOptical) ++globalOpticalSampleCount;
}
