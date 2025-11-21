#include "drawing-tool-node-edit.h"
#include "drawingscene.h"
#include "drawing-shape.h"
#include "node-handle-manager.h"
#include "handle-item.h"
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QDebug>

// NodeEditCommand 实现
NodeEditCommand::NodeEditCommand(DrawingScene *scene, DrawingShape *shape, int nodeIndex,
                                 const QPointF &oldPos, const QPointF &newPos,
                                 qreal oldCornerRadius, qreal newCornerRadius, QUndoCommand *parent)
    : QUndoCommand("编辑节点", parent), m_scene(scene), m_shape(shape), m_nodeIndex(nodeIndex),
      m_oldPos(oldPos), m_newPos(newPos), m_oldCornerRadius(oldCornerRadius), m_newCornerRadius(newCornerRadius)
{
}

void NodeEditCommand::undo()
{
    if (m_shape && m_shape->scene() == m_scene)
    {
        // 恢复到旧位置
        m_shape->setNodePoint(m_nodeIndex, m_oldPos);

        // 如果是矩形且编辑的是圆角，恢复圆角半径（在setNodePoint之后）
        if (m_oldCornerRadius >= 0.0 && m_shape->shapeType() == DrawingShape::Rectangle && m_nodeIndex == 0)
        {
            DrawingRectangle *rect = static_cast<DrawingRectangle *>(m_shape);
            if (rect)
            {
                rect->setCornerRadius(m_oldCornerRadius);
            }
        }

        // 更新场景
        if (m_scene)
        {
            m_scene->update();
            // 通知所有工具对象状态已变化
            emit m_scene->objectStateChanged(m_shape);
        }
    }
}

void NodeEditCommand::redo()
{
    // 什么都不做！状态已经在拖动过程中设置好了
    // 避免双重设置导致的位置跳跃
}

DrawingNodeEditTool::DrawingNodeEditTool(QObject *parent)
    : ToolBase(parent)
    , m_selectedShape(nullptr)
    , m_activeHandle(nullptr)
    , m_dragging(false)
    , m_handleManager(nullptr)
{
}

DrawingNodeEditTool::~DrawingNodeEditTool()
{
    delete m_handleManager;
    m_handleManager = nullptr;
}

