#include "drawing-canvas.h"
#include "drawingview.h"
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

DrawingCanvas::DrawingCanvas(QWidget *parent)
    : QWidget(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_zoomLevel(1.0)
{
    setupUI();
}

void DrawingCanvas::setupUI()
{
    // 创建view但不设置scene，scene将由外部通过setScene方法设置
    m_view = new DrawingView(nullptr, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    
    setLayout(layout);
}

QGraphicsScene* DrawingCanvas::scene() const
{
    return m_scene;
}

DrawingView* DrawingCanvas::view() const
{
    return m_view;
}

void DrawingCanvas::setScene(QGraphicsScene *scene)
{
    if (m_scene) {
        // Disconnect from old scene
    }
    
    m_scene = scene;
    m_view->setScene(m_scene);
    
    if (m_scene) {
        // 不在这里调用fitInView，让调用方控制视图初始化
        // Connect to new scene
    }
}

void DrawingCanvas::zoomIn()
{
    setZoomLevel(m_zoomLevel * 1.2);
}

void DrawingCanvas::zoomOut()
{
    setZoomLevel(m_zoomLevel / 1.2);
}

void DrawingCanvas::resetZoom()
{
    setZoomLevel(1.0);
}

void DrawingCanvas::fitToWindow()
{
    if (!m_scene || !m_view) {
        return;
    }
    
    QRectF contentRect = m_scene->itemsBoundingRect();
    
    // 如果内容矩形有效，使用它；否则使用场景矩形
    if (!contentRect.isEmpty()) {
        m_view->fitInView(contentRect, Qt::KeepAspectRatio);
    } else {
        m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    }
    
    QTransform transform = m_view->transform();
    m_zoomLevel = transform.m11();
    emit zoomChanged(m_zoomLevel);
}

void DrawingCanvas::centerOnContent()
{
    if (!m_scene || !m_view) {
        return;
    }
    
    QRectF contentRect = m_scene->itemsBoundingRect();
    
    // 如果内容矩形有效，居中到内容；否则居中到场景中心
    if (!contentRect.isEmpty()) {
        m_view->centerOn(contentRect.center());
    } else {
        m_view->centerOn(m_scene->sceneRect().center());
    }
}

double DrawingCanvas::zoomLevel() const
{
    return m_zoomLevel;
}

void DrawingCanvas::setZoomLevel(double zoom)
{
    if (qFuzzyCompare(m_zoomLevel, zoom)) {
        return;
    }
    
    m_zoomLevel = qBound(0.1, zoom, 10.0);
    
    QTransform transform;
    transform.scale(m_zoomLevel, m_zoomLevel);
    m_view->setTransform(transform);
    
    emit zoomChanged(m_zoomLevel);
}

void DrawingCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    if (m_scene && m_view) {
        // Optionally maintain view center or fit to window
    }
}

void DrawingCanvas::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
}

#include "drawing-canvas.moc"