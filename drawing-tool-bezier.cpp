#include "drawing-tool-bezier.h"
#include "drawingscene.h"
#include "drawing-shape.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>

DrawingBezierTool::DrawingBezierTool(QObject *parent)
    : ToolBase(parent)
    , m_currentPath(nullptr)
    , m_isDrawing(false)
    , m_currentItem(nullptr)
    , m_previewItem(nullptr)
{
}

DrawingBezierTool::~DrawingBezierTool()
{
    // 确保清理所有资源
    if (m_currentPath) {
        delete m_currentPath;
        m_currentPath = nullptr;
    }
    
    // 清理预览项（如果存在）
    if (m_previewItem) {
        // 注意：在析构时，场景可能已经被销毁，所以我们只删除对象而不从场景中移除
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    
    // 注意：不要在这里删除m_currentItem，
    // 因为它已经被添加到场景中，场景会负责清理它
}

bool DrawingBezierTool::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    // 应用网格对齐
    QPointF alignedPos = scenePos;
    if (qobject_cast<DrawingScene*>(m_scene)) {
        DrawingScene *drawingScene = qobject_cast<DrawingScene*>(m_scene);
        if (drawingScene->isGridAlignmentEnabled()) {
            // 使用智能网格吸附
            DrawingScene::SnapResult gridSnap = drawingScene->smartAlignToGrid(scenePos);
            alignedPos = gridSnap.snappedPos;
            
            // 尝试对象吸附
            DrawingScene::ObjectSnapResult objectSnap = drawingScene->snapToObjects(scenePos);
            if (objectSnap.snappedToObject) {
                // 对象吸附优先级更高
                alignedPos = objectSnap.snappedPos;
            }
        }
    }
    
    if (event->button() == Qt::LeftButton) {
        if (!m_isDrawing) {
            // 开始绘制新的贝塞尔曲线
            m_isDrawing = true;
            m_currentPath = new QPainterPath();
            m_currentPath->moveTo(alignedPos);
            
            // 添加第一个控制点
            m_controlPoints.clear();
            m_controlPoints.append(alignedPos);
            
            // 创建预览项
            if (!m_previewItem) {
                m_previewItem = new DrawingPath();
                m_previewItem->setStrokePen(QPen(Qt::blue, 2, Qt::DashLine));
                m_previewItem->setFillBrush(Qt::NoBrush);
                if (m_scene) {
                    m_scene->addItem(m_previewItem);
                }
            }
            
            qDebug() << "Started drawing bezier curve at:" << alignedPos;
        } else {
            // 添加新的控制点
            m_controlPoints.append(alignedPos);
            
            // 如果已经有足够的点来形成贝塞尔曲线，则更新路径
            updatePath();
            
            qDebug() << "Added control point at:" << alignedPos 
                     << "Total points:" << m_controlPoints.size();
        }
        
        // 更新预览
        if (m_previewItem && m_currentPath) {
            m_previewItem->setPath(*m_currentPath);
        }
        
        // 触发场景重绘
        if (m_scene) {
            m_scene->update();
        }
        
        return true; // 事件被消费
    } else if (event->button() == Qt::RightButton && m_isDrawing) {
        // 右键完成绘制
        finishDrawing();
        
        // 触发场景重绘
        if (m_scene) {
            m_scene->update();
        }
        
        return true; // 事件被消费
    }
    
    return ToolBase::mousePressEvent(event, scenePos);
}

bool DrawingBezierTool::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    // 应用网格对齐
    QPointF alignedPos = scenePos;
    if (qobject_cast<DrawingScene*>(m_scene)) {
        DrawingScene *drawingScene = qobject_cast<DrawingScene*>(m_scene);
        if (drawingScene->isGridAlignmentEnabled()) {
            // 使用智能网格吸附
            DrawingScene::SnapResult gridSnap = drawingScene->smartAlignToGrid(scenePos);
            alignedPos = gridSnap.snappedPos;
            
            // 尝试对象吸附
            DrawingScene::ObjectSnapResult objectSnap = drawingScene->snapToObjects(scenePos);
            if (objectSnap.snappedToObject) {
                // 对象吸附优先级更高
                alignedPos = objectSnap.snappedPos;
            }
        }
    }
    
    if (m_isDrawing && m_currentPath && !m_controlPoints.isEmpty()) {
        // 更新最后一个控制点的位置（用于预览）
        if (m_controlPoints.size() > 1) {
            // 如果已经有至少两个点，则更新最后一个控制点
            m_controlPoints.last() = alignedPos;
        } else {
            // 如果只有一个点，则添加临时点用于预览
            if (m_controlPoints.size() == 1) {
                m_controlPoints.append(alignedPos);
            }
        }
        
        // 更新路径用于预览
        updatePath();
        
        // 更新预览
        if (m_previewItem && m_currentPath) {
            m_previewItem->setPath(*m_currentPath);
        }
        
        // 触发场景重绘
        if (m_scene) {
            m_scene->update();
        }
        
        return true; // 事件被消费
    }
    
    return ToolBase::mouseMoveEvent(event, scenePos);
}