bool DrawingNodeEditTool::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene)
        return false;

    if (event->button() == Qt::LeftButton)
    {
        // 检查是否点击了节点手柄
        CustomHandleItem *handle = nullptr;
        if (m_handleManager) {
            handle = m_handleManager->getHandleAt(scenePos);
        }

        if (handle)
        {
            // 点击了节点手柄，开始拖动
            m_activeHandle = handle;
            m_dragging = true;
            m_dragStartPos = scenePos;
            
            // 设置活动手柄
            if (m_handleManager) {
                m_handleManager->setActiveHandle(handle);
            }

            // 获取手柄信息
            NodeHandleManager::NodeHandleInfo handleInfo;
            if (m_handleManager) {
                handleInfo = m_handleManager->getHandleInfo(handle);
            }

            if (!handleInfo.handle)
            {
                return false;
            }

            // 通知图形开始拖动节点
            if (m_selectedShape)
            {
                m_selectedShape->beginNodeDrag(handleInfo.nodeIndex);

                // 保存原始值用于撤销
                QVector<QPointF> nodePoints = m_selectedShape->getNodePoints();
                if (handleInfo.nodeIndex < nodePoints.size())
                {
                    m_originalValue = m_selectedShape->mapToScene(nodePoints[handleInfo.nodeIndex]);

                    // 如果是矩形且编辑的是圆角（第一个节点），保存原始圆角半径
                    m_originalCornerRadius = -1.0;
                    if (m_selectedShape->shapeType() == DrawingShape::Rectangle && handleInfo.nodeIndex == 0)
                    {
                        DrawingRectangle *rect = static_cast<DrawingRectangle *>(m_selectedShape);
                        if (rect)
                        {
                            m_originalCornerRadius = rect->cornerRadius();
                        }
                    }
                }
            }

            return true;
        }
        else
        {
            // 点击了空白区域或其他对象，清除当前节点手柄
            clearNodeHandles();

            // 检查是否点击了图形
            QGraphicsItem *clickedItem = m_scene->itemAt(scenePos, QTransform());

            // 跳过DrawingLayer
            if (clickedItem && clickedItem->type() == QGraphicsItem::UserType + 100)
            {
                // qDebug() << "Node edit tool: clicked on DrawingLayer, skipping";
                return false;
            }

            DrawingShape *shape = dynamic_cast<DrawingShape *>(clickedItem);

            if (shape)
            {
                // 如果之前有选中的图形且是路径类型，隐藏其控制点连线
                if (m_selectedShape)
                {
                    if (m_selectedShape && m_selectedShape->shapeType() == DrawingShape::Path)
                    {
                        DrawingPath *oldPath = static_cast<DrawingPath *>(m_selectedShape);
                        oldPath->setShowControlPolygon(false);
                    }
                }

                // 禁用之前图形的几何变换手柄
                if (m_selectedShape)
                {
                    m_selectedShape->setEditHandlesEnabled(false);
                }

                // 选中新的图形
                m_selectedShape = shape;

                // 隐藏选择边框，只显示节点手柄
                shape->setShowSelectionIndicator(false);

                // 禁用几何变换手柄（因为我们要显示节点手柄）
                shape->setEditHandlesEnabled(false);

                // 在节点编辑状态下，禁用图形的移动功能
                shape->setFlag(QGraphicsItem::ItemIsMovable, false);

                // 如果是路径类型，启用控制点连线显示，并让路径自己处理控制点
                if (shape->shapeType() == DrawingShape::Path)
                {
                    DrawingPath *path = static_cast<DrawingPath *>(shape);
                    path->setShowControlPolygon(true);
                    // 不创建手柄，让路径自己处理控制点
                    return false; // 让事件传播到DrawingPath
                }

                updateNodeHandles();
                return true;
            }
            else
            {
                // 点击了空白区域，清除选择
                if (m_selectedShape)
                {
                    // 如果之前选中的图形是路径类型，隐藏其控制点连线
                    if (m_selectedShape->shapeType() == DrawingShape::Path)
                    {
                        DrawingPath *path = static_cast<DrawingPath *>(m_selectedShape);
                        path->setShowControlPolygon(false);
                    }
                    // 恢复图形的移动功能
                    m_selectedShape->setFlag(QGraphicsItem::ItemIsMovable, true);
                    // 取消图形的选择状态，但不显示选择边框
                    m_selectedShape->setSelected(false);
                    m_selectedShape = nullptr;
                }

                // 清除场景选择
                m_scene->clearSelection();
            }
        }
    }

    // 在节点编辑状态下，不调用父类的mousePressEvent，以防止图形被移动
    return false;
}

bool DrawingNodeEditTool::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_dragging && m_activeHandle && m_selectedShape)
    {
        // 检查图形是否仍然有效
        if (!m_selectedShape->scene())
        {
            // qDebug() << "Shape is no longer in scene during drag, stopping drag";
            m_dragging = false;
            m_activeHandle = nullptr;
            clearNodeHandles();
            m_selectedShape = nullptr;
            return false;
        }
        // 获取手柄信息
        NodeHandleManager::NodeHandleInfo handleInfo;
        if (m_handleManager) {
            handleInfo = m_handleManager->getHandleInfo(m_activeHandle);
        }

        if (!handleInfo.handle)
        {
            return false;
        }

        // 应用网格对齐（如果启用）
        QPointF alignedScenePos = scenePos;
        if (m_scene)
        {
            DrawingScene *drawingScene = qobject_cast<DrawingScene *>(m_scene);
            if (drawingScene && drawingScene->isGridAlignmentEnabled())
            {
                // 使用智能网格吸附
                DrawingScene::SnapResult gridSnap = drawingScene->smartAlignToGrid(scenePos);
                alignedScenePos = gridSnap.snappedPos;

                // 尝试对象吸附
                DrawingScene::ObjectSnapResult objectSnap = drawingScene->snapToObjects(scenePos, m_selectedShape);
                if (objectSnap.snappedToObject)
                {
                    // 对象吸附优先级更高
                    alignedScenePos = objectSnap.snappedPos;
                }
            }
        }

        // 获取图形的变换
        DrawingTransform transform = m_selectedShape->transform();

        // 直接传递场景坐标给setNodePoint，让图形自己处理坐标转换
        m_selectedShape->setNodePoint(handleInfo.nodeIndex, alignedScenePos);

        // 更新正在拖动的手柄位置到鼠标位置（确保视觉上跟随鼠标）
        if (m_handleManager) {
            m_handleManager->updateHandlePosition(m_activeHandle, alignedScenePos);
            
            // 对于椭圆等图形，需要更新所有相关手柄的位置
            if (m_selectedShape->shapeType() == DrawingShape::Ellipse) {
                m_handleManager->updateExistingHandlePositions(m_selectedShape);
            }
        }

        // 触发场景重绘
        if (m_scene)
        {
            m_scene->update();
        }

        // 触发场景重绘
        if (m_scene)
        {
            m_scene->update();
        }

        return true;
    }

    

    // 在节点编辑状态下，不调用父类的mouseMoveEvent，以防止图形被移动
    return false;
}

