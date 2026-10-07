#include "SerialPortAssistant.h"

#include <QFileInfo>
#include <algorithm>

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
        Label_ConnectionStatus->setText(QString::fromUtf8("CSV 回放"));
        Label_DataRate->setText(QString::fromUtf8("回放 ") + Combo_PlaybackSpeed->currentText());
    }
    else {
        Label_ConnectionStatus->setText(QString::fromUtf8("未连接"));
        Label_DataRate->setText("0 B/s");
    }

    Label_FrameStatus->setText(
        QString::fromUtf8("有效帧 %1 | CRC %2 | 无效帧 %3 | 丢弃 %4 B")
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
    Label_RenderStatus->setText(
        QString::fromUtf8("绘图 %1 点 | %2 ms | 缓存 %3/%4/%5")
            .arg(m_lastRenderedPoints)
            .arg(m_lastPlotRenderMilliseconds, 0, 'f', 1)
            .arg(m_plotBuffer.pointCount(0))
            .arg(m_plotBuffer.pointCount(1))
            .arg(m_plotBuffer.pointCount(2)));

    QStringList peakParts;
    const PlotChannelStatistics& voltage = m_plotBuffer.statistics(0);
    const PlotChannelStatistics& current = m_plotBuffer.statistics(1);
    const PlotChannelStatistics& optical = m_plotBuffer.statistics(2);
    if (voltage.hasValue) {
        peakParts << QString("V [%1, %2]")
            .arg(voltage.minimum, 0, 'g', 6)
            .arg(voltage.maximum, 0, 'g', 6);
    }
    if (current.hasValue) {
        peakParts << QString("I [%1, %2]")
            .arg(current.minimum, 0, 'g', 6)
            .arg(current.maximum, 0, 'g', 6);
    }
    if (optical.hasValue) {
        peakParts << QString::fromUtf8("ECL 最大值 %1").arg(optical.maximum, 0, 'g', 6);
    }
    Label_PeakStatus->setText(
        peakParts.isEmpty() ? QString::fromUtf8("暂无数据") : peakParts.join(" | "));

    if (m_csvRecorder.isRecording()) {
        const quint64 rowCount = m_csvRecorder.rowsWritten()
            + static_cast<quint64>(m_csvRecorder.pendingRows());
        Label_CSVStatus->setText(
            QString::fromUtf8("正在记录 %1 行：%2")
                .arg(rowCount)
                .arg(QFileInfo(m_csvRecorder.filePath()).fileName()));
    }
}
