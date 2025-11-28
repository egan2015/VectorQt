#include <QDebug>
#include <QGraphicsLineItem>
#include "../tools/node-handle-manager.h"
#include "../ui/drawingscene.h"
#include "../core/drawing-shape.h"

// 静态常量定义
const qreal NodeHandleManager::DEFAULT_HANDLE_SIZE = 8.0;
const QColor NodeHandleManager::CORNER_RADIUS_COLOR = QColor(255, 165, 0, 200);  // 橙色
const QColor NodeHandleManager::SIZE_CONTROL_COLOR = QColor(70, 130, 180, 200);  // 钢蓝色
const QColor NodeHandleManager::PATH_NODE_COLOR = QColor(255, 255, 255, 230);     // 白色
const QColor NodeHandleManager::PATH_CONTROL_COLOR = QColor(255, 255, 255, 200); // 白色（稍微透明）
const QColor NodeHandleManager::BEZIER_NODE_COLOR = QColor(255, 255, 255, 230);    // 白色（节点）
const QColor NodeHandleManager::BEZIER_CONTROL_IN_COLOR = QColor(255, 255, 255, 200); // 白色（进入控制点）
const QColor NodeHandleManager::BEZIER_CONTROL_OUT_COLOR = QColor(255, 255, 255, 200); // 白色（离开控制点）

// 智能节点类型颜色配置
const QColor NodeHandleManager::CORNER_NODE_COLOR = QColor(255, 100, 100, 230);     // 红色 - 尖角节点
const QColor NodeHandleManager::SMOOTH_NODE_COLOR = QColor(100, 255, 100, 230);     // 绿色 - 平滑节点
const QColor NodeHandleManager::SYMMETRIC_NODE_COLOR = QColor(200, 100, 255, 230); // 紫色 - 对称节点
const QColor NodeHandleManager::CURVE_NODE_COLOR = QColor(255, 180, 100, 230);      // 橙色 - 曲线节点

