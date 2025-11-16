#include "drawing-tool-bezier-edit.h"
#include "drawingscene.h"
#include "drawing-shape.h"
#include "drawing-edit-handles.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>

DrawingBezierEditTool::DrawingBezierEditTool(QObject *parent)
    : ToolBase(parent)
    , m_selectedPath(nullptr)
    , m_activeHandle(nullptr)
    , m_isDragging(false)
{
}

DrawingBezierEditTool::~DrawingBezierEditTool()
{
    cleanupControlPointHandles();
}

bool DrawingBezierEditTool::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton) {
        // 查找点击位置是否有控制点把手
        for (EditHandle *handle : m_controlPointHandles) {
            if (handle && handle->contains(handle->mapFromScene(scenePos))) {
                m_activeHandle = handle;
                m_isDragging = true;
                m_originalControlPoints = m_selectedPath->controlPoints();
                
                // qDebug() << "Started dragging control point handle";
                return true;
            }
        }
        
        // 检查是否点击了路径本身，选择新的路径
        if (m_scene) {
            QList<QGraphicsItem*> items = m_scene->items(scenePos);
            for (QGraphicsItem *item : items) {
                DrawingPath *path = qgraphicsitem_cast<DrawingPath*>(item);
                if (path && path->path().contains(path->mapFromScene(scenePos))) {
                    // 如果选中了不同的路径，清理旧的控制点把手
                    if (m_selectedPath) {
                        m_selectedPath->setShowControlPolygon(false);  // 隐藏旧路径的控制点连线
                        cleanupControlPointHandles();
                    }
                    
                    m_selectedPath = path;
                    // 启用新路径的控制点连线显示
                    m_selectedPath->setShowControlPolygon(true);
                    createControlPointHandles();
                    qDebug() << "Selected bezier path for editing, control points:" << m_selectedPath->controlPoints().size();
                    return true;
                }
            }
        }
        
        // 没有选中任何路径，清理控制点把手
        if (m_selectedPath) {
            m_selectedPath->setShowControlPolygon(false);  // 隐藏控制点连线
        }
        cleanupControlPointHandles();
        m_selectedPath = nullptr;
        
        return true;
    }
    
    return ToolBase::mousePressEvent(event, scenePos);
}

bool DrawingBezierEditTool::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_isDragging && m_activeHandle && m_selectedPath) {
        // 拖动控制点
        int handleIndex = m_controlPointHandles.indexOf(m_activeHandle);
        if (handleIndex >= 0 && handleIndex < m_selectedPath->controlPoints().size()) {
            // 更新控制点位置
            QVector<QPointF> controlPoints = m_selectedPath->controlPoints();
            controlPoints[handleIndex] = scenePos;  // 使用场景坐标
            m_selectedPath->setControlPoints(controlPoints);
            
            // 更新所有控制点把手的位置
            updateControlPointHandles();
            
            // 触发场景重绘
            m_scene->update();
        }
        
        return true;
    }
    
    return ToolBase::mouseMoveEvent(event, scenePos);
}

bool DrawingBezierEditTool::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        m_activeHandle = nullptr;
        
        // 确保路径已标记为修改
        if (m_scene) {
            m_scene->setModified(true);
        }
        
        return true;
    }
    
    return ToolBase::mouseReleaseEvent(event, scenePos);
}

void DrawingBezierEditTool::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    qDebug() << "Bezier edit tool activated";
    
    // 连接对象状态变化信号
    if (scene) {
        connect(scene, &DrawingScene::objectStateChanged, this, &DrawingBezierEditTool::onObjectStateChanged, Qt::UniqueConnection);
    }
    
    // 检查当前是否有选中的路径
    if (scene) {
        QList<QGraphicsItem*> selectedItems = scene->selectedItems();
        for (QGraphicsItem *item : selectedItems) {
            DrawingPath *path = qgraphicsitem_cast<DrawingPath*>(item);
            if (path) {
                m_selectedPath = path;
                // 启用控制点连线显示
                m_selectedPath->setShowControlPolygon(true);
                createControlPointHandles();
                break;
            }
        }
    }
}

void DrawingBezierEditTool::deactivate()
{
    // 清理控制点把手
    cleanupControlPointHandles();
    
    // 隐藏控制点连线
    if (m_selectedPath) {
        m_selectedPath->setShowControlPolygon(false);
        m_selectedPath = nullptr;
    }
    
    // 断开对象状态变化信号
    if (m_scene) {
        disconnect(m_scene, &DrawingScene::objectStateChanged, this, &DrawingBezierEditTool::onObjectStateChanged);
    }
    
    ToolBase::deactivate();
}

void DrawingBezierEditTool::createControlPointHandles()
{
    if (!m_selectedPath || !m_scene) {
        return;
    }
    
    // 清理现有的控制点把手
    cleanupControlPointHandles();
    
    // 获取路径的控制点
    QVector<QPointF> controlPoints = m_selectedPath->controlPoints();
    
    // 为每个控制点创建把手
    for (const QPointF &point : controlPoints) {
        // 创建控制点把手
        EditHandle *handle = new EditHandle(EditHandle::Custom, static_cast<QGraphicsItem*>(m_selectedPath));
        handle->setRect(QRectF(-5, -5, 10, 10));  // 设置把手大小
        handle->setPos(point);  // 设置把手位置（在场景坐标中）
        
        // 添加到场景
        m_scene->addItem(handle);
        
        // 添加到列表
        m_controlPointHandles.append(handle);
    }
    
    qDebug() << "Created" << m_controlPointHandles.size() << "control point handles";
}

void DrawingBezierEditTool::updateControlPointHandles()
{
    if (!m_selectedPath) {
        return;
    }
    
    QVector<QPointF> controlPoints = m_selectedPath->controlPoints();
    
    // 更新每个控制点把手的位置
    for (int i = 0; i < m_controlPointHandles.size() && i < controlPoints.size(); ++i) {
        if (m_controlPointHandles[i]) {
            m_controlPointHandles[i]->setPos(controlPoints[i]);
        }
    }
}

void DrawingBezierEditTool::cleanupControlPointHandles()
{
    // 从场景中移除并删除所有控制点把手
    for (EditHandle *handle : m_controlPointHandles) {
        if (handle && m_scene) {
            m_scene->removeItem(handle);
            delete handle;
        }
    }
    m_controlPointHandles.clear();
    m_activeHandle = nullptr;
}

void DrawingBezierEditTool::onObjectStateChanged(DrawingShape* shape)
{
    // 如果状态变化的路径是当前正在编辑的路径，更新控制点把手
    if (shape == m_selectedPath) {
        updateControlPointHandles();
    }
}