#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include "../tools/drawing-tool-bezier.h"
#include "../ui/drawingscene.h"
#include "../core/drawing-shape.h"
#include "../ui/snap-manager.h"


DrawingBezierTool::DrawingBezierTool(QObject *parent)
    : ToolBase(parent)
    , m_currentPath(nullptr)
    , m_isDrawing(false)
    , m_currentItem(nullptr)
    , m_previewItem(nullptr)
{
    // 贝塞尔工具不需要对象池，直接创建对象更简单高效
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
        if (m_scene) {
            m_scene->removeItem(m_previewItem);
        }
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
            SnapResult gridSnap = drawingScene->snapManager()->smartAlignToGrid(scenePos);
            alignedPos = gridSnap.snappedPos;
            
            // 尝试对象吸附
            ObjectSnapResult objectSnap = drawingScene->snapManager()->snapToObjects(scenePos);
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
            SnapResult gridSnap = drawingScene->snapManager()->smartAlignToGrid(scenePos);
            alignedPos = gridSnap.snappedPos;
            
            // 尝试对象吸附
            ObjectSnapResult objectSnap = drawingScene->snapManager()->snapToObjects(scenePos);
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

bool DrawingBezierTool::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_isDrawing) {
        // 双击完成绘制
        finishDrawing();
        
        // 触发场景重绘
        if (m_scene) {
            m_scene->update();
        }
        
        return true; // 事件被消费
    }
    
    return ToolBase::mouseDoubleClickEvent(event, scenePos);
}

void DrawingBezierTool::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    qDebug() << "Bezier tool activated, scene:" << scene << "view:" << view;
    
    // 检查场景和视图的有效性
    if (!scene) {
        qDebug() << "ERROR: scene is null!";
        return;
    }
    
    if (!view) {
        qDebug() << "ERROR: view is null!";
        return;
    }
    
    qDebug() << "Scene item count:" << scene->items().count();
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
    
    // 根据控制点数量智能创建贝塞尔曲线
    for (int i = 1; i < m_controlPoints.size(); ) {
        if (i + 2 < m_controlPoints.size()) {
            // 有足够的点，创建三次贝塞尔曲线
            m_currentPath->cubicTo(m_controlPoints[i], m_controlPoints[i+1], m_controlPoints[i+2]);
            i += 3; // 跳过已经处理的两个控制点和终点，移动到下一个起点
        } else if (i + 1 < m_controlPoints.size()) {
            // 有足够的点，创建二次贝塞尔曲线
            m_currentPath->quadTo(m_controlPoints[i], m_controlPoints[i+1]);
            i += 2; // 跳过已经处理的一个控制点和终点，移动到下一个起点
        } else {
            // 只有一个点，创建直线
            m_currentPath->lineTo(m_controlPoints[i]);
            i += 1; // 移动到下一个点
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
                alignedPath.moveTo(drawingScene->snapManager()->alignToGrid(m_controlPoints.first()));
                
                for (int i = 1; i < m_controlPoints.size(); ) {
                    if (i + 2 < m_controlPoints.size()) {
                        // 有足够的点创建三次贝塞尔曲线
                        QPointF p1 = drawingScene->snapManager() ? drawingScene->snapManager()->alignToGrid(m_controlPoints[i]) : m_controlPoints[i];
                        QPointF p2 = drawingScene->snapManager() ? drawingScene->snapManager()->alignToGrid(m_controlPoints[i+1]) : m_controlPoints[i+1];
                        QPointF p3 = drawingScene->snapManager() ? drawingScene->snapManager()->alignToGrid(m_controlPoints[i+2]) : m_controlPoints[i+2];
                        alignedPath.cubicTo(p1, p2, p3);
                        i += 3; // 跳过已经处理的两个控制点和终点
                    } else if (i + 1 < m_controlPoints.size()) {
                        // 有足够的点创建二次贝塞尔曲线
                        QPointF p1 = drawingScene->snapManager() ? drawingScene->snapManager()->alignToGrid(m_controlPoints[i]) : m_controlPoints[i];
                        QPointF p2 = drawingScene->snapManager() ? drawingScene->snapManager()->alignToGrid(m_controlPoints[i+1]) : m_controlPoints[i+1];
                        alignedPath.quadTo(p1, p2);
                        i += 2; // 跳过已经处理的一个控制点和终点
                    } else {
                        // 只有一个点，创建直线
                        QPointF p1 = drawingScene->snapManager() ? drawingScene->snapManager()->alignToGrid(m_controlPoints[i]) : m_controlPoints[i];
                        alignedPath.lineTo(p1);
                        i += 1; // 移动到下一个点
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
        
        // 检查并移除所有重叠的节点
        QVector<QPointF> cleanedControlPoints = m_controlPoints;
        const qreal overlapThreshold = 2.0; // 重叠阈值（像素）
        
        // 从后向前遍历，移除所有重叠的相邻点
        for (int i = cleanedControlPoints.size() - 1; i > 0; i--) {
            QPointF currentPoint = cleanedControlPoints[i];
            QPointF previousPoint = cleanedControlPoints[i - 1];
            
            // 如果两个相邻点距离小于阈值，移除当前点
            if (QLineF(currentPoint, previousPoint).length() < overlapThreshold) {
                cleanedControlPoints.removeAt(i);
                qDebug() << "Removed overlapping point at index" << i << ", remaining points:" << cleanedControlPoints.size();
            }
        }
        
        // 保存清理后的控制点以便后续编辑
        static_cast<DrawingPath*>(m_currentItem)->setControlPoints(cleanedControlPoints);
        
        // 将图形添加到撤销栈，AddItemCommand会处理addItem
        if (m_scene) {
            m_scene->setModified(true);
            
            // 使用DrawingScene中的AddItemCommand
            class AddItemCommand : public QUndoCommand
            {
            public:
                AddItemCommand(DrawingScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
                    : QUndoCommand("添加贝塞尔曲线", parent), m_scene(scene), m_item(item) {}
                
                void undo() override {
                    m_scene->removeItem(m_item);
                    m_item->setVisible(false);
                    // 强制通知所有工具清理手柄（针对撤销删除操作的特殊处理）
                    emit m_scene->allToolsClearHandles();
                }
                
                void redo() override {
                    m_scene->addItem(m_item);
                    m_item->setVisible(true);
                    
                    // 自动选中新创建的图形
                    m_item->setSelected(true);
                    
                    // 清除其他选中项
                    for (QGraphicsItem *item : m_scene->selectedItems()) {
                        if (item != m_item) {
                            item->setSelected(false);
                        }
                    }
                }
                
            private:
                DrawingScene *m_scene;
                QGraphicsItem *m_item;
            };
            
            // 创建并推送撤销命令
            AddItemCommand *command = new AddItemCommand(m_scene, m_currentItem);
            m_scene->executeCommand(command);
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
    
    // 重要：将所有权转移给场景，不再由工具管理
        m_currentItem = nullptr;
    
    // 触发场景重绘
    if (m_scene) {
        m_scene->update();
    }
}