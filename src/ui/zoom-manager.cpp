#include "zoom-manager.h"
#include "../core/drawing-canvas.h"
#include "../ui/drawingview.h"
#include <QApplication>
#include <QScrollBar>

ZoomManager::ZoomManager(DrawingCanvas *canvas, QObject *parent)
    : QObject(parent)
    , m_canvas(canvas)
    , m_view(nullptr)
    , m_zoomLevel(1.0)
    , m_zoomStep(0.1)
    , m_minZoomLevel(0.1)
    , m_maxZoomLevel(10.0)
{
    if (m_canvas) {
        m_view = qobject_cast<DrawingView*>(m_canvas->view());
        if (m_view) {
            m_zoomLevel = m_view->zoomLevel();
            connect(m_view, &DrawingView::zoomChanged, 
                    this, &ZoomManager::onViewChanged);
        }
        
        connect(m_canvas, &DrawingCanvas::viewportChanged,
                this, &ZoomManager::onCanvasChanged);
    }
}

ZoomManager::~ZoomManager()
{
}

void ZoomManager::zoomIn()
{
    setZoomLevel(m_zoomLevel + m_zoomStep);
}

void ZoomManager::zoomOut()
{
    setZoomLevel(m_zoomLevel - m_zoomStep);
}

void ZoomManager::resetZoom()
{
    setZoomLevel(1.0);
}

void ZoomManager::fitToWindow()
{
    if (!m_view || !m_canvas) return;
    
    // 获取场景边界
    QRectF sceneRect = m_canvas->scene()->sceneRect();
    if (sceneRect.isEmpty()) {
        emit statusMessageChanged("场景为空，无法适应窗口");
        return;
    }
    
    // 获取视口大小
    QSize viewportSize = m_view->viewport()->size();
    if (viewportSize.isEmpty()) {
        emit statusMessageChanged("视口为空，无法适应窗口");
        return;
    }
    
    // 计算适应缩放级别
    double scaleX = static_cast<double>(viewportSize.width()) / sceneRect.width();
    double scaleY = static_cast<double>(viewportSize.height()) / sceneRect.height();
    double fitZoom = qMin(scaleX, scaleY);
    
    // 限制缩放范围
    fitZoom = qBound(m_minZoomLevel, fitZoom, m_maxZoomLevel);
    
    setZoomLevel(fitZoom);
    centerOnContent();
    
    emit statusMessageChanged(QString("已适应窗口 (缩放: %1%)").arg(qRound(fitZoom * 100)));
}

void ZoomManager::setZoomLevel(double zoom)
{
    if (qFuzzyCompare(m_zoomLevel, zoom)) {
        return;
    }
    
    // 添加到历史记录
    m_zoomHistory.append(m_zoomLevel);
    if (m_zoomHistory.size() > MAX_ZOOM_HISTORY) {
        m_zoomHistory.removeFirst();
    }
    
    double oldZoom = m_zoomLevel;
    m_zoomLevel = zoom;
    
    // 限制缩放范围
    clampZoom();
    
    // 应用缩放
    if (m_view) {
        m_view->setZoomLevel(m_zoomLevel);
    }
    
    // 更新缩放标签
    updateZoomLabel();
    
    // 发送信号
    emit zoomChanged(m_zoomLevel);
    emit zoomLevelChanged(formatZoomText(m_zoomLevel));
    
    // 显示状态消息
    if (!qFuzzyCompare(oldZoom, m_zoomLevel)) {
        emit statusMessageChanged(QString("缩放: %1%").arg(qRound(m_zoomLevel * 100)));
    }
}

double ZoomManager::zoomLevel() const
{
    return m_zoomLevel;
}

void ZoomManager::setZoomStep(double step)
{
    m_zoomStep = qMax(0.01, step);
}

double ZoomManager::zoomStep() const
{
    return m_zoomStep;
}

void ZoomManager::setMinZoomLevel(double minZoom)
{
    m_minZoomLevel = qMax(0.01, minZoom);
    clampZoom();
}

double ZoomManager::minZoomLevel() const
{
    return m_minZoomLevel;
}

void ZoomManager::setMaxZoomLevel(double maxZoom)
{
    m_maxZoomLevel = qMax(0.1, maxZoom);
    clampZoom();
}

double ZoomManager::maxZoomLevel() const
{
    return m_maxZoomLevel;
}

void ZoomManager::centerOn(const QPointF &point)
{
    if (m_view) {
        m_view->centerOn(point);
    }
}

void ZoomManager::centerOnContent()
{
    if (!m_view || !m_canvas) return;
    
    QRectF sceneRect = m_canvas->scene()->sceneRect();
    if (!sceneRect.isEmpty()) {
        m_view->centerOn(sceneRect.center());
    }
}

void ZoomManager::ensureVisible(const QRectF &rect)
{
    if (m_view) {
        m_view->ensureVisible(rect, 50, 50);
    }
}

void ZoomManager::onCanvasChanged()
{
    // 视口变化时可能需要重新计算缩放
    updateZoomLabel();
}

void ZoomManager::onViewChanged()
{
    if (m_view) {
        m_zoomLevel = m_view->zoomLevel();
        updateZoomLabel();
    }
}

void ZoomManager::updateZoomLabel()
{
    QString zoomText = formatZoomText(m_zoomLevel);
    emit zoomLevelChanged(zoomText);
}

void ZoomManager::clampZoom()
{
    m_zoomLevel = qBound(m_minZoomLevel, m_zoomLevel, m_maxZoomLevel);
}

QString ZoomManager::formatZoomText(double zoom) const
{
    return QString("%1%").arg(qRound(zoom * 100));
}