bool DrawingNodeEditTool::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_dragging)
    {
        // 获取手柄信息
        NodeHandleManager::NodeHandleInfo handleInfo;
        if (m_handleManager) {
            handleInfo = m_handleManager->getHandleInfo(m_activeHandle);
        }

        // 创建撤销命令
        if (m_selectedShape && handleInfo.handle && m_scene)
        {
            // 获取当前节点位置
            QVector<QPointF> nodePoints = m_selectedShape->getNodePoints();
            if (handleInfo.nodeIndex < nodePoints.size())
            {
                QPointF currentPos = m_selectedShape->mapToScene(nodePoints[handleInfo.nodeIndex]);

                // 只有当位置真正发生变化时才创建撤销命令
                if (currentPos != m_originalValue)
                {
                    qreal oldCornerRadius = -1.0;
                    qreal newCornerRadius = -1.0;

                    // 如果是矩形且编辑的是圆角（第一个节点），保存圆角信息
                    if (m_selectedShape->shapeType() == DrawingShape::Rectangle && handleInfo.nodeIndex == 0)
                    {
                        oldCornerRadius = m_originalCornerRadius;

                        // 计算新的圆角半径
                        DrawingRectangle *rect = static_cast<DrawingRectangle *>(m_selectedShape);
                        if (rect)
                        {
                            QPointF localPos = m_selectedShape->mapFromScene(currentPos);
                            DrawingTransform transform = m_selectedShape->transform();
                            localPos = transform.transform().inverted().map(localPos);

                            qreal distance = localPos.x() - rect->rectangle().left();
                            qreal maxRadius = qMin(rect->rectangle().width(), rect->rectangle().height()) / 2.0;
                            newCornerRadius = qBound(0.0, distance, maxRadius);
                        }
                    }

                    // 简单方案：redo什么都不做，直接传递场景坐标
                    NodeEditCommand *command = new NodeEditCommand(m_scene, m_selectedShape,
                                                                   handleInfo.nodeIndex, m_originalValue, currentPos,
                                                                   oldCornerRadius, newCornerRadius);
                    m_scene->undoStack()->push(command);
                }
            }

            // 通知图形结束拖动节点
            m_selectedShape->endNodeDrag(handleInfo.nodeIndex);
        }

        // 结束拖动
        m_dragging = false;
        m_activeHandle = nullptr;

        // 如果有选中的图形，更新其显示
        if (m_selectedShape)
        {
            m_selectedShape->update();
        }

        return true;
    }

    // 在节点编辑状态下，不调用父类的mouseReleaseEvent，以防止图形被移动
    return false;
}

