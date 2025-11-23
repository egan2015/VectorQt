#include <QDebug>
#include "../tools/node-handle-manager.h"
#include "../ui/drawingscene.h"
#include "../core/drawing-shape.h"

// 静态常量定义
const qreal NodeHandleManager::DEFAULT_HANDLE_SIZE = 8.0;
const QColor NodeHandleManager::CORNER_RADIUS_COLOR = QColor(255, 165, 0, 200);  // 橙色
const QColor NodeHandleManager::SIZE_CONTROL_COLOR = QColor(70, 130, 180, 200);  // 钢蓝色
const QColor NodeHandleManager::PATH_NODE_COLOR = QColor(50, 205, 50, 200);     // 青绿色
const QColor NodeHandleManager::PATH_CONTROL_COLOR = QColor(238, 130, 238, 200); // 紫罗兰色

NodeHandleManager::NodeHandleManager(DrawingScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_currentShape(nullptr)
    , m_activeHandle(nullptr)
    , m_handlesVisible(false)
{
}

NodeHandleManager::~NodeHandleManager()
{
    clearHandles();
}

void NodeHandleManager::updateHandles(DrawingShape *shape)
{
    if (!shape || !m_scene) {
        clearHandles();
        return;
    }
    
    // 如果图形发生变化，清除旧手柄并重新创建
    if (m_currentShape != shape) {
        clearHandles();
        m_currentShape = shape;
        
        // 根据图形类型创建相应的手柄
        switch (shape->shapeType()) {
        case DrawingShape::Rectangle:
            createHandlesForRectangle(static_cast<DrawingRectangle*>(shape));
            break;
        case DrawingShape::Ellipse:
            createHandlesForEllipse(static_cast<DrawingEllipse*>(shape));
            break;
        case DrawingShape::Path:
            createHandlesForPath(static_cast<DrawingPath*>(shape));
            break;
        default:
            // 对于其他类型的图形，创建通用手柄
            createCustomNodeHandles(shape);
            break;
        }
        
        if (m_handlesVisible) {
            showHandles();
        }
    } else {
        // 图形没有变化，只更新现有手柄的位置
        updateExistingHandlePositions(shape);
    }
}

void NodeHandleManager::clearHandles()
{
    // 清除所有手柄
    for (const NodeHandleInfo &info : m_handleInfos) {
        if (info.handle && info.handle->scene()) {
            m_scene->removeItem(info.handle);
            delete info.handle;
        }
    }
    
    m_handleInfos.clear();
    m_activeHandle = nullptr;
    m_currentShape = nullptr;
}

CustomHandleItem* NodeHandleManager::getHandleAt(const QPointF &scenePos) const
{
    for (const NodeHandleInfo &info : m_handleInfos) {
        if (info.handle && info.handle->contains(info.handle->mapFromScene(scenePos))) {
            return info.handle;
        }
    }
    return nullptr;
}

NodeHandleManager::NodeHandleInfo NodeHandleManager::getHandleInfo(CustomHandleItem *handle) const
{
    for (const NodeHandleInfo &info : m_handleInfos) {
        if (info.handle == handle) {
            return info;
        }
    }
    return NodeHandleInfo();
}

void NodeHandleManager::showHandles()
{
    m_handlesVisible = true;
    for (const NodeHandleInfo &info : m_handleInfos) {
        if (info.handle) {
            info.handle->setVisible(true);
        }
    }
}

void NodeHandleManager::hideHandles()
{
    m_handlesVisible = false;
    for (const NodeHandleInfo &info : m_handleInfos) {
        if (info.handle) {
            info.handle->setVisible(false);
        }
    }
}

void NodeHandleManager::setActiveHandle(CustomHandleItem *handle)
{
    // 重置之前的活动手柄
    if (m_activeHandle) {
        m_activeHandle->setState(HandleItemBase::Normal);
    }
    
    // 设置新的活动手柄
    m_activeHandle = handle;
    if (m_activeHandle) {
        m_activeHandle->setState(HandleItemBase::Active);
    }
}

void NodeHandleManager::updateHandlePosition(CustomHandleItem *handle, const QPointF &newPos)
{
    if (!handle) return;
    
    // 更新手柄位置
    handle->setPos(newPos);
    
    // 更新手柄信息中的原始位置
    for (NodeHandleInfo &info : m_handleInfos) {
        if (info.handle == handle) {
            info.originalPos = newPos;
            break;
        }
    }
}

void NodeHandleManager::applyHandleChanges()
{
    if (!m_currentShape) return;
    
    // 根据手柄类型应用变化到图形
    for (const NodeHandleInfo &info : m_handleInfos) {
        if (!info.handle) continue;
        
        QPointF localPos = m_currentShape->mapFromScene(info.handle->pos());
        
        switch (info.type) {
        case CornerRadiusHandle:
            if (m_currentShape->shapeType() == DrawingShape::Rectangle && info.nodeIndex == 0) {
                DrawingRectangle *rect = static_cast<DrawingRectangle*>(m_currentShape);
                if (rect) {
                    // 根据拖动距离计算圆角半径
                    QPointF center = rect->localBounds().center();
                    qreal distance = QLineF(center, localPos).length();
                    rect->setCornerRadius(distance);
                }
            }
            break;
            
        case SizeControlHandle:
            // 尺寸控制手柄：更新图形尺寸
            if (m_currentShape->shapeType() == DrawingShape::Rectangle) {
                DrawingRectangle *rect = static_cast<DrawingRectangle*>(m_currentShape);
                if (rect) {
                    QRectF bounds = rect->localBounds();
                    // 根据手柄索引更新相应的边界点
                    if (info.nodeIndex == 1) {
                        bounds.setBottomRight(localPos);
                    }
                    rect->setRectangle(bounds);
                }
            } else if (m_currentShape->shapeType() == DrawingShape::Ellipse) {
                DrawingEllipse *ellipse = static_cast<DrawingEllipse*>(m_currentShape);
                if (ellipse) {
                    QRectF bounds = ellipse->localBounds();
                    bounds.setBottomRight(localPos);
                    ellipse->setEllipse(bounds);
                }
            }
            break;
            
        case PathNodeHandle:
            // 路径节点手柄：更新路径节点位置
            if (m_currentShape->shapeType() == DrawingShape::Path) {
                DrawingPath *path = static_cast<DrawingPath*>(m_currentShape);
                if (path) {
                    path->setNodePoint(info.nodeIndex, localPos);
                }
            }
            break;
            
        case PathControlHandle:
        // 路径控制点手柄：更新贝塞尔曲线控制点
        if (m_currentShape->shapeType() == DrawingShape::Path) {
            DrawingPath *path = static_cast<DrawingPath*>(m_currentShape);
            if (path) {
                // TODO: 实现路径控制点设置方法
                // path->setControlPoint(info.nodeIndex, localPos);
                path->setNodePoint(info.nodeIndex, localPos);
            }
        }
        break;
            
        default:
            // 自定义手柄：通用节点位置更新
            m_currentShape->setNodePoint(info.nodeIndex, localPos);
            break;
        }
    }
}

CustomHandleItem* NodeHandleManager::createCornerRadiusHandle(const QPointF &pos)
{
    CustomHandleItem *handle = new CustomHandleItem(TransformHandle::Center);
    handle->setSize(DEFAULT_HANDLE_SIZE * 1.2);  // 稍大一点
    handle->setStyle(HandleItemBase::Circle);
    handle->setSpecificColor(CORNER_RADIUS_COLOR);
    handle->setPos(pos);
    
    if (m_scene) {
        m_scene->addItem(handle);
    }
    
    return handle;
}

CustomHandleItem* NodeHandleManager::createSizeControlHandle(const QPointF &pos)
{
    CustomHandleItem *handle = new CustomHandleItem(TransformHandle::TopRight);
    handle->setSize(DEFAULT_HANDLE_SIZE);
    handle->setStyle(HandleItemBase::Square);
    handle->setSpecificColor(SIZE_CONTROL_COLOR);
    handle->setPos(pos);
    
    if (m_scene) {
        m_scene->addItem(handle);
    }
    
    return handle;
}

CustomHandleItem* NodeHandleManager::createPathNodeHandle(const QPointF &pos)
{
    CustomHandleItem *handle = new CustomHandleItem(TransformHandle::TopLeft);
    handle->setSize(DEFAULT_HANDLE_SIZE);
    handle->setStyle(HandleItemBase::Diamond);
    handle->setSpecificColor(PATH_NODE_COLOR);
    handle->setPos(pos);
    
    if (m_scene) {
        m_scene->addItem(handle);
    }
    
    return handle;
}

CustomHandleItem* NodeHandleManager::createPathControlHandle(const QPointF &pos)
{
    CustomHandleItem *handle = new CustomHandleItem(TransformHandle::BottomRight);
    handle->setSize(DEFAULT_HANDLE_SIZE * 0.8);  // 稍小一点
    handle->setStyle(HandleItemBase::Circle);
    handle->setSpecificColor(PATH_CONTROL_COLOR);
    handle->setPos(pos);
    
    if (m_scene) {
        m_scene->addItem(handle);
    }
    
    return handle;
}

CustomHandleItem* NodeHandleManager::createCustomNodeHandle(const QPointF &pos, int index)
{
    CustomHandleItem *handle = new CustomHandleItem(TransformHandle::None);
    handle->setSize(DEFAULT_HANDLE_SIZE);
    handle->setStyle(HandleItemBase::Square);
    handle->setPos(pos);
    
    if (m_scene) {
        m_scene->addItem(handle);
    }
    
    return handle;
}

void NodeHandleManager::createHandlesForRectangle(DrawingRectangle *rect)
{
    if (!rect) return;
    
    // 获取矩形的节点点
    QVector<QPointF> nodePoints = rect->getNodePoints();
    
    for (int i = 0; i < nodePoints.size(); ++i) {
        QPointF scenePos = calculateHandlePosition(nodePoints[i], rect);
        
        NodeHandleInfo info;
        info.nodeIndex = i;
        info.originalPos = scenePos;
        
        if (i == 0) {
            // 第一个点是圆角控制
            info.type = CornerRadiusHandle;
            info.handle = createCornerRadiusHandle(scenePos);
        } else {
            // 其他点是尺寸控制
            info.type = SizeControlHandle;
            info.handle = createSizeControlHandle(scenePos);
        }
        
        m_handleInfos.append(info);
    }
}

void NodeHandleManager::createHandlesForEllipse(DrawingEllipse *ellipse)
{
    if (!ellipse) return;
    
    // 获取椭圆的节点点
    QVector<QPointF> nodePoints = ellipse->getNodePoints();
    
    for (int i = 0; i < nodePoints.size(); ++i) {
        QPointF scenePos = calculateHandlePosition(nodePoints[i], ellipse);
        
        NodeHandleInfo info;
        info.nodeIndex = i;
        info.originalPos = scenePos;
        
        // 根据索引设置不同类型的手柄
        // 0: 右边中点 - 宽度控制（方形）
        // 1: 下边中点 - 高度控制（方形）  
        // 2: 左边中点 - 弧度控制（圆形）
        // 3: 上边中点 - 弧度控制（圆形）
        if (i == 2 || i == 3) {
            // 弧度控制手柄 - 圆形，使用弧度颜色
            info.type = PathControlHandle;
            info.handle = createPathControlHandle(scenePos);
        } else {
            // 尺寸控制手柄 - 方形，使用尺寸控制颜色
            info.type = SizeControlHandle;
            info.handle = createSizeControlHandle(scenePos);
        }
        
        m_handleInfos.append(info);
    }
}

void NodeHandleManager::createHandlesForPath(DrawingPath *path)
{
    if (!path) return;
    
    // 路径对象自己处理节点显示，这里不创建手柄
    // 保持与原实现一致
    return;
}

void NodeHandleManager::createCustomNodeHandles(DrawingShape *shape)
{
    if (!shape) return;
    
    // 获取图形的节点点
    QVector<QPointF> nodePoints = shape->getNodePoints();
    
    for (int i = 0; i < nodePoints.size(); ++i) {
        QPointF scenePos = calculateHandlePosition(nodePoints[i], shape);
        
        NodeHandleInfo info;
        info.type = CustomNodeHandle;
        info.nodeIndex = i;
        info.originalPos = scenePos;
        info.handle = createCustomNodeHandle(scenePos, i);
        
        m_handleInfos.append(info);
    }
}

QPointF NodeHandleManager::calculateHandlePosition(const QPointF &localPoint, DrawingShape *shape) const
{
    if (!shape) return localPoint;
    
    // 应用图形变换
    QTransform transform = shape->transform();
    QPointF transformedPoint = transform.map(localPoint);
    
    // 转换为场景坐标
    return shape->mapToScene(transformedPoint);
}

void NodeHandleManager::setupHandleStyle(CustomHandleItem *handle, NodeHandleType type)
{
    if (!handle) return;
    
    switch (type) {
    case CornerRadiusHandle:
        handle->setStyle(HandleItemBase::Circle);
        handle->setSpecificColor(CORNER_RADIUS_COLOR);
        break;
    case SizeControlHandle:
        handle->setStyle(HandleItemBase::Square);
        handle->setSpecificColor(SIZE_CONTROL_COLOR);
        break;
    case PathNodeHandle:
        handle->setStyle(HandleItemBase::Diamond);
        handle->setSpecificColor(PATH_NODE_COLOR);
        break;
    case PathControlHandle:
        handle->setStyle(HandleItemBase::Circle);
        handle->setSpecificColor(PATH_CONTROL_COLOR);
        break;
    default:
        handle->setStyle(HandleItemBase::Square);
        break;
    }
    
    handle->setSize(DEFAULT_HANDLE_SIZE);
}

void NodeHandleManager::updateExistingHandlePositions(DrawingShape *shape)
{
    if (!shape || m_handleInfos.isEmpty()) return;
    
    // 获取图形的当前节点点
    QVector<QPointF> nodePoints = shape->getNodePoints();
    
    // 对于椭圆和撤销操作，需要更新所有手柄
    bool updateAllHandles = (shape->shapeType() == DrawingShape::Ellipse) || (shape->shapeType() == DrawingShape::Polyline) || (shape->shapeType() == DrawingShape::Polygon);
    
    // 更新每个手柄的位置
    for (NodeHandleInfo &info : m_handleInfos) {
        if (!info.handle) continue;
        
        // 如果不是特殊图形且是活动手柄，跳过（因为它由鼠标直接控制）
        if (!updateAllHandles && info.handle == m_activeHandle) continue;
        
        if (info.nodeIndex < nodePoints.size()) {
            QPointF newScenePos = calculateHandlePosition(nodePoints[info.nodeIndex], shape);
            info.handle->setPos(newScenePos);
            info.originalPos = newScenePos;
        }
    }
}