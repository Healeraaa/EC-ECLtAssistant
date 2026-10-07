#include "PlotDataBuffer.h"

#include <algorithm>
#include <cmath>

namespace {

const QVector<QPointF>& emptyPoints()
{
    static const QVector<QPointF> points;
    return points;
}

const PlotChannelStatistics& emptyStatistics()
{
    static const PlotChannelStatistics statistics;
    return statistics;
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

}

bool PlotDataBuffer::append(int channel, double timeSeconds, double value, bool retainPoint)
{
    if (channel < 0 || channel >= ChannelCount
        || !std::isfinite(timeSeconds) || !std::isfinite(value)) {
        return false;
    }

    if (retainPoint) m_points[channel].append(QPointF(timeSeconds, value));
    PlotChannelStatistics& channelStatistics = m_statistics[channel];
    if (!channelStatistics.hasValue) {
        channelStatistics.minimum = value;
        channelStatistics.maximum = value;
        channelStatistics.hasValue = true;
    }
    else {
        channelStatistics.minimum = std::min(channelStatistics.minimum, value);
        channelStatistics.maximum = std::max(channelStatistics.maximum, value);
    }
    ++channelStatistics.count;
    return true;
}

void PlotDataBuffer::clear()
{
    for (int channel = 0; channel < ChannelCount; ++channel) {
        m_points[channel].clear();
        m_statistics[channel] = PlotChannelStatistics{};
    }
}

void PlotDataBuffer::trim(int maximumPointsPerChannel)
{
    maximumPointsPerChannel = std::max(1, maximumPointsPerChannel);
    for (QVector<QPointF>& channelPoints : m_points) {
        const int excessPointCount = channelPoints.size() - maximumPointsPerChannel;
        if (excessPointCount > 0) channelPoints.remove(0, excessPointCount);
    }
}

const QVector<QPointF>& PlotDataBuffer::points(int channel) const
{
    return channel >= 0 && channel < ChannelCount ? m_points[channel] : emptyPoints();
}

const PlotChannelStatistics& PlotDataBuffer::statistics(int channel) const
{
    return channel >= 0 && channel < ChannelCount
        ? m_statistics[channel]
        : emptyStatistics();
}

PlotDataRange PlotDataBuffer::range(
    const std::array<bool, ChannelCount>& visibleChannels) const
{
    PlotDataRange result;
    for (int channel = 0; channel < ChannelCount; ++channel) {
        if (!visibleChannels[channel]) continue;
        for (const QPointF& point : m_points[channel]) {
            includeValue(point.x(), &result.xMinimum, &result.xMaximum, &result.hasX);
            if (channel == 2) {
                includeValue(
                    point.y(),
                    &result.rightMinimum,
                    &result.rightMaximum,
                    &result.hasRightAxis);
            }
            else {
                includeValue(
                    point.y(),
                    &result.leftMinimum,
                    &result.leftMaximum,
                    &result.hasLeftAxis);
            }
        }
    }
    return result;
}