void DrawingNodeEditTool::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    // qDebug() << "Node edit tool activated";

    // 创建手柄管理器
    if (!m_handleManager) {
        m_handleManager = new NodeHandleManager(scene, this);
    }

    // 清除之前选中的图形
    m_selectedShape = nullptr;

    // 在节点编辑状态下，禁用场景中所有图形的移动功能
    if (m_scene)
    {
        QList<QGraphicsItem *> allItems = m_scene->items();
        for (QGraphicsItem *item : allItems)
        {
            DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
            if (shape)
            {
                // 确保图形的移动功能被禁用
                shape->setFlag(QGraphicsItem::ItemIsMovable, false);
                // 同时禁用编辑手柄，避免显示选择工具的手柄
                shape->setEditHandlesEnabled(false);
            }
        }

        // 检查场景中是否有选中的图形
        QList<QGraphicsItem *> selectedItems = m_scene->selectedItems();
        // qDebug() << "Node edit tool: checking selected items, count:" << selectedItems.size();

        for (QGraphicsItem *item : selectedItems)
        {
            // 跳过DrawingLayer
            if (item->type() == QGraphicsItem::UserType + 100)
            {
                // qDebug() << "Node edit tool: skipping DrawingLayer item";
                continue;
            }

            DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
            if (shape)
            { // 处理所有选中的图形
                // qDebug() << "Node edit tool: found selected shape, type:" << shape->shapeType();
                // 隐藏选择边框，只显示节点手柄
                shape->setShowSelectionIndicator(false);
                // 设置第一个选中的图形为当前编辑的图形
                m_selectedShape = shape;

                // 如果是路径类型，启用控制点连线显示
                if (shape->shapeType() == DrawingShape::Path)
                {
                    DrawingPath *path = static_cast<DrawingPath *>(shape);
                    path->setShowControlPolygon(true);
                }

                break;
            }
        }

        // 如果没有选中的图形，尝试获取场景中的第一个图形
        if (!m_selectedShape && !m_scene->items().isEmpty())
        {
            QList<QGraphicsItem *> allItems = m_scene->items();
            for (QGraphicsItem *item : allItems)
            {
                // 跳过DrawingLayer
                if (item->type() == QGraphicsItem::UserType + 100)
                {
                    continue;
                }

                DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
                if (shape)
                {
                    // qDebug() << "Node edit tool: no selected shape, using first available shape, type:" << shape->shapeType();
                    // 选中这个图形
                    shape->setSelected(true);
                    m_selectedShape = shape;

                    // 隐藏选择边框，只显示节点手柄
                    shape->setShowSelectionIndicator(false);

                    // 如果是路径类型，启用控制点连线显示
                    if (shape->shapeType() == DrawingShape::Path)
                    {
                        DrawingPath *path = static_cast<DrawingPath *>(shape);
                        path->setShowControlPolygon(true);
                    }

                    break;
                }
            }
        }
    }

    // 如果有选中的图形，让图形自己处理节点显示
    // 路径已经在paintShape中绘制控制点，其他图形通过updateNodeHandles显示手柄
    if (m_selectedShape)
    {
        bool isPath = (m_selectedShape->shapeType() == DrawingShape::Path);
        // qDebug() << "Node edit tool: selected shape is path:" << (isPath ? "yes" : "no");
        if (!isPath)
        {
            // qDebug() << "Node edit tool: calling updateNodeHandles for non-path shape";
            updateNodeHandles();
        }
        else
        {
            // qDebug() << "Node edit tool: skipping updateNodeHandles for path shape";
        }
    }
    else
    {
        // qDebug() << "Node edit tool: no selected shape";
    }

    // 连接场景的选择变化信号，以便在选择变化时更新节点手柄
    if (m_scene)
    {
        connect(m_scene, &DrawingScene::selectionChanged, this, &DrawingNodeEditTool::onSceneSelectionChanged, Qt::UniqueConnection);
        // 连接对象状态变化信号，以便在撤销/重做时更新手柄位置
        connect(m_scene, &DrawingScene::objectStateChanged, this, &DrawingNodeEditTool::onObjectStateChanged, Qt::UniqueConnection);
    }
}

void DrawingNodeEditTool::deactivate()
{

    // 如果正在拖动，先结束拖动
    if (m_dragging && m_selectedShape)
    {
        // 结束任何正在进行的节点拖动
        NodeHandleManager::NodeHandleInfo handleInfo;
        if (m_activeHandle && m_handleManager)
        {
            handleInfo = m_handleManager->getHandleInfo(m_activeHandle);
        }

        if (handleInfo.handle)
        {
            m_selectedShape->endNodeDrag(handleInfo.nodeIndex);
        }

        // 结束拖动状态
        m_dragging = false;
        m_activeHandle = nullptr;
    }

    // 清除节点手柄
    clearNodeHandles();

    // 在退出节点编辑工具时，恢复场景中所有图形的移动功能
    if (m_scene)
    {
        QList<QGraphicsItem *> allItems = m_scene->items();
        for (QGraphicsItem *item : allItems)
        {
            DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
            if (shape)
            {
                shape->setFlag(QGraphicsItem::ItemIsMovable, true);
            }
        }
    }

    // 如果当前有编辑的图形，取消其选中状态
    if (m_selectedShape)
    {
        // 重新获取图形在场景中的状态以确保安全性
        DrawingShape *shape = m_selectedShape;

        // 检查图形是否仍然存在于场景中
        if (shape && shape->scene())
        {
            // 如果是路径类型，隐藏控制点连线
            if (shape->shapeType() == DrawingShape::Path)
            {
                DrawingPath *path = static_cast<DrawingPath *>(shape);
                if (path->showControlPolygon())
                {
                    path->setShowControlPolygon(false);
                }
            }

            // 取消图形的选中状态
            shape->setSelected(false);

            // 恢复选择边框的显示（为下次选择做准备）
            shape->setShowSelectionIndicator(true);

            // 禁用几何变换手柄（因为图形不再被选中）
            shape->setEditHandlesEnabled(false);
        }
    }

    // 断开场景的选择变化信号连接
    if (m_scene)
    {
        disconnect(m_scene, &DrawingScene::selectionChanged, this, &DrawingNodeEditTool::onSceneSelectionChanged);
        disconnect(m_scene, &DrawingScene::objectStateChanged, this, &DrawingNodeEditTool::onObjectStateChanged);
    }

    m_selectedShape = nullptr;
    m_activeHandle = nullptr;
    m_dragging = false;

    ToolBase::deactivate();
    // qDebug() << "Node edit tool deactivated";
}

