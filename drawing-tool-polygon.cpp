#include "drawing-tool-polygon.h"
#include "drawingscene.h"
#include "drawingview.h"
#include <QGraphicsScene>

DrawingToolPolygon::DrawingToolPolygon(QObject *parent)
    : ToolBase(parent)
    , m_currentPolygon(nullptr)
    , m_drawing(false)
{
}

void DrawingToolPolygon::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    m_currentPolygon = nullptr;
    m_drawing = false;
}

void DrawingToolPolygon::deactivate()
{
    if (m_currentPolygon) {
        if (m_scene) {
            m_scene->removeItem(m_currentPolygon);
        }
        delete m_currentPolygon;
        m_currentPolygon = nullptr;
    }
    ToolBase::deactivate();
}

bool DrawingToolPolygon::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        if (!m_drawing) {
            // 开始新的多边形
            m_drawing = true;
            m_currentPolygon = new DrawingPolygon();
            m_currentPolygon->setPos(0, 0);
            m_currentPolygon->setFillBrush(Qt::lightGray);
            m_currentPolygon->setStrokePen(QPen(Qt::black));
            
            // 添加第一个点和预览点
            m_currentPolygon->addPoint(scenePos);
            m_currentPolygon->addPoint(scenePos); // 第二个点用于预览
            m_scene->addItem(m_currentPolygon);
        } else {
            // 确认上一个点的位置，然后添加新点
            if (m_currentPolygon && m_currentPolygon->pointCount() > 0) {
                // 先确认上一个点的位置（此时已经被mouseMove更新到正确位置）
                // 然后添加新的预览点
                m_currentPolygon->addPoint(scenePos);
            }
        }
        
        return true;
    } else if (event->button() == Qt::RightButton && m_drawing) {
        // 右键完成多边形绘制
        m_drawing = false;
        if (m_currentPolygon) {
            // 移除最后一个预览点
            if (m_currentPolygon->pointCount() > 2) {
                m_currentPolygon->removePoint(m_currentPolygon->pointCount() - 1);
            }
            m_currentPolygon = nullptr; // 不删除，让场景管理
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolygon::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentPolygon) {
        // 更新最后一个预览点的位置
        if (m_currentPolygon->pointCount() > 1) {
            m_currentPolygon->setPoint(m_currentPolygon->pointCount() - 1, scenePos);
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolygon::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    // 不需要特殊处理，mousePressEvent已经处理了点的添加
    return false;
}

bool DrawingToolPolygon::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        // 双击完成多边形绘制
        m_drawing = false;
        if (m_currentPolygon) {
            // 移除最后一个预览点
            if (m_currentPolygon->pointCount() > 2) {
                m_currentPolygon->removePoint(m_currentPolygon->pointCount() - 1);
            }
            m_currentPolygon = nullptr; // 不删除，让场景管理
        }
        return true;
    }
    
    return false;
}

#include "drawing-tool-polygon.moc"