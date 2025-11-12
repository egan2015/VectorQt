#include "drawing-tool-polyline.h"
#include "drawingscene.h"
#include "drawingview.h"
#include <QGraphicsScene>

DrawingToolPolyline::DrawingToolPolyline(QObject *parent)
    : ToolBase(parent)
    , m_currentPolyline(nullptr)
    , m_drawing(false)
{
}

void DrawingToolPolyline::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    m_currentPolyline = nullptr;
    m_drawing = false;
}

void DrawingToolPolyline::deactivate()
{
    if (m_currentPolyline) {
        if (m_scene) {
            m_scene->removeItem(m_currentPolyline);
        }
        delete m_currentPolyline;
        m_currentPolyline = nullptr;
    }
    ToolBase::deactivate();
}

bool DrawingToolPolyline::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        if (!m_drawing) {
            // 开始新的折线
            m_drawing = true;
            m_currentPolyline = new DrawingPolyline();
            m_currentPolyline->setPos(0, 0);
            m_currentPolyline->setFillBrush(Qt::NoBrush);
            m_currentPolyline->setStrokePen(QPen(Qt::black));
            
            // 添加第一个点和预览点
            m_currentPolyline->addPoint(scenePos);
            m_currentPolyline->addPoint(scenePos); // 第二个点用于预览
            m_scene->addItem(m_currentPolyline);
        } else {
            // 确认上一个点的位置，然后添加新点
            if (m_currentPolyline && m_currentPolyline->pointCount() > 0) {
                // 先确认上一个点的位置（此时已经被mouseMove更新到正确位置）
                // 然后添加新的预览点
                m_currentPolyline->addPoint(scenePos);
            }
        }
        
        return true;
    } else if (event->button() == Qt::RightButton && m_drawing) {
        // 右键完成折线绘制
        m_drawing = false;
        if (m_currentPolyline) {
            // 最后一个点已经在mouseReleaseEvent中被确认了
            // 不需要移除任何点
            m_currentPolyline = nullptr; // 不删除，让场景管理
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolyline::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentPolyline) {
        // 更新最后一个预览点的位置
        if (m_currentPolyline->pointCount() > 1) {
            m_currentPolyline->setPoint(m_currentPolyline->pointCount() - 1, scenePos);
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolyline::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        // 确保最后一个点在正确位置
        if (m_currentPolyline && m_currentPolyline->pointCount() > 0) {
            m_currentPolyline->setPoint(m_currentPolyline->pointCount() - 1, scenePos);
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolyline::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        // 双击完成折线绘制
        m_drawing = false;
        if (m_currentPolyline) {
            // 最后一个点已经在mouseReleaseEvent中被确认了
            // 不需要移除任何点
            m_currentPolyline = nullptr; // 不删除，让场景管理
        }
        return true;
    }
    
    return false;
}

#include "drawing-tool-polyline.moc"