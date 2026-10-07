#ifndef CHARTINTERACTIONVIEW_H
#define CHARTINTERACTIONVIEW_H

#include <QtCharts/QChartView>
#include <functional>

QT_CHARTS_USE_NAMESPACE

class ChartInteractionView : public QChartView
{
public:
    using PositionHandler = std::function<void(const QPointF&)>;

    explicit ChartInteractionView(QChart* chart, QWidget* parent = nullptr);

    void setCursorEnabled(bool enabled);
    void setCursorMovedHandler(PositionHandler handler);
    void setPlotClickedHandler(PositionHandler handler);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool isInsidePlot(const QPointF& position) const;

    bool m_cursorEnabled = true;
    bool m_cursorInsidePlot = false;
    QPointF m_cursorPosition;
    PositionHandler m_cursorMovedHandler;
    PositionHandler m_plotClickedHandler;
};

#endif // CHARTINTERACTIONVIEW_H
