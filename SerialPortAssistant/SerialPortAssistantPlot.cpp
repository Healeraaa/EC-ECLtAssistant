#include "SerialPortAssistant.h"

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

void SerialPortAssistant::updatePlotSeries()
{
    trimPlotBuffers();
    for (int channel = 0; channel < PlotDataBuffer::ChannelCount; ++channel) {
        if (channel < seriesList.size()) {
            seriesList[channel]->replace(m_plotBuffer.points(channel));
        }
    }

    updateSeriesVisibility();
    if (CheckBox_AutoScale->isChecked()) fitChartToData();
    chartView->chart()->update();
}

void SerialPortAssistant::trimPlotBuffers()
{
    int displayPointCount = Edit_XRange->text().toInt();
    if (displayPointCount <= 0) displayPointCount = 100;
    m_plotBuffer.trim(qMin(displayPointCount, 50000));
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

    const PlotDataRange range = m_plotBuffer.range(visibleChannels);
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
