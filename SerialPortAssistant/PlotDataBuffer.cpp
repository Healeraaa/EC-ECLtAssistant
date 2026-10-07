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

int PlotDataBuffer::pointCount(int channel) const
{
    return channel >= 0 && channel < ChannelCount ? m_points[channel].size() : 0;
}

const PlotChannelStatistics& PlotDataBuffer::statistics(int channel) const
{
    return channel >= 0 && channel < ChannelCount
        ? m_statistics[channel]
        : emptyStatistics();
}

bool PlotDataBuffer::nearestPoint(int channel, double timeSeconds, QPointF* point) const
{
    if (!point || channel < 0 || channel >= ChannelCount
        || !std::isfinite(timeSeconds) || m_points[channel].isEmpty()) {
        return false;
    }

    const QVector<QPointF>& channelPoints = m_points[channel];
    const auto upper = std::lower_bound(
        channelPoints.cbegin(),
        channelPoints.cend(),
        timeSeconds,
        [](const QPointF& candidate, double time) { return candidate.x() < time; });

    if (upper == channelPoints.cbegin()) {
        *point = *upper;
        return true;
    }
    if (upper == channelPoints.cend()) {
        *point = channelPoints.last();
        return true;
    }

    const QPointF& right = *upper;
    const QPointF& left = *(upper - 1);
    *point = std::fabs(timeSeconds - left.x()) <= std::fabs(right.x() - timeSeconds)
        ? left
        : right;
    return true;
}

QVector<QPointF> PlotDataBuffer::decimatedPoints(int channel, int maximumPoints) const
{
    if (channel < 0 || channel >= ChannelCount || maximumPoints <= 0) return {};
    const QVector<QPointF>& source = m_points[channel];
    if (source.size() <= maximumPoints) return source;
    if (maximumPoints == 1) return { source.last() };
    if (maximumPoints == 2) return { source.first(), source.last() };

    QVector<QPointF> result;
    result.reserve(maximumPoints);
    result.append(source.first());

    const int interiorCount = source.size() - 2;
    const int bucketCount = std::max(1, (maximumPoints - 2) / 2);
    for (int bucket = 0; bucket < bucketCount; ++bucket) {
        const int begin = 1 + (bucket * interiorCount) / bucketCount;
        const int end = 1 + ((bucket + 1) * interiorCount) / bucketCount;
        int minimumIndex = begin;
        int maximumIndex = begin;
        for (int index = begin + 1; index < end; ++index) {
            if (source.at(index).y() < source.at(minimumIndex).y()) minimumIndex = index;
            if (source.at(index).y() > source.at(maximumIndex).y()) maximumIndex = index;
        }
        if (minimumIndex == maximumIndex) {
            result.append(source.at(minimumIndex));
        }
        else if (minimumIndex < maximumIndex) {
            result.append(source.at(minimumIndex));
            result.append(source.at(maximumIndex));
        }
        else {
            result.append(source.at(maximumIndex));
            result.append(source.at(minimumIndex));
        }
    }
    result.append(source.last());
    return result;
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
