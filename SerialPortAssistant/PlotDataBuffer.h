#ifndef PLOTDATABUFFER_H
#define PLOTDATABUFFER_H

#include <QPointF>
#include <QVector>
#include <QtGlobal>
#include <array>

struct PlotChannelStatistics
{
    bool hasValue = false;
    double minimum = 0.0;
    double maximum = 0.0;
    quint64 count = 0;
};

struct PlotDataRange
{
    bool hasX = false;
    bool hasLeftAxis = false;
    bool hasRightAxis = false;
    double xMinimum = 0.0;
    double xMaximum = 0.0;
    double leftMinimum = 0.0;
    double leftMaximum = 0.0;
    double rightMinimum = 0.0;
    double rightMaximum = 0.0;
};

class PlotDataBuffer
{
public:
    static constexpr int ChannelCount = 3;

    bool append(int channel, double timeSeconds, double value, bool retainPoint = true);
    void clear();
    void trim(int maximumPointsPerChannel);

    const QVector<QPointF>& points(int channel) const;
    const PlotChannelStatistics& statistics(int channel) const;
    bool nearestPoint(int channel, double timeSeconds, QPointF* point) const;
    PlotDataRange range(const std::array<bool, ChannelCount>& visibleChannels) const;

private:
    QVector<QPointF> m_points[ChannelCount];
    PlotChannelStatistics m_statistics[ChannelCount];
};

#endif // PLOTDATABUFFER_H
