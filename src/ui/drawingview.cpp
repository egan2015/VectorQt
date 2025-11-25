#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QPainter>
#include "../ui/drawingview.h"
#include "../core/toolbase.h"

DrawingView::DrawingView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
    , m_zoomLevel(1.0)
    , m_currentTool(nullptr)
{
    // Qt原生渲染优化
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setRenderHint(QPainter::TextAntialiasing);
    
    // 视图优化标志
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    
    setDragMode(RubberBandDrag);
    setMouseTracking(true);
    
    // 设置对齐方式为居中
    setAlignment(Qt::AlignCenter);
    
    // 确保滚动条始终显示
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 设置最小的场景大小，确保滚动条能正常工作
    if (scene) {
        setSceneRect(scene->sceneRect());
    }
}

void DrawingView::setZoomLevel(double zoom)
{
    if (m_zoomLevel != zoom) {
        m_zoomLevel = zoom;
        QTransform transform;
        transform.scale(zoom, zoom);
        setTransform(transform);
        emit zoomChanged(zoom);
        emit viewportChanged(); // 触发视口变化信号，用于更新标尺
    }
}

void DrawingView::setCurrentTool(ToolBase *tool)
{
    m_currentTool = tool;
}

void DrawingView::zoomIn()
{
    setZoomLevel(m_zoomLevel * 1.2);
}

void DrawingView::zoomOut()
{
    setZoomLevel(m_zoomLevel / 1.2);
}

void DrawingView::resetZoom()
{
    setZoomLevel(1.0);
}

void DrawingView::fitToWindow()
{
    if (scene()) {
        fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
        m_zoomLevel = transform().m11();
        emit zoomChanged(m_zoomLevel);
    }
}

void DrawingView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void DrawingView::mousePressEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    emit mousePositionChanged(scenePos);
    
    if (m_currentTool && m_currentTool->mousePressEvent(event, scenePos)) {
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void DrawingView::mouseMoveEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    emit mousePositionChanged(scenePos);
    
    if (m_currentTool && m_currentTool->mouseMoveEvent(event, scenePos)) {
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void DrawingView::mouseReleaseEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    
    if (m_currentTool && m_currentTool->mouseReleaseEvent(event, scenePos)) {
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void DrawingView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    
    if (m_currentTool && m_currentTool->mouseDoubleClickEvent(event, scenePos)) {
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void DrawingView::keyPressEvent(QKeyEvent *event)
{
    if (m_currentTool && m_currentTool->keyPressEvent(event)) {
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void DrawingView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    emit viewportChanged();
}

void DrawingView::updateZoomLabel()
{
    emit zoomChanged(m_zoomLevel);
    emit viewportChanged();
}