NodeHandleManager::NodeHandleManager(DrawingScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_currentShape(nullptr)
    , m_activeHandle(nullptr)
    , m_handlesVisible(false)
{
    qDebug() << "Creating node handles...";
}

NodeHandleManager::~NodeHandleManager()
{
    qDebug() << "Destroying node handles...";
    clearHandles();
}

void NodeHandleManager::updateHandles(DrawingShape *shape)
{
    if (!shape || !m_scene) {
        clearHandles();
        return;
    }
    qDebug() << "Updating node handles for shape:" << shape;
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
    static bool inClearHandles = false;
    if (inClearHandles) {
        qDebug() << "WARNING: clearHandles() called recursively or concurrently!";
        return;
    }
    
    // 如果已经没有手柄，直接返回
    if (m_handleInfos.isEmpty()) {
        qDebug() << "clearHandles() called but no handles to clear";
        return;
    }
    
    inClearHandles = true;
    qDebug() << "Clearing node handles... count:" << m_handleInfos.size();
    // 清除所有手柄
    for (NodeHandleInfo &info : m_handleInfos) {
        if (info.handle) {
            if (m_scene && info.handle->scene()) {
                m_scene->removeItem(info.handle);
            }
            delete info.handle;
            info.handle = nullptr;  // 防止重复删除
        }
    }
    
    // 清除控制点连线
    clearPathControlLines();
    
    // 清除控制杆连线
    for (QGraphicsLineItem *line : m_controlArmLines) {
        if (line && line->scene()) {
            m_scene->removeItem(line);
            delete line;
        }
    }
    m_controlArmLines.clear();
    
    // 清除贝塞尔节点数据
    m_bezierNodes.clear();
    
    m_handleInfos.clear();
    m_activeHandle = nullptr;
    m_currentShape = nullptr;
    
    qDebug() << "clearHandles() completed";
    inClearHandles = false;
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
    
    // 如果是路径类型，实时更新控制点连线
    if (m_currentShape && m_currentShape->shapeType() == DrawingShape::Path) {
        updatePathControlLines();
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
            if (m_currentShape->shapeType() == DrawingShape::Rectangle) {
                DrawingRectangle *rect = static_cast<DrawingRectangle*>(m_currentShape);
                if (rect) {
                    QRectF bounds = rect->localBounds();
                    
                    if (info.nodeIndex == 10) {
                        // X方向圆角控制 - 参考qdraw的10号手柄逻辑
                        qreal x = localPos.x();
                        if (x < bounds.center().x()) 
                            x = bounds.center().x();
                        if (x > bounds.right()) 
                            x = bounds.right();
                        
                        qreal W = bounds.width();
                        if (W == 0) W = 1;
                        qreal ratioX = qAbs((bounds.right() - x)) / W;
                        rect->setCornerRadiusRatios(ratioX, rect->cornerRadiusRatioY());
                        
                        // 更新手柄位置到约束位置
                        QPointF constrainedPos = QPointF(x, bounds.top());
                        info.handle->setPos(calculateHandlePosition(constrainedPos, rect));
                        
                    } else if (info.nodeIndex == 9) {
                        // Y方向圆角控制 - 参考qdraw的9号手柄逻辑
                        qreal y = localPos.y();
                        if (y > bounds.center().y()) 
                            y = bounds.center().y();
                        if (y < bounds.top()) 
                            y = bounds.top();
                        
                        qreal H = bounds.height();
                        if (H == 0) H = 1;
                        qreal ratioY = qAbs((bounds.top() - y)) / H;
                        rect->setCornerRadiusRatios(rect->cornerRadiusRatioX(), ratioY);
                        
                        // 更新手柄位置到约束位置
                        QPointF constrainedPos = QPointF(bounds.right(), y);
                        info.handle->setPos(calculateHandlePosition(constrainedPos, rect));
                    }
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
    handle->setStyle(HandleItemBase::Square);  // 节点使用方形
    handle->setSpecificColor(BEZIER_NODE_COLOR);  // 深蓝色，统一贝塞尔节点颜色
    handle->setPos(pos);
    
    if (m_scene) {
        m_scene->addItem(handle);
    }
    
    return handle;
}

CustomHandleItem* NodeHandleManager::createPathControlHandle(const QPointF &pos)
{
    CustomHandleItem *handle = new CustomHandleItem(TransformHandle::BottomRight);
    handle->setSize(DEFAULT_HANDLE_SIZE * 0.8);  // 控制点稍小一点
    handle->setStyle(HandleItemBase::Circle);   // 控制点使用圆形
    handle->setSpecificColor(BEZIER_CONTROL_IN_COLOR);  // 蓝色，控制点颜色
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
    
    // 使用 getNodePoints 获取矩形的控制点位置
    QVector<QPointF> nodePoints = rect->getNodePoints();
    
    // 确保有3个节点点
    if (nodePoints.size() < 3) {
        return;
    }
    
    // 1. 左上角 - 尺寸控制（方形）
    QPointF topLeftPos = calculateHandlePosition(nodePoints[0], rect);
    NodeHandleInfo topLeftHandle;
    topLeftHandle.nodeIndex = 0;
    topLeftHandle.originalPos = topLeftPos;
    topLeftHandle.type = SizeControlHandle;
    topLeftHandle.handle = createSizeControlHandle(topLeftPos);
    m_handleInfos.append(topLeftHandle);
    
    // 2. 右下角 - 尺寸控制（方形）
    QPointF bottomRightPos = calculateHandlePosition(nodePoints[1], rect);
    NodeHandleInfo bottomRightHandle;
    bottomRightHandle.nodeIndex = 1;
    bottomRightHandle.originalPos = bottomRightPos;
    bottomRightHandle.type = SizeControlHandle;
    bottomRightHandle.handle = createSizeControlHandle(bottomRightPos);
    m_handleInfos.append(bottomRightHandle);
    
    // 3. 上边右半部分 - X轴圆角控制（圆形）
    QPointF radiusXPos = calculateHandlePosition(nodePoints[2], rect);
    NodeHandleInfo radiusXHandle;
    radiusXHandle.nodeIndex = 2;
    radiusXHandle.originalPos = radiusXPos;
    radiusXHandle.type = CornerRadiusHandle;
    radiusXHandle.handle = createCornerRadiusHandle(radiusXPos);
    m_handleInfos.append(radiusXHandle);
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
    
    // 强制禁用路径的自绘控制点，改用手柄系统
    path->setShowControlPolygon(false);
    path->update(); // 立即更新以确保禁用生效
    
    // 再次强制禁用，确保生效
    path->setShowControlPolygon(false);
    path->update();
    
    // 使用新的控制杆系统
    updateBezierControlArms(path);
    
    // 获取路径的控制点和类型
    QVector<QPointF> controlPoints = path->controlPoints();
    QVector<QPainterPath::ElementType> controlTypes = path->controlPointTypes();
    
    // 分析路径元素，创建相应的手柄
    for (int i = 0; i < controlPoints.size(); ++i) {
        QPointF scenePos = calculateHandlePosition(controlPoints[i], path);
        QPainterPath::ElementType elementType = (i < controlTypes.size()) ? controlTypes[i] : QPainterPath::MoveToElement;
        
        NodeHandleInfo info;
        info.nodeIndex = i;
        info.originalPos = scenePos;
        
        // 根据元素类型确定手柄类型
        if (elementType == QPainterPath::MoveToElement) {
            // 路径起点（移动到点）- 使用方形手柄
            info.type = PathNodeHandle;
            info.handle = createPathNodeHandle(scenePos);
        } else if (elementType == QPainterPath::LineToElement) {
            // 直线终点 - 使用方形手柄
            info.type = PathNodeHandle;
            info.handle = createPathNodeHandle(scenePos);
        } else if (elementType == QPainterPath::CurveToElement) {
            // 贝塞尔第一个控制点 - 使用圆形手柄
            info.type = PathControlHandle;
            info.handle = createPathControlHandle(scenePos);
        } else if (elementType == QPainterPath::CurveToDataElement) {
            // CurveToDataElement需要根据上下文判断
            // 查找前面的CurveToElement来确定当前在贝塞尔曲线中的位置
            bool isControlPoint = false;
            bool isEndPoint = false;
            
            // 向前查找最近的CurveToElement
            for (int j = i - 1; j >= 0; j--) {
                if (controlTypes[j] == QPainterPath::CurveToElement) {
                    // 找到了对应的CurveToElement
                    int curveToIndex = j;
                    int dataPointCount = i - curveToIndex; // 当前是第几个数据点
                    
                    if (dataPointCount == 1) {
                        // 第一个CurveToDataElement，是第二个控制点
                        isControlPoint = true;
                    } else if (dataPointCount == 2) {
                        // 第二个CurveToDataElement，是贝塞尔曲线的终点
                        isEndPoint = true;
                    }
                    break;
                } else if (controlTypes[j] == QPainterPath::MoveToElement || 
                          controlTypes[j] == QPainterPath::LineToElement) {
                    // 遇到了新的路径段开始，说明当前数据点不属于前面的贝塞尔曲线
                    // 这种情况下可能是二次贝塞尔或者其他特殊情况，当作终点处理
                    isEndPoint = true;
                    break;
                }
            }
            
            // 如果没有找到前面的CurveToElement，当作终点处理
            if (!isControlPoint && !isEndPoint) {
                isEndPoint = true;
            }
            
            if (isControlPoint) {
                // 贝塞尔控制点 - 使用圆形手柄
                info.type = PathControlHandle;
                info.handle = createPathControlHandle(scenePos);
            } else {
                // 贝塞尔终点 - 使用方形手柄
                info.type = PathNodeHandle;
                info.handle = createPathNodeHandle(scenePos);
            }
        } else {
            // 其他类型，默认为节点
            info.type = PathNodeHandle;
            info.handle = createPathNodeHandle(scenePos);
        }
        
        m_handleInfos.append(info);
    }
    
    // 重新启用控制点连线（虚线）
    updatePathControlLines();
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
        handle->setStyle(HandleItemBase::Square);  // 节点使用方形
        handle->setSpecificColor(BEZIER_NODE_COLOR);  // 使用统一的贝塞尔节点颜色
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
    
    // 对于椭圆、路径和撤销操作，需要更新所有手柄
    bool updateAllHandles = (shape->shapeType() == DrawingShape::Ellipse) || (shape->shapeType() == DrawingShape::Path) || (shape->shapeType() == DrawingShape::Polyline) || (shape->shapeType() == DrawingShape::Polygon);
    
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
    
    // 如果是路径，更新控制点连线
    if (shape->shapeType() == DrawingShape::Path) {
        updatePathControlLines(); // 恢复蓝线
        // createSimpleControlArm(shape); // 禁用红线
    }
}

void NodeHandleManager::updatePathControlLines()
{
    // 清除现有连线
    clearPathControlLines();
    
    if (!m_currentShape || m_currentShape->shapeType() != DrawingShape::Path) {
        return;
    }
    
    DrawingPath *path = static_cast<DrawingPath*>(m_currentShape);
    QVector<QPainterPath::ElementType> controlTypes = path->controlPointTypes();
    
    // 按照正确的贝塞尔控制关系创建连线
    for (int i = 0; i < controlTypes.size(); ++i) {
        QPainterPath::ElementType currentType = controlTypes[i];
        
        if (currentType == QPainterPath::CurveToElement) {
            // 三次贝塞尔的第一个控制点：连接到它控制的节点（前一个节点）
            if (i >= 1 && i < m_handleInfos.size()) {
                // 查找前一个节点
                int prevNodeIndex = -1;
                for (int j = i - 1; j >= 0; --j) {
                    if (m_handleInfos[j].type == PathNodeHandle) {
                        prevNodeIndex = j;
                        break;
                    }
                }
                
                if (prevNodeIndex != -1) {
                    QGraphicsLineItem *line = new QGraphicsLineItem(
                        QLineF(m_handleInfos[prevNodeIndex].handle->pos(), m_handleInfos[i].handle->pos())
                    );
                    line->setPen(QPen(QColor(100, 100, 255, 128), 1, Qt::DashLine));
                    line->setZValue(-1);
                    m_scene->addItem(line);
                    m_controlLines.append(line);
                }
            }
        } else if (currentType == QPainterPath::CurveToDataElement) {
            // 判断这是控制点还是节点
            bool isControlPoint = false;
            
            if (i >= 1 && i - 1 < controlTypes.size()) {
                QPainterPath::ElementType prevType = controlTypes[i - 1];
                if (prevType == QPainterPath::CurveToElement) {
                    // 前一个是CurveToElement，需要进一步判断
                    if (i + 1 < controlTypes.size() && controlTypes[i + 1] == QPainterPath::CurveToDataElement) {
                        // 下一个还是CurveToDataElement，当前是第二个控制点
                        // 第二个控制点连接到它控制的节点（下一个节点）
                        isControlPoint = true;
                    } else {
                        // 下一个不是CurveToDataElement，当前是终点（节点）
                        isControlPoint = false;
                    }
                } else {
                    // 前一个不是CurveToElement，这是二次贝塞尔的控制点
                    isControlPoint = true;
                }
            }
            
            if (isControlPoint && i < m_handleInfos.size()) {
                // 控制点连接到它控制的节点
                int controlledNodeIndex = -1;
                
                if (i >= 1 && i - 1 < controlTypes.size()) {
                    QPainterPath::ElementType prevType = controlTypes[i - 1];
                    if (prevType == QPainterPath::CurveToElement) {
                        // 三次贝塞尔的第二个控制点：查找下一个节点
                        for (int j = i + 1; j < m_handleInfos.size(); ++j) {
                            if (m_handleInfos[j].type == PathNodeHandle) {
                                controlledNodeIndex = j;
                                break;
                            }
                        }
                    } else {
                        // 二次贝塞尔的控制点：查找下一个节点
                        for (int j = i + 1; j < m_handleInfos.size(); ++j) {
                            if (m_handleInfos[j].type == PathNodeHandle) {
                                controlledNodeIndex = j;
                                break;
                            }
                        }
                    }
                }
                
                if (controlledNodeIndex != -1) {
                    QGraphicsLineItem *line = new QGraphicsLineItem(
                        QLineF(m_handleInfos[i].handle->pos(), m_handleInfos[controlledNodeIndex].handle->pos())
                    );
                    line->setPen(QPen(QColor(100, 100, 255, 128), 1, Qt::DashLine));
                    line->setZValue(-1);
                    m_scene->addItem(line);
                    m_controlLines.append(line);
                }
            }
        }
    }
}

void NodeHandleManager::clearPathControlLines()
{
    for (QGraphicsLineItem *line : m_controlLines) {
        if (line && line->scene()) {
            m_scene->removeItem(line);
            delete line;
        }
    }
    m_controlLines.clear();
}

// 控制杆相关方法实现
void NodeHandleManager::updateBezierControlArms(DrawingShape *shape)
{
    if (!shape || shape->shapeType() != DrawingShape::Path) {
        return;
    }
    
    DrawingPath *path = static_cast<DrawingPath*>(shape);
    m_bezierNodes.clear();
    
    // 从DrawingPath的控制点转换为贝塞尔节点
    QVector<QPointF> controlPoints = path->controlPoints();
    QVector<QPainterPath::ElementType> controlTypes = path->controlPointTypes();
    
    // 不再绘制红线，保持简洁
    
    // 根据调试信息修复算法：正确识别贝塞尔曲线结构
    for (int i = 0; i < controlPoints.size(); ++i) {
        if (i >= controlTypes.size()) break;
        
        QPainterPath::ElementType type = controlTypes[i];
        
        // 特殊处理：根据贝塞尔曲线的实际结构识别锚点
        bool isAnchor = false;
        if (type == QPainterPath::MoveToElement) {
            isAnchor = true; // 起点总是锚点
        } else if (type == QPainterPath::LineToElement) {
            isAnchor = true; // LineTo是锚点
        } else if (type == QPainterPath::CurveToDataElement) {
            // CurveData可能是锚点（贝塞尔曲线的终点）
            // 检查它是否是曲线段的最后一个点
            if (i == controlPoints.size() - 1) {
                isAnchor = true; // 最后一个点是锚点
            } else if (i + 1 < controlTypes.size() && 
                      (controlTypes[i + 1] == QPainterPath::MoveToElement || 
                       controlTypes[i + 1] == QPainterPath::LineToElement)) {
                isAnchor = true; // 下一个是锚点，当前点是曲线终点
            }
        }
        
        if (isAnchor) {
            // 这是一个锚点（节点）
            QPointF nodePos = controlPoints[i];
            BezierNode bezierNode(nodePos, BezierNodeType::Smooth);
            
            // 检查是否是贝塞尔曲线的起点
            if (i + 1 < controlPoints.size() && i + 1 < controlTypes.size() &&
                controlTypes[i + 1] == QPainterPath::CurveToElement) {
                
                // 这是贝塞尔曲线的起点，第一个控制点是 i+1
                QPointF outControlPoint = controlPoints[i + 1];
                bezierNode.outArm = BezierControlArm(outControlPoint - nodePos);
                bezierNode.outArm.isVisible = true;
                
                qDebug() << "Node" << i << "is curve start, out control:" << outControlPoint;
            }
            
            // 检查是否是贝塞尔曲线的终点
            if (i > 0) {
                // 查找这个锚点前面的曲线段
                bool foundCurve = false;
                int curveStart = -1;
                
                // 从当前锚点向前查找 CurveTo 元素
                for (int j = i - 1; j >= 0; --j) {
                    if (j < controlTypes.size() && controlTypes[j] == QPainterPath::CurveToElement) {
                        curveStart = j;
                        foundCurve = true;
                        break;
                    }
                    // 如果遇到另一个锚点，停止查找
                    if (j < controlTypes.size() && (controlTypes[j] == QPainterPath::MoveToElement || 
                                                  controlTypes[j] == QPainterPath::LineToElement)) {
                        break;
                    }
                }
                
                if (foundCurve && curveStart + 2 < i) {
                    // 找到了曲线段，第二个控制点是 i-1
                    QPointF inControlPoint = controlPoints[i - 1];
                    bezierNode.inArm = BezierControlArm(inControlPoint - nodePos);
                    bezierNode.inArm.isVisible = true;
                    
                    qDebug() << "Node" << i << "is curve end, in control:" << inControlPoint;
                }
            }
            
            // 如果没有控制杆，设为尖角节点
            if (!bezierNode.inArm.isVisible && !bezierNode.outArm.isVisible) {
                bezierNode.nodeType = BezierNodeType::Corner;
            }
            
            m_bezierNodes.append(bezierNode);
        }
    }
    
    // 创建控制杆可视化
    createBezierControlHandles(shape);
}

void NodeHandleManager::createBezierControlHandles(DrawingShape *shape)
{
    if (!shape || !m_scene) return;
    
    // 清除旧的控制杆连线
    for (QGraphicsLineItem *line : m_controlArmLines) {
        if (line && line->scene()) {
            m_scene->removeItem(line);
            delete line;
        }
    }
    m_controlArmLines.clear();
    
    // 为每个贝塞尔节点创建控制杆连线
    for (const BezierNode &bezierNode : m_bezierNodes) {
        QPointF nodeScenePos = calculateHandlePosition(bezierNode.nodePoint, shape);
        
        // 实现真正的Inkscape风格控制杆：连接两个控制点
        if (bezierNode.inArm.isVisible && bezierNode.outArm.isVisible) {
            // 节点既有进入控制点又有离开控制点，连接两个控制点
            QPointF inControlScenePos = calculateHandlePosition(bezierNode.getInControlPoint(), shape);
            QPointF outControlScenePos = calculateHandlePosition(bezierNode.getOutControlPoint(), shape);
            
            // QGraphicsLineItem *controlArm = new QGraphicsLineItem(
            //     QLineF(inControlScenePos, outControlScenePos)
            // );
            // controlArm->setPen(QPen(QColor(255, 100, 100, 200), 3, Qt::SolidLine)); // 红色粗线表示控制杆
            // controlArm->setZValue(-1);
            // m_scene->addItem(controlArm);
            // m_controlArmLines.append(controlArm);
            
            qDebug() << "Created control arm between two control points";
        } else {
            // 只有一个控制点的情况，连接节点和控制点
            //QPointF nodeScenePos = calculateHandlePosition(bezierNode.nodePoint, shape);
            
            // if (bezierNode.inArm.isVisible) {
            //     QPointF inControlScenePos = calculateHandlePosition(bezierNode.getInControlPoint(), shape);
            //     QGraphicsLineItem *inLine = new QGraphicsLineItem(
            //         QLineF(nodeScenePos, inControlScenePos)
            //     );
            //     inLine->setPen(QPen(QColor(255, 100, 100, 200), 2, Qt::SolidLine));
            //     inLine->setZValue(-1);
            //     m_scene->addItem(inLine);
            //     m_controlArmLines.append(inLine);
            // }
            
            // if (bezierNode.outArm.isVisible) {
            //     QPointF outControlScenePos = calculateHandlePosition(bezierNode.getOutControlPoint(), shape);
            //     QGraphicsLineItem *outLine = new QGraphicsLineItem(
            //         QLineF(nodeScenePos, outControlScenePos)
            //     );
            //     outLine->setPen(QPen(QColor(100, 100, 255, 200), 2, Qt::SolidLine));
            //     outLine->setZValue(-1);
            //     m_scene->addItem(outLine);
            //     m_controlArmLines.append(outLine);
            // }
        }
        
        // 暂时移除离开控制杆连线（绿色线）
        // if (bezierNode.outArm.isVisible) {
        //     QPointF outControlScenePos = calculateHandlePosition(bezierNode.getOutControlPoint(), shape);
        //     QGraphicsLineItem *outLine = new QGraphicsLineItem(
        //         QLineF(nodeScenePos, outControlScenePos)
        //     );
        //     outLine->setPen(QPen(QColor(100, 255, 100, 180), 2, Qt::SolidLine));
        //     outLine->setZValue(-2);
        //     m_scene->addItem(outLine);
        //     m_controlArmLines.append(outLine);
        // }
    }
}

void NodeHandleManager::updateControlArmLines()
{
    // 重新创建控制杆连线
    if (m_currentShape && m_currentShape->shapeType() == DrawingShape::Path) {
        createSimpleControlArm(m_currentShape);
    }
}

void NodeHandleManager::createSimpleControlArm(DrawingShape *shape)
{
    if (!shape || !m_scene) return;
    
    // 清除旧的控制杆连线
    for (QGraphicsLineItem *line : m_controlArmLines) {
        if (line && line->scene()) {
            m_scene->removeItem(line);
            delete line;
        }
    }
    m_controlArmLines.clear();
    
    // 不再绘制红线，保持简洁
}

// 基于NodeInfo的智能手柄管理方法实现
void NodeHandleManager::updateHandlesFromNodeInfo(DrawingShape *shape)
{
    if (!shape) return;
    
    // 清除现有手柄
    clearHandles();
    m_currentShape = shape;
    
    // 获取图形的节点信息
    QVector<NodeInfo> nodeInfos = shape->getNodeInfo();
    
    // 为每个节点信息创建相应的手柄
    for (int i = 0; i < nodeInfos.size(); ++i) {
        createHandlesForNodeInfo(nodeInfos[i], i);
    }
    
    if (m_handlesVisible) {
        showHandles();
    }
}

void NodeHandleManager::createHandlesForNodeInfo(const NodeInfo &nodeInfo, int index)
{
    if (!m_scene) return;
    
    QPointF scenePos = calculateHandlePosition(nodeInfo.position, m_currentShape);
    NodeHandleType handleType = getHandleTypeFromNodeInfo(nodeInfo);
    
    NodeHandleInfo info;
    info.nodeIndex = index;
    info.originalPos = scenePos;
    info.type = handleType;
    
    // 根据节点类型创建手柄
    switch (handleType) {
    case CornerRadiusHandle:
        info.handle = createCornerRadiusHandle(scenePos);
        break;
    case SizeControlHandle:
        info.handle = createSizeControlHandle(scenePos);
        break;
    case PathNodeHandle:
        info.handle = createPathNodeHandle(scenePos);
        break;
    case PathControlHandle:
        info.handle = createPathControlHandle(scenePos);
        break;
    default:
        info.handle = createCustomNodeHandle(scenePos, index);
        break;
    }
    
    // 应用智能节点样式
    setupHandleStyleFromNodeInfo(info.handle, nodeInfo);
    
    m_handleInfos.append(info);
    
    // 如果节点有控制点，创建控制点手柄
    if (nodeInfo.hasControlIn) {
        QPointF inControlPos = calculateHandlePosition(nodeInfo.controlIn, m_currentShape);
        NodeHandleInfo inInfo;
        inInfo.nodeIndex = index;
        inInfo.originalPos = inControlPos;
        inInfo.type = BezierControlIn;
        inInfo.handle = createPathControlHandle(inControlPos);
        setupHandleStyle(inInfo.handle, BezierControlIn);
        m_handleInfos.append(inInfo);
    }
    
    if (nodeInfo.hasControlOut) {
        QPointF outControlPos = calculateHandlePosition(nodeInfo.controlOut, m_currentShape);
        NodeHandleInfo outInfo;
        outInfo.nodeIndex = index;
        outInfo.originalPos = outControlPos;
        outInfo.type = BezierControlOut;
        outInfo.handle = createPathControlHandle(outControlPos);
        setupHandleStyle(outInfo.handle, BezierControlOut);
        m_handleInfos.append(outInfo);
    }
}

NodeHandleManager::NodeHandleType NodeHandleManager::getHandleTypeFromNodeInfo(const NodeInfo &nodeInfo) const
{
    switch (nodeInfo.type) {
    case NodeInfo::SizeControl:
        return SizeControlHandle;
    case NodeInfo::AngleControl:
        return PathControlHandle;
    case NodeInfo::RadiusControl:
        return CornerRadiusHandle;
    case NodeInfo::Corner:
    case NodeInfo::Smooth:
    case NodeInfo::Symmetric:
    case NodeInfo::Curve:
    case NodeInfo::Start:
    case NodeInfo::End:
        return PathNodeHandle;
    default:
        return CustomNodeHandle;
    }
}

void NodeHandleManager::setupHandleStyleFromNodeInfo(CustomHandleItem *handle, const NodeInfo &nodeInfo) const
{
    if (!handle) return;
    
    // 根据节点类型设置颜色
    switch (nodeInfo.type) {
    case NodeInfo::Corner:
        handle->setSpecificColor(CORNER_NODE_COLOR);
        handle->setStyle(HandleItemBase::Square);
        break;
    case NodeInfo::Smooth:
        handle->setSpecificColor(SMOOTH_NODE_COLOR);
        handle->setStyle(HandleItemBase::Square);
        break;
    case NodeInfo::Symmetric:
        handle->setSpecificColor(SYMMETRIC_NODE_COLOR);
        handle->setStyle(HandleItemBase::Diamond);
        break;
    case NodeInfo::Curve:
        handle->setSpecificColor(CURVE_NODE_COLOR);
        handle->setStyle(HandleItemBase::Circle);
        break;
    case NodeInfo::Start:
        handle->setSpecificColor(QColor(100, 200, 255, 230)); // 浅蓝色
        handle->setStyle(HandleItemBase::Square);
        break;
    case NodeInfo::End:
        handle->setSpecificColor(QColor(255, 100, 200, 230)); // 粉色
        handle->setStyle(HandleItemBase::Square);
        break;
    default:
        // 使用默认样式
        break;
    }
}

// 智能控制杆处理方法实现
void NodeHandleManager::handleSmartControlArmDrag(CustomHandleItem *handle, const QPointF &newPos)
{
    if (!handle || !m_currentShape) return;
    
    // 获取手柄信息
    NodeHandleInfo handleInfo = getHandleInfo(handle);
    if (handleInfo.nodeIndex < 0) return;
    
    qDebug() << "=== Smart Control Arm Drag ===";
    qDebug() << "Handle type:" << handleInfo.type << "Node index:" << handleInfo.nodeIndex;
    
    // 获取图形的节点信息
    QVector<NodeInfo> nodeInfos = m_currentShape->getNodeInfo();
    if (handleInfo.nodeIndex >= nodeInfos.size()) return;
    
    NodeInfo &nodeInfo = nodeInfos[handleInfo.nodeIndex];
    QPointF localPos = m_currentShape->mapFromScene(newPos);
    
    // 判断是拖拽进入控制点还是离开控制点
    bool isInArm = (handleInfo.type == BezierControlIn);
    
    qDebug() << "Node type before drag:" << nodeInfo.type;
    qDebug() << "Is dragging in arm:" << isInArm;
    qDebug() << "New control position:" << localPos;
    
    // 使用NodeInfo的智能单杆控制逻辑
    nodeInfo.handleSingleArmDrag(isInArm, localPos);
    
    // 更新图形的控制点
    updateNodeFromHandle(handle, newPos);
    
    qDebug() << "Node type after drag:" << nodeInfo.type;
    nodeInfo.debugPrint();
    qDebug() << "=== End Smart Control Arm Drag ===";
}

void NodeHandleManager::updateNodeFromHandle(CustomHandleItem *handle, const QPointF &newPos)
{
    if (!handle || !m_currentShape) return;
    
    NodeHandleInfo handleInfo = getHandleInfo(handle);
    if (handleInfo.nodeIndex < 0) return;
    
    // 获取图形的节点信息
    QVector<NodeInfo> nodeInfos = m_currentShape->getNodeInfo();
    if (handleInfo.nodeIndex >= nodeInfos.size()) return;
    
    NodeInfo &nodeInfo = nodeInfos[handleInfo.nodeIndex];
    QPointF localPos = m_currentShape->mapFromScene(newPos);
    
    // 根据手柄类型更新相应的控制点
    switch (handleInfo.type) {
    case BezierControlIn:
        nodeInfo.controlIn = localPos;
        nodeInfo.hasControlIn = true;
        break;
        
    case BezierControlOut:
        nodeInfo.controlOut = localPos;
        nodeInfo.hasControlOut = true;
        break;
        
    case PathNodeHandle:
        nodeInfo.position = localPos;
        break;
        
    default:
        // 其他手柄类型的处理
        break;
    }
    
    // 对于路径图形，需要同步更新控制点数组
    if (m_currentShape->shapeType() == DrawingShape::Path) {
        DrawingPath *path = static_cast<DrawingPath*>(m_currentShape);
        if (path) {
            QVector<QPointF> controlPoints = path->controlPoints();
            
            // 根据节点信息更新控制点数组
            if (handleInfo.type == BezierControlIn && nodeInfo.hasControlIn) {
                // 找到对应的控制点索引并更新
                // 这里需要根据实际的控制点数组结构来映射
                for (int i = 0; i < controlPoints.size(); ++i) {
                    if (i < nodeInfos.size() && nodeInfos[i].elementIndex == nodeInfo.elementIndex) {
                        // 更新对应的控制点
                        controlPoints[i] = nodeInfo.controlIn;
                        break;
                    }
                }
            } else if (handleInfo.type == BezierControlOut && nodeInfo.hasControlOut) {
                // 类似地更新离开控制点
                for (int i = 0; i < controlPoints.size(); ++i) {
                    if (i < nodeInfos.size() && nodeInfos[i].elementIndex == nodeInfo.elementIndex) {
                        controlPoints[i] = nodeInfo.controlOut;
                        break;
                    }
                }
            }
            
            path->setControlPoints(controlPoints);
        }
    }
}
