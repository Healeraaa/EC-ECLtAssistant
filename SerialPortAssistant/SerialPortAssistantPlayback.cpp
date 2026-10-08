#include "SerialPortAssistant.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QtConcurrent/QtConcurrentRun>
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

    openCSVForPlayback(filePath);
}

bool SerialPortAssistant::openCSVForPlayback(const QString& filePath)
{
    if (serialPort->isOpen() || filePath.isEmpty() || m_csvLoadWatcher->isRunning()) return false;

    stopCSVPlayback();
    m_pendingCsvPath = filePath;
    Btn_LoadCSV->setEnabled(false);
    Btn_OpenRecentCSV->setEnabled(false);
    Btn_PlayPauseCSV->setEnabled(false);
    Slider_Playback->setEnabled(false);
    Label_PlaybackFile->setText(
        QString::fromUtf8("正在后台加载：%1").arg(QFileInfo(filePath).fileName()));
    m_csvLoadWatcher->setFuture(QtConcurrent::run(loadCsvPlaybackFile, filePath));
    return true;
}

void SerialPortAssistant::finishCSVPlaybackLoad()
{
    Btn_LoadCSV->setEnabled(!serialPort->isOpen());
    Btn_OpenRecentCSV->setEnabled(!serialPort->isOpen() && !m_recentCsvFiles.isEmpty());
    const CsvPlaybackLoadResult result = m_csvLoadWatcher->result();
    if (!result.succeeded) {
        Btn_PlayPauseCSV->setEnabled(!serialPort->isOpen() && !m_playbackData.isEmpty());
        Slider_Playback->setEnabled(!serialPort->isOpen() && !m_playbackData.isEmpty());
        Label_PlaybackFile->setText(m_playbackData.isEmpty()
            ? QString::fromUtf8("没有已加载的 CSV")
            : QString::fromUtf8("保留上一次成功加载的 CSV"));
        QMessageBox::warning(this, QString::fromUtf8("CSV 加载失败"), result.errorMessage);
        m_pendingCsvPath.clear();
        return;
    }

    m_playbackData = result.data;

    m_frameParser.clear();
    m_lastStatusFrames = 0;
    Label_PlaybackFile->setText(
        QString::fromUtf8("%1 | %2 行 | %3 s | %4")
            .arg(QFileInfo(m_pendingCsvPath).fileName())
            .arg(m_playbackData.rows().size())
            .arg(m_playbackData.duration(), 0, 'f', 3)
            .arg(m_playbackData.hasFilteredData()
                ? QString::fromUtf8("滤波可用")
                : QString::fromUtf8("仅原始数据")));
    Btn_PlayPauseCSV->setEnabled(true);
    Slider_Playback->setEnabled(true);
    seekCSVPlayback(0);
    addRecentCSVFile(m_pendingCsvPath);
    SerialPort_ReceiveAear->appendPlainText(
        "[Playback] Loaded: " + QDir::toNativeSeparators(m_pendingCsvPath));
    m_pendingCsvPath.clear();
}

void SerialPortAssistant::openRecentCSV()
{
    if (Combo_RecentCSV->currentIndex() < 0) return;
    const QString filePath = Combo_RecentCSV->currentData().toString();
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, QString::fromUtf8("文件不存在"), QString::fromUtf8("最近使用的 CSV 文件已被移动或删除。"));
        m_recentCsvFiles.removeAll(filePath);
        updateRecentCSVList();
        saveSettings();
        return;
    }
    openCSVForPlayback(filePath);
}

void SerialPortAssistant::addRecentCSVFile(const QString& filePath)
{
    const QString normalizedPath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    for (int index = m_recentCsvFiles.size() - 1; index >= 0; --index) {
        if (QString::compare(m_recentCsvFiles.at(index), normalizedPath, Qt::CaseInsensitive) == 0) {
            m_recentCsvFiles.removeAt(index);
        }
    }
    m_recentCsvFiles.prepend(normalizedPath);
    while (m_recentCsvFiles.size() > 8) m_recentCsvFiles.removeLast();
    updateRecentCSVList();
    saveSettings();
}

void SerialPortAssistant::updateRecentCSVList()
{
    Combo_RecentCSV->clear();
    for (const QString& filePath : m_recentCsvFiles) {
        const QFileInfo fileInfo(filePath);
        Combo_RecentCSV->addItem(fileInfo.fileName(), filePath);
        Combo_RecentCSV->setItemData(
            Combo_RecentCSV->count() - 1,
            QDir::toNativeSeparators(filePath),
            Qt::ToolTipRole);
    }
    Btn_OpenRecentCSV->setEnabled(!m_recentCsvFiles.isEmpty() && !serialPort->isOpen());
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
    Btn_PlayPauseCSV->setText(QString::fromUtf8("暂停"));
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
    m_rawPlotBuffer.clear();
    m_filteredPlotBuffer.clear();
    clearMeasurement();
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
    if (Btn_PlayPauseCSV) Btn_PlayPauseCSV->setText(QString::fromUtf8("播放"));
}

void SerialPortAssistant::appendPlaybackRow(const CsvPlaybackRow& row)
{
    ensureSeriesCreated();
    if (row.hasVoltage) {
        m_rawPlotBuffer.append(0, row.timeSeconds, row.voltage);
        m_filteredPlotBuffer.append(
            0,
            row.timeSeconds,
            row.hasFiltered ? row.filteredVoltage : row.voltage);
    }
    if (row.hasCurrent) {
        m_rawPlotBuffer.append(1, row.timeSeconds, row.current);
        m_filteredPlotBuffer.append(
            1,
            row.timeSeconds,
            row.hasFiltered ? row.filteredCurrent : row.current);
    }
    if (row.hasOptical) {
        m_rawPlotBuffer.append(2, row.timeSeconds, row.optical);
        m_filteredPlotBuffer.append(
            2,
            row.timeSeconds,
            row.hasFiltered ? row.filteredOptical : row.optical);
    }
    if (row.hasVoltage || row.hasCurrent) ++globalSamplePairCount;
    if (row.hasOptical) ++globalOpticalSampleCount;
}
