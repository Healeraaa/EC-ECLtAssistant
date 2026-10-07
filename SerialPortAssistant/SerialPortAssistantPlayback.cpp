#include "SerialPortAssistant.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSignalBlocker>
#include <algorithm>

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

    if (m_playbackRowIndex >= m_playbackData.rows().size()) seekCSVPlayback(0);
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
    m_plotBuffer.clear();
    globalSamplePairCount = 0;
    globalOpticalSampleCount = 0;
    ivSamplingRate = 0;
    lightSamplingRate = 0;
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
    if (row.hasVoltage) m_plotBuffer.append(0, row.timeSeconds, row.voltage);
    if (row.hasCurrent) m_plotBuffer.append(1, row.timeSeconds, row.current);
    if (row.hasOptical) m_plotBuffer.append(2, row.timeSeconds, row.optical);
    if (row.hasVoltage || row.hasCurrent) ++globalSamplePairCount;
    if (row.hasOptical) ++globalOpticalSampleCount;
}