void DrawingNodeEditTool::onSceneSelectionChanged()
{
    if (!m_scene)
        return;

    // 获取当前选中的项目
    QList<QGraphicsItem *> selectedItems = m_scene->selectedItems();

    // 检查是否有选中的图形
    DrawingShape *selectedShape = nullptr;
    for (QGraphicsItem *item : selectedItems)
    {
        DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
        if (shape)
        {
            selectedShape = shape;
            break;
        }
    }
    qDebug() << "Node edit tool: onSceneSelectionChanged, selected shape:" << (selectedShape ? "found" : "none");
    if (!selectedShape)
    {
        // 清除当前节点手柄
        clearNodeHandles();
        return;
    }
    // 如果选中的图形与当前编辑的图形不同
    if (selectedShape != m_selectedShape)
    {
        // 清除当前节点手柄
        clearNodeHandles();

        // 如果之前有选中的图形，隐藏其编辑状态但不恢复选择边框
        if (m_selectedShape)
        {
            // 如果是路径类型，隐藏控制点连线
            if (m_selectedShape->shapeType() == DrawingShape::Path)
            {
                DrawingPath *path = static_cast<DrawingPath *>(m_selectedShape);
                path->setShowControlPolygon(false);
            }

            // 不恢复选择边框和几何变换手柄，因为我们要保持图形的未选中状态
            // 取消图形的选择状态
            m_selectedShape->setSelected(false);
        }

        // 设置新的选中图形
        m_selectedShape = selectedShape;

        if (m_selectedShape)
        {
            // 隐藏选择边框，只显示节点手柄
            m_selectedShape->setShowSelectionIndicator(false);

            // 禁用几何变换手柄（因为我们要显示节点手柄）
            m_selectedShape->setEditHandlesEnabled(false);

            // 如果是路径类型，启用控制点连线显示
            if (m_selectedShape->shapeType() == DrawingShape::Path)
            {
                DrawingPath *path = static_cast<DrawingPath *>(m_selectedShape);
                path->setShowControlPolygon(true);
            }

            // 更新节点手柄
            updateNodeHandles();
        }
    }
    else if (m_selectedShape)
    {
        // 如果当前选中的图形没有变化，但可能其节点点发生了变化，更新手柄位置
        updateNodeHandles();
    }
}

void DrawingNodeEditTool::onObjectStateChanged(DrawingShape *shape)
{
    // 如果状态变化的图形是当前正在编辑的图形，更新手柄位置
    if (shape == m_selectedShape)
    {
        updateNodeHandles();
    }
}

void DrawingNodeEditTool::updateNodeHandles()
{
    if (!m_handleManager) return;
    
    m_handleManager->updateHandles(m_selectedShape);
    
    // 更新活动手柄
    if (m_activeHandle) {
        m_handleManager->setActiveHandle(m_activeHandle);
    }
}

void DrawingNodeEditTool::updateOtherNodeHandles(int draggedIndex, const QPointF &draggedPos)
{
    if (!m_selectedShape || !m_scene)
        return;

    // 获取图形的节点点和变换
    QVector<QPointF> nodePoints = m_selectedShape->getNodePoints();
    DrawingTransform transform = m_selectedShape->transform();

    // 使用手柄管理器更新所有手柄位置
    if (m_handleManager) {
        m_handleManager->updateHandles(m_selectedShape);
        
        // 重新设置活动手柄
        if (m_activeHandle) {
            m_handleManager->setActiveHandle(m_activeHandle);
        }
    }
}

void DrawingNodeEditTool::clearNodeHandles()
{
    if (m_handleManager) {
        m_handleManager->clearHandles();
    }
    m_activeHandle = nullptr;
}