bool DrawingBezierTool::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_isDrawing) {
        // 左键释放，已经在mousePressEvent中处理了控制点的添加
        return true; // 事件被消费
    }
    
    return ToolBase::mouseReleaseEvent(event, scenePos);
}

void DrawingBezierTool::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    qDebug() << "Bezier tool activated";
}

void DrawingBezierTool::deactivate()
{
    // 如果正在绘制，完成绘制
    if (m_isDrawing) {
        finishDrawing();
    } else {
        // 如果有预览项但没有在绘制，也要清理预览项
        if (m_previewItem) {
            if (m_scene) {
                m_scene->removeItem(m_previewItem);
            }
            delete m_previewItem;
            m_previewItem = nullptr;
        }
    }
    
    // 清理当前路径
    if (m_currentPath) {
        delete m_currentPath;
        m_currentPath = nullptr;
    }
    
    ToolBase::deactivate();
    qDebug() << "Bezier tool deactivated";
}

void DrawingBezierTool::updatePath()
{
    if (!m_currentPath || m_controlPoints.isEmpty()) {
        return;
    }
    
    // 重新构建路径
    m_currentPath->clear();
    
    // 安全地获取第一个点
    if (m_controlPoints.size() > 0) {
        m_currentPath->moveTo(m_controlPoints[0]);
    }
    
    // 根据控制点数量创建贝塞尔曲线
    for (int i = 1; i < m_controlPoints.size(); i += 3) {
        if (i + 2 < m_controlPoints.size()) {
            // 有足够的点创建三次贝塞尔曲线
            m_currentPath->cubicTo(m_controlPoints[i], m_controlPoints[i+1], m_controlPoints[i+2]);
        } else if (i + 1 < m_controlPoints.size()) {
            // 有足够的点创建二次贝塞尔曲线
            m_currentPath->quadTo(m_controlPoints[i], m_controlPoints[i+1]);
        } else if (i < m_controlPoints.size()) {
            // 只有一个点，创建直线
            m_currentPath->lineTo(m_controlPoints[i]);
        }
    }
}

void DrawingBezierTool::finishDrawing()
{
    if (m_isDrawing && m_currentPath && m_controlPoints.size() >= 2) {
        // 如果启用了网格对齐，创建对齐后的路径
        QPainterPath alignedPath;
        if (qobject_cast<DrawingScene*>(m_scene)) {
            DrawingScene *drawingScene = qobject_cast<DrawingScene*>(m_scene);
            if (drawingScene->isGridAlignmentEnabled()) {
                // 重新构建路径，应用网格对齐到所有控制点
                alignedPath.moveTo(drawingScene->alignToGrid(m_controlPoints.first()));
                
                for (int i = 1; i < m_controlPoints.size(); i += 3) {
                    if (i + 2 < m_controlPoints.size()) {
                        // 有足够的点创建三次贝塞尔曲线
                        QPointF p1 = drawingScene->alignToGrid(m_controlPoints[i]);
                        QPointF p2 = drawingScene->alignToGrid(m_controlPoints[i+1]);
                        QPointF p3 = drawingScene->alignToGrid(m_controlPoints[i+2]);
                        alignedPath.cubicTo(p1, p2, p3);
                    } else if (i + 1 < m_controlPoints.size()) {
                        // 有足够的点创建二次贝塞尔曲线
                        QPointF p1 = drawingScene->alignToGrid(m_controlPoints[i]);
                        QPointF p2 = drawingScene->alignToGrid(m_controlPoints[i+1]);
                        alignedPath.quadTo(p1, p2);
                    } else if (i < m_controlPoints.size()) {
                        // 只有一个点，创建直线
                        QPointF p1 = drawingScene->alignToGrid(m_controlPoints[i]);
                        alignedPath.lineTo(p1);
                    }
                }
            }
        }
        
        // 创建最终的路径图形
        m_currentItem = new DrawingPath();
        if (qobject_cast<DrawingScene*>(m_scene) && qobject_cast<DrawingScene*>(m_scene)->isGridAlignmentEnabled()) {
            m_currentItem->setPath(alignedPath);
        } else {
            m_currentItem->setPath(*m_currentPath);
        }
        m_currentItem->setStrokePen(QPen(Qt::black, 2));
        m_currentItem->setFillBrush(Qt::NoBrush);
        
        // 保存原始控制点以便后续编辑
        static_cast<DrawingPath*>(m_currentItem)->setControlPoints(m_controlPoints);
        
        // 将图形添加到场景
        if (m_scene) {
            m_scene->addItem(m_currentItem);
            m_scene->setModified(true);
        }
        
        qDebug() << "Finished drawing bezier curve with" << m_controlPoints.size() << "control points";
    }
    
    // 从场景中移除并删除预览项
    if (m_previewItem) {
        if (m_scene) {
            m_scene->removeItem(m_previewItem);
        }
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    
    // 重置状态
    m_isDrawing = false;
    m_controlPoints.clear();
    
    if (m_currentPath) {
        delete m_currentPath;
        m_currentPath = nullptr;
    }
    
    m_currentItem = nullptr;
    
    // 触发场景重绘
    if (m_scene) {
        m_scene->update();
    }
}