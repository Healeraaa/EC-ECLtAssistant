#include "SerialPortAssistant.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {

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
    while (seriesList.size() < PlotDataBuffer::ChannelCount) {
        QLineSeries* series = new QLineSeries();
        const int seriesIndex = seriesList.size();
        const QColor colors[] = { QColor("#00e5ff"), QColor("#ff6bc1"), QColor("#00ff88") };
        const QString names[] = {
            QString::fromUtf8("电压"),
            QString::fromUtf8("电流"),
            QStringLiteral("ECL")
        };

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

void SerialPortAssistant::updatePlotSeries()
{
    QElapsedTimer renderTimer;
    renderTimer.start();
    trimPlotBuffers();
    int renderPointLimit = Edit_RenderPoints->text().toInt();
    if (renderPointLimit <= 0) renderPointLimit = 4000;
    renderPointLimit = qBound(200, renderPointLimit, 20000);
    m_lastRenderedPoints = 0;
    const PlotDataBuffer& plotBuffer = displayPlotBuffer();
    for (int channel = 0; channel < PlotDataBuffer::ChannelCount; ++channel) {
        if (channel < seriesList.size() && CheckBox_ChannelVisible[channel]->isChecked()) {
            const QVector<QPointF> displayPoints =
                plotBuffer.decimatedPoints(channel, renderPointLimit);
            m_lastRenderedPoints += displayPoints.size();
            seriesList[channel]->replace(displayPoints);
        }
    }

    updateSeriesVisibility();
    if (CheckBox_AutoScale->isChecked()) fitChartToData();
    chartView->chart()->update();
    m_lastPlotRenderMilliseconds = renderTimer.nsecsElapsed() / 1000000.0;
}

void SerialPortAssistant::trimPlotBuffers()
{
    int displayPointCount = Edit_XRange->text().toInt();
    if (displayPointCount <= 0) displayPointCount = 100;
    const int maximumPoints = qMin(displayPointCount, 50000);
    m_rawPlotBuffer.trim(maximumPoints);
    m_filteredPlotBuffer.trim(maximumPoints);
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
    std::array<bool, PlotDataBuffer::ChannelCount> visibleChannels;
    for (int channel = 0; channel < PlotDataBuffer::ChannelCount; ++channel) {
        visibleChannels[channel] = CheckBox_ChannelVisible[channel]->isChecked();
    }

    const PlotDataRange range = displayPlotBuffer().range(visibleChannels);
    if (range.hasX) setPaddedRange(axisX, range.xMinimum, range.xMaximum);
    if (range.hasLeftAxis) {
        setPaddedRange(axisY, range.leftMinimum, range.leftMaximum);
    }
    if (range.hasRightAxis) {
        setPaddedRange(axisYRight, range.rightMinimum, range.rightMaximum);
    }
}

void SerialPortAssistant::updateSeriesVisibility()
{
    for (int channel = 0;
         channel < seriesList.size() && channel < PlotDataBuffer::ChannelCount;
         ++channel) {
        seriesList[channel]->setVisible(CheckBox_ChannelVisible[channel]->isChecked());
    }
}

void SerialPortAssistant::handleChartCursor(const QPointF& position)
{
    if (!CheckBox_Crosshair->isChecked()) return;
    const QRectF plotArea = chartView->chart()->plotArea();
    if (!plotArea.contains(position) || plotArea.width() <= 0.0) return;

    const double ratio = (position.x() - plotArea.left()) / plotArea.width();
    const double time = axisX->min() + ratio * (axisX->max() - axisX->min());
    const char* channelNames[] = { "V", "I", "ECL" };
    QStringList values;
    values << QString("t=%1 s").arg(time, 0, 'g', 7);
    for (int channel = 0; channel < PlotDataBuffer::ChannelCount; ++channel) {
        if (!CheckBox_ChannelVisible[channel]->isChecked()) continue;
        QPointF nearest;
        if (displayPlotBuffer().nearestPoint(channel, time, &nearest)) {
            values << QString("%1=%2").arg(channelNames[channel]).arg(nearest.y(), 0, 'g', 7);
        }
    }
    Label_CursorReadout->setText(values.join(" | "));
}

void SerialPortAssistant::handleChartClick(const QPointF& position)
{
    if (!CheckBox_Measurement->isChecked()) return;
    const QRectF plotArea = chartView->chart()->plotArea();
    if (!plotArea.contains(position) || plotArea.width() <= 0.0) return;

    MeasurementPoint selected;
    const double ratio = (position.x() - plotArea.left()) / plotArea.width();
    selected.timeSeconds = axisX->min() + ratio * (axisX->max() - axisX->min());
    for (int channel = 0; channel < PlotDataBuffer::ChannelCount; ++channel) {
        QPointF nearest;
        if (displayPlotBuffer().nearestPoint(channel, selected.timeSeconds, &nearest)) {
            selected.hasValue[channel] = true;
            selected.values[channel] = nearest.y();
            selected.valid = true;
        }
    }
    if (!selected.valid) {
        Label_Measurement->setText(QString::fromUtf8("当前曲线没有可测量的数据"));
        return;
    }

    if (m_measurementClickCount == 0 || m_measurementClickCount >= 2) {
        m_measurementPointA = selected;
        m_measurementPointB = MeasurementPoint{};
        m_measurementClickCount = 1;
        Label_Measurement->setText(
            QString::fromUtf8("A：t=%1 s；请单击选择测量点 B")
                .arg(selected.timeSeconds, 0, 'g', 7));
        return;
    }

    m_measurementPointB = selected;
    m_measurementClickCount = 2;
    const char* deltaNames[] = { "ΔV", "ΔI", "ΔECL" };
    QStringList result;
    result << QString::fromUtf8("Δt=%1 s")
        .arg(m_measurementPointB.timeSeconds - m_measurementPointA.timeSeconds, 0, 'g', 7);
    for (int channel = 0; channel < PlotDataBuffer::ChannelCount; ++channel) {
        if (m_measurementPointA.hasValue[channel] && m_measurementPointB.hasValue[channel]) {
            result << QString("%1=%2")
                .arg(deltaNames[channel])
                .arg(m_measurementPointB.values[channel] - m_measurementPointA.values[channel], 0, 'g', 7);
        }
    }
    Label_Measurement->setText(result.join(" | "));
}

void SerialPortAssistant::clearMeasurement()
{
    m_measurementPointA = MeasurementPoint{};
    m_measurementPointB = MeasurementPoint{};
    m_measurementClickCount = 0;
    if (Label_Measurement) {
        Label_Measurement->setText(CheckBox_Measurement->isChecked()
            ? QString::fromUtf8("请在曲线上单击选择测量点 A")
            : QString::fromUtf8("测量未启用"));
    }
}

void SerialPortAssistant::resetChartZoom()
{
    chartView->chart()->zoomReset();
    if (CheckBox_AutoScale->isChecked()) fitChartToData();
    else applyManualAxisRanges();
}

void SerialPortAssistant::exportChartImage()
{
    const QString defaultDirectory = QDir(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
        .filePath("EC-ECL");
    QDir().mkpath(defaultDirectory);
    QString filePath = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8("导出波形图片"),
        QDir(defaultDirectory).filePath(
            "EC-ECL_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png"),
        QString::fromUtf8("PNG 图片 (*.png)"));
    if (filePath.isEmpty()) return;
    if (QFileInfo(filePath).suffix().isEmpty()) filePath += ".png";

    if (!chartView->grab().save(filePath, "PNG")) {
        QMessageBox::warning(this, QString::fromUtf8("导出失败"), QString::fromUtf8("无法保存波形图片。"));
        return;
    }
    SerialPort_ReceiveAear->appendPlainText("[Chart] Exported: " + QDir::toNativeSeparators(filePath));
}
