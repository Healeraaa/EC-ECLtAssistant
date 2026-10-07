#include "ChartInteractionView.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <utility>

ChartInteractionView::ChartInteractionView(QChart* chart, QWidget* parent)
    : QChartView(chart, parent)
{
    setMouseTracking(true);
}

void ChartInteractionView::setCursorEnabled(bool enabled)
{
    m_cursorEnabled = enabled;
    if (!enabled) m_cursorInsidePlot = false;
    viewport()->update();
}

void ChartInteractionView::setCursorMovedHandler(PositionHandler handler)
{
    m_cursorMovedHandler = std::move(handler);
}

void ChartInteractionView::setPlotClickedHandler(PositionHandler handler)
{
    m_plotClickedHandler = std::move(handler);
}

void ChartInteractionView::mouseMoveEvent(QMouseEvent* event)
{
    m_cursorPosition = event->pos();
    m_cursorInsidePlot = isInsidePlot(m_cursorPosition);
    if (m_cursorInsidePlot && m_cursorMovedHandler) {
        m_cursorMovedHandler(m_cursorPosition);
    }
    viewport()->update();
    QChartView::mouseMoveEvent(event);
}

void ChartInteractionView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton
        && isInsidePlot(event->pos())
        && m_plotClickedHandler) {
        m_plotClickedHandler(event->pos());
    }
    QChartView::mousePressEvent(event);
}

void ChartInteractionView::leaveEvent(QEvent* event)
{
    m_cursorInsidePlot = false;
    viewport()->update();
    QChartView::leaveEvent(event);
}

void ChartInteractionView::paintEvent(QPaintEvent* event)
{
    QChartView::paintEvent(event);
    if (!m_cursorEnabled || !m_cursorInsidePlot) return;

    const QRectF plotArea = chart()->plotArea();
    QPainter painter(viewport());
    QPen pen(QColor("#7dd3fc"));
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawLine(
        QPointF(m_cursorPosition.x(), plotArea.top()),
        QPointF(m_cursorPosition.x(), plotArea.bottom()));
    painter.drawLine(
        QPointF(plotArea.left(), m_cursorPosition.y()),
        QPointF(plotArea.right(), m_cursorPosition.y()));
}

bool ChartInteractionView::isInsidePlot(const QPointF& position) const
{
    return chart() && chart()->plotArea().contains(position);
}
