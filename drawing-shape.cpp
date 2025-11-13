#include "drawing-shape.h"
#include "drawing-document.h"
#include "drawing-transform.h"
#include "drawing-edit-handles.h"
#include "drawingview.h"
#include "toolbase.h"
#include "drawingscene.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QDebug>
#include <QGraphicsScene>

// DrawingShape
DrawingShape::DrawingShape(ShapeType type, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_type(type)
    , m_fillBrush(Qt::white)
    , m_strokePen(QPen(Qt::black, 1.0))
    , m_showSelectionIndicator(true)
{
    setFlags(QGraphicsItem::ItemIsSelectable | 
             QGraphicsItem::ItemIsMovable | 
             QGraphicsItem::ItemSendsGeometryChanges);
}

DrawingShape::~DrawingShape()
{
    // 清理编辑把手管理器
    if (m_handleManager) {
        // 先隐藏所有把手
        m_handleManager->hideHandles();
        // 从场景中移除所有把手（如果它们在场景中）
        const QList<EditHandle*> handles = m_handleManager->handles();
        for (EditHandle *handle : handles) {
            if (handle && handle->scene()) {
                handle->scene()->removeItem(handle);
            }
        }
        // 清空把手列表，避免在析构函数中再次访问
        m_handleManager->clearHandles();
        delete m_handleManager;
        m_handleManager = nullptr;
    }
    
    // 清理graphics effect
    QGraphicsEffect* effect = graphicsEffect();
    if (effect) {
        // Qt会自动删除graphics effect，但我们先设置为nullptr
        setGraphicsEffect(nullptr);
    }
    
    // 清除可能存在的吸附指示器（防止悬空指针）
    if (scene()) {
        DrawingScene *drawingScene = qobject_cast<DrawingScene*>(scene());
        if (drawingScene) {
            drawingScene->clearSnapIndicators();
        }
    }
}

void DrawingShape::setTransform(const DrawingTransform &transform)
{
    prepareGeometryChange();
    m_transform = transform;
    update();
    
    // 通知文档对象已修改
    if (m_document) {
        // TODO: 实现Document的setModified方法
    }
}

void DrawingShape::rotateAroundAnchor(double angle, DrawingTransform::AnchorPoint anchor)
{
    QPointF center = m_transform.getAnchorPoint(anchor, localBounds());
    m_transform.rotate(angle, center);
    update();
}

void DrawingShape::scaleAroundAnchor(double sx, double sy, DrawingTransform::AnchorPoint anchor)
{
    QPointF center = m_transform.getAnchorPoint(anchor, localBounds());
    m_transform.scale(sx, sy, center);
    update();
}

void DrawingShape::shearAroundAnchor(double sh, double sv, DrawingTransform::AnchorPoint anchor)
{
    QPointF center = m_transform.getAnchorPoint(anchor, localBounds());
    m_transform.shear(sh, sv, center);
    update();
}

QRectF DrawingShape::boundingRect() const
{
    QRectF localBoundsRect = localBounds();
    QRectF transformedBounds = m_transform.transformedBounds(localBoundsRect);
    return transformedBounds;
}

QPainterPath DrawingShape::shape() const
{
    QPainterPath path;
    // 创建本地边界的路径
    path.addRect(localBounds());
    return m_transform.transform().map(path);
}

QPainterPath DrawingShape::transformedShape() const
{
    QPainterPath path;
    // 创建本地边界的路径
    path.addRect(localBounds());
    // 应用变换并设置填充规则
    path = m_transform.transform().map(path);
    path.setFillRule(Qt::WindingFill);
    return path;
}

void DrawingShape::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    
    // 保存当前变换状态
    painter->save();
    
    // 应用变换矩阵
    painter->setTransform(m_transform.transform(), true);
    
    // 绘制填充
    painter->setBrush(m_fillBrush);
    painter->setPen(Qt::NoPen);
    paintShape(painter);
    
    // 绘制描边，使用cosmetic画笔确保线宽不随缩放变化
    painter->setBrush(Qt::NoBrush);
    QPen cosmeticPen = m_strokePen;
    cosmeticPen.setCosmetic(true);  // 设置为cosmetic画笔，线宽不受变换影响
    painter->setPen(cosmeticPen);
    paintShape(painter);
    
    // 恢复变换状态
    painter->restore();
    
    // 绘制选择指示器（在场景坐标系中）
    // 只有当m_showSelectionIndicator为true且图形被选中时才绘制
    if (isSelected() && m_showSelectionIndicator) {
        // 使用更明显的选择指示器
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        
        // 绘制边界框
        QRectF bounds = boundingRect();
        painter->drawRect(bounds);
        
        // 绘制选择手柄（角落的小方块）
        qreal handleSize = 8.0;
        painter->setPen(QPen(Qt::blue, 1));
        painter->setBrush(QBrush(Qt::white));
        
        // 四个角的手柄
        painter->drawRect(QRectF(bounds.left() - handleSize/2, bounds.top() - handleSize/2, handleSize, handleSize));
        painter->drawRect(QRectF(bounds.right() - handleSize/2, bounds.top() - handleSize/2, handleSize, handleSize));
        painter->drawRect(QRectF(bounds.left() - handleSize/2, bounds.bottom() - handleSize/2, handleSize, handleSize));
        painter->drawRect(QRectF(bounds.right() - handleSize/2, bounds.bottom() - handleSize/2, handleSize, handleSize));
    }
}

void DrawingShape::setEditHandlesEnabled(bool enabled)
{
    if (m_editHandlesEnabled == enabled) {
        return;
    }
    
    m_editHandlesEnabled = enabled;
    
    if (enabled) {
        qDebug() << "Enabling edit handles for shape:" << this << "type:" << type() << "in scene:" << (scene() ? "yes" : "no");
        
        // 确保图形已在场景中
        if (!scene()) {
            qDebug() << "Shape not in scene, deferring edit handles";
            return;
        }
        
        // 创建编辑把手管理器
        if (!m_handleManager) {
            m_handleManager = new EditHandleManager(this);
            qDebug() << "Created EditHandleManager for shape:" << this;
        }
        
        // 显示编辑把手
        if (m_handleManager) {
            m_handleManager->updateHandles(); // 先更新位置
            m_handleManager->showHandles();   // 再显示把手
        }
        
        qDebug() << "Edit handles enabled, handle count:" << (m_handleManager ? m_handleManager->handles().count() : 0);
    } else {
        qDebug() << "Disabling edit handles for shape:" << this << "type:" << type();
        
        // 隐藏编辑把手
        if (m_handleManager) {
            m_handleManager->hideHandles();
        }
    }
}

QVariant DrawingShape::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        // 🌟 在位置改变之前应用对象吸附（使用temp分支的方案）
        QPointF newPos = value.toPointF();
        
        DrawingScene *drawingScene = qobject_cast<DrawingScene*>(scene());
        
        if (drawingScene && drawingScene->isObjectSnapEnabled()) {
            // 使用alignToGrid方法，它会处理所有吸附逻辑
            bool isObjectSnap = false;
            QPointF alignedPos = drawingScene->alignToGrid(newPos, this, &isObjectSnap);
            
            // 如果位置有变化，返回吸附后的位置
            if (alignedPos != newPos) {
                // 只有对象吸附才显示指示器
                if (isObjectSnap) {
                    return alignedPos;
                } else {
                    // 网格吸附，直接返回位置但不显示指示器
                    return alignedPos;
                }
            }
        }
    } else if (change == ItemTransformHasChanged || change == ItemPositionHasChanged) {
        // 更新编辑把手位置
        if (m_handleManager) {
            m_handleManager->updateHandles();
        }
        
        // 位置变化，不需要发射信号
    } else if (change == ItemParentHasChanged) {
        // 父项改变时更新手柄状态
        if (m_handleManager) {
            m_handleManager->updateHandles();
        }
    }
    
    return QGraphicsItem::itemChange(change, value);
}

// DrawingRectangle
DrawingRectangle::DrawingRectangle(QGraphicsItem *parent)
    : DrawingShape(Rectangle, parent)
    , m_rect(0, 0, 100, 100)
{
}

DrawingRectangle::DrawingRectangle(const QRectF &rect, QGraphicsItem *parent)
    : DrawingShape(Rectangle, parent)
    , m_rect(rect)
{
}

QRectF DrawingRectangle::localBounds() const
{
    return m_rect;
}

QPainterPath DrawingRectangle::shape() const
{
    QPainterPath path;
    if (m_cornerRadius > 0) {
        // 创建圆角矩形路径
        path.addRoundedRect(m_rect, m_cornerRadius, m_cornerRadius);
    } else {
        // 创建普通矩形路径
        path.addRect(m_rect);
    }
    return path;
}

QPainterPath DrawingRectangle::transformedShape() const
{
    QPainterPath path;
    if (m_cornerRadius > 0) {
        // 创建圆角矩形路径
        path.addRoundedRect(m_rect, m_cornerRadius, m_cornerRadius);
    } else {
        // 创建普通矩形路径
        path.addRect(m_rect);
    }
    
    // 应用变换
    path = m_transform.transform().map(path);
    
    // 设置填充规则
    path.setFillRule(Qt::WindingFill);
    
    return path;
}

void DrawingRectangle::setRectangle(const QRectF &rect)
{
    if (m_rect != rect) {
        prepareGeometryChange();
        m_rect = rect;
        update();
    }
}

QVector<QPointF> DrawingRectangle::getNodePoints() const
{
    QVector<QPointF> points;
    points.reserve(2);
    
    // 1. 圆角控制点：简单放在左上角
    if (m_cornerRadius > 0) {
        points.append(QPointF(m_rect.left() + m_cornerRadius, m_rect.top()));
    } else {
        points.append(QPointF(m_rect.left() + 10, m_rect.top()));
    }
    
    // 2. 尺寸控制点：右下角
    points.append(m_rect.bottomRight());
    
    return points;
}

void DrawingRectangle::setNodePoint(int index, const QPointF &pos)
{
    switch (index) {
        case 0: {
            // 圆角控制：简单的距离控制，实时更新
            QPointF localPos = mapFromScene(pos);
            qreal distance = localPos.x() - m_rect.left();
            qreal maxRadius = qMin(m_rect.width(), m_rect.height()) / 2.0;
            m_cornerRadius = qBound(0.0, distance, maxRadius);
            update();
            break;
        }
        case 1: {
            // 尺寸控制：简单的右下角拖动，实时更新
            QPointF localPos = mapFromScene(pos);
            QRectF newRect = m_rect;
            newRect.setRight(localPos.x());
            newRect.setBottom(localPos.y());
            
            // 确保最小尺寸
            if (newRect.width() < 20) newRect.setWidth(20);
            if (newRect.height() < 20) newRect.setHeight(20);
            
            setRectangle(newRect);
            break;
        }
        default:
            return;
    }
}

// 空实现，使用基类的默认实现

QPointF DrawingRectangle::constrainNodePoint(int index, const QPointF &pos) const
{
    // 获取当前的旋转角度
    qreal rotation = m_transform.rotation();
    qreal rotationRad = rotation * M_PI / 180.0;
    
    QPointF localPos = mapFromScene(pos);
    
    switch (index) {
        case 0: {
            // 圆角控制：限制在旋转后的上边框上
            // 将点转换到旋转前的坐标系
            QPointF rotatedPos = localPos - m_rect.topLeft();
            QPointF unrotatedPos;
            unrotatedPos.setX(rotatedPos.x() * qCos(-rotationRad) - rotatedPos.y() * qSin(-rotationRad));
            unrotatedPos.setY(rotatedPos.x() * qSin(-rotationRad) + rotatedPos.y() * qCos(-rotationRad));
            
            // 在未旋转坐标系中限制在上边框上
            unrotatedPos.setY(0);
            unrotatedPos.setX(qBound(0.0, unrotatedPos.x(), m_rect.width()));
            
            // 转换回旋转后的坐标系
            QPointF finalPos;
            finalPos.setX(unrotatedPos.x() * qCos(rotationRad) - unrotatedPos.y() * qSin(rotationRad));
            finalPos.setY(unrotatedPos.x() * qSin(rotationRad) + unrotatedPos.y() * qCos(rotationRad));
            finalPos += m_rect.topLeft();
            
            return mapToScene(finalPos);
        }
        case 1: {
            // 尺寸控制：不限制，允许自由拖动
            return pos;
        }
        default:
            return pos;
    }
}

void DrawingRectangle::beginNodeDrag(int index)
{
    if (index == 0) {
        m_dragMode = 1; // 圆角控制
    } else if (index == 1) {
        m_dragMode = 2; // 尺寸控制
    }
}

void DrawingRectangle::endNodeDrag(int index)
{
    Q_UNUSED(index);
    // 清除拖动状态
    m_dragMode = 0;
}

void DrawingRectangle::paintShape(QPainter *painter)
{
    // 简单的圆角绘制
    if (m_cornerRadius > 0) {
        painter->drawRoundedRect(m_rect, m_cornerRadius, m_cornerRadius);
    } else {
        painter->drawRect(m_rect);
    }
}

// DrawingEllipse
DrawingEllipse::DrawingEllipse(QGraphicsItem *parent)
    : DrawingShape(Ellipse, parent)
    , m_rect(0, 0, 100, 100)
{
}

DrawingEllipse::DrawingEllipse(const QRectF &rect, QGraphicsItem *parent)
    : DrawingShape(Ellipse, parent)
    , m_rect(rect)
{
}

QRectF DrawingEllipse::localBounds() const
{
    return m_rect;
}

QPainterPath DrawingEllipse::shape() const
{
    QPainterPath path;
    path.addEllipse(m_rect);
    return path;
}

QPainterPath DrawingEllipse::transformedShape() const
{
    QPainterPath path;
    path.addEllipse(m_rect);
    
    // 应用变换
    path = m_transform.transform().map(path);
    
    // 设置填充规则
    path.setFillRule(Qt::WindingFill);
    
    return path;
}

void DrawingEllipse::setEllipse(const QRectF &rect)
{
    if (m_rect != rect) {
        prepareGeometryChange();
        m_rect = rect;
        update();
    }
}

QVector<QPointF> DrawingEllipse::getNodePoints() const
{
    QVector<QPointF> points;
    points.reserve(4);
    
    // 添加4个控制点：
    // 0. 右边中点 - 控制水平半径
    // 1. 下边中点 - 控制垂直半径  
    // 2. 起始角度控制点
    // 3. 结束角度控制点
    
    // 尺寸控制点
    points.append(QPointF(m_rect.right(), m_rect.center().y()));  // 右边中点
    points.append(QPointF(m_rect.center().x(), m_rect.bottom())); // 下边中点
    
    // 角度控制点（始终添加，即使椭圆闭合也可以编辑）
    if (true) {
        qreal centerX = m_rect.center().x();
        qreal centerY = m_rect.center().y();
        qreal radiusX = m_rect.width() / 2.0;
        qreal radiusY = m_rect.height() / 2.0;
        
        // 应用../qdraw的角度处理逻辑
        qreal startAngle = m_startAngle <= m_spanAngle ? m_startAngle : m_spanAngle;
        qreal endAngle = m_startAngle >= m_spanAngle ? m_startAngle : m_spanAngle;
        
        // 起始角度控制点（使用与../qdraw相同的计算方式）
        qreal startAngleRad = -startAngle * M_PI / 180.0;
        qreal x1 = centerX + radiusX * qCos(startAngleRad);
        qreal y1 = centerY + radiusY * qSin(startAngleRad);
        points.append(QPointF(x1, y1));
        
        // 结束角度控制点（使用与../qdraw相同的计算方式）
        qreal endAngleRad = -endAngle * M_PI / 180.0;
        qreal x2 = centerX + radiusX * qCos(endAngleRad);
        qreal y2 = centerY + radiusY * qSin(endAngleRad);
        points.append(QPointF(x2, y2));
    }
    
    return points;
}

void DrawingEllipse::setNodePoint(int index, const QPointF &pos)
{
    // 将场景坐标转换为本地坐标
    QPointF localPos = mapFromScene(pos);
    
    switch (index) {
        case 0: {
            // 控制水平半径（右边中点）
            QRectF newRect = m_rect;
            newRect.setRight(localPos.x());
            // 确保最小宽度
            if (newRect.width() < 20) {
                newRect.setRight(newRect.left() + 20);
            }
            setEllipse(newRect);
            break;
        }
        case 1: {
            // 控制垂直半径（下边中点）
            QRectF newRect = m_rect;
            newRect.setBottom(localPos.y());
            // 确保最小高度
            if (newRect.height() < 20) {
                newRect.setBottom(newRect.top() + 20);
            }
            setEllipse(newRect);
            break;
        }
        case 2:
        case 3: {
            // 控制角度（即使椭圆闭合也可以编辑）
            // 如果椭圆是闭合的（spanAngle == 360），先将其转换为可编辑状态
            if (m_spanAngle == 360) {
                // 如果是第一次编辑闭合椭圆，设置一个小的起始角度和跨度
                if (index == 2) {
                    // 编辑起始角度时，设置起始角度为当前鼠标位置，跨度为270度
                    m_startAngle = 0;
                    m_spanAngle = 270;
                } else {
                    // 编辑结束角度时，保持起始角度为0，设置跨度为当前鼠标位置
                    m_spanAngle = 270;
                }
            }
            
            qreal centerX = m_rect.center().x();
            qreal centerY = m_rect.center().y();
            qreal dx = pos.x() - centerX;
            qreal dy = pos.y() - centerY;
            // 使用与../qdraw相同的角度计算方式
            qreal angle = -qAtan2(dy, dx) * 180.0 / M_PI;
            
            // 不进行角度范围限制，允许超过360度
            
            if (index == 2) {
                // 控制起始角度
                setStartAngle(angle);
            } else if (index == 3) {
                // 控制结束角度，直接设置跨度
                setSpanAngle(angle);
            }
            
            // 应用../qdraw的角度交换逻辑
            if (m_startAngle > m_spanAngle) {
                m_startAngle -= 360;
            }
            if (m_spanAngle < m_startAngle) {
                qreal tmp = m_spanAngle;
                m_spanAngle = m_startAngle;
                m_startAngle = tmp;
            }
            
            // 防止角度差值过大
            if (qAbs(m_spanAngle - m_startAngle) > 360) {
                m_startAngle = 40;
                m_spanAngle = 400;
            }
            break;
        }
        default:
            return;
    }
}

QPointF DrawingEllipse::constrainNodePoint(int index, const QPointF &pos) const
{
    // 获取当前的旋转角度
    qreal rotation = m_transform.rotation();
    qreal rotationRad = rotation * M_PI / 180.0;
    
    QPointF localPos = mapFromScene(pos);
    QPointF center = m_rect.center();
    
    switch (index) {
        case 0: {
            // 水平半径控制：限制在旋转后的水平轴上
            // 将点转换到旋转前的坐标系
            QPointF rotatedPos = localPos - center;
            QPointF unrotatedPos;
            unrotatedPos.setX(rotatedPos.x() * qCos(-rotationRad) - rotatedPos.y() * qSin(-rotationRad));
            unrotatedPos.setY(rotatedPos.x() * qSin(-rotationRad) + rotatedPos.y() * qCos(-rotationRad));
            
            // 在未旋转坐标系中限制在水平轴上
            qreal x = qMax(10.0, unrotatedPos.x()); // 最小半径10
            unrotatedPos.setY(0);
            
            // 转换回旋转后的坐标系
            QPointF finalPos;
            finalPos.setX(unrotatedPos.x() * qCos(rotationRad) - unrotatedPos.y() * qSin(rotationRad));
            finalPos.setY(unrotatedPos.x() * qSin(rotationRad) + unrotatedPos.y() * qCos(rotationRad));
            finalPos += center;
            
            return mapToScene(finalPos);
        }
        case 1: {
            // 垂直半径控制：限制在旋转后的垂直轴上
            // 将点转换到旋转前的坐标系
            QPointF rotatedPos = localPos - center;
            QPointF unrotatedPos;
            unrotatedPos.setX(rotatedPos.x() * qCos(-rotationRad) - rotatedPos.y() * qSin(-rotationRad));
            unrotatedPos.setY(rotatedPos.x() * qSin(-rotationRad) + rotatedPos.y() * qCos(-rotationRad));
            
            // 在未旋转坐标系中限制在垂直轴上
            qreal y = qMax(10.0, unrotatedPos.y()); // 最小半径10
            unrotatedPos.setX(0);
            
            // 转换回旋转后的坐标系
            QPointF finalPos;
            finalPos.setX(unrotatedPos.x() * qCos(rotationRad) - unrotatedPos.y() * qSin(rotationRad));
            finalPos.setY(unrotatedPos.x() * qSin(rotationRad) + unrotatedPos.y() * qCos(rotationRad));
            finalPos += center;
            
            return mapToScene(finalPos);
        }
        case 2:
        case 3: {
            // 角度控制：允许自由拖动
            return pos;
        }
        default:
            break;
    }
    
    return pos;
}

void DrawingEllipse::beginNodeDrag(int index)
{
    if (index == 2 || index == 3) {
        // 记录拖动开始时的状态
        m_dragStartAngle = m_startAngle;
        m_dragStartSpan = m_spanAngle;
        m_dragMode = index; // 2=起始角度，3=结束角度
    }
}

void DrawingEllipse::endNodeDrag(int index)
{
    Q_UNUSED(index);
    // 清除拖动状态
    m_dragMode = 0;
}

// 空实现，使用基类的默认实现

void DrawingEllipse::paintShape(QPainter *painter)
{
    // 应用../qdraw的角度处理逻辑
    qreal startAngle = m_startAngle <= m_spanAngle ? m_startAngle : m_spanAngle;
    qreal endAngle = m_startAngle >= m_spanAngle ? m_startAngle : m_spanAngle;
    
    if (endAngle - startAngle > 360) {
        endAngle = startAngle + 360;
    }
    
    if (qFuzzyCompare(qAbs(endAngle - startAngle), 360.0) || qFuzzyCompare(qAbs(endAngle - startAngle), 0.0)) {
        // 绘制完整椭圆
        painter->drawEllipse(m_rect);
    } else {
        // 绘制椭圆弧
        painter->drawArc(m_rect, qRound(startAngle * 16), qRound((endAngle - startAngle) * 16));
    }
}

// DrawingPath
DrawingPath::DrawingPath(QGraphicsItem *parent)
    : DrawingShape(Path, parent)
    , m_activeControlPoint(-1)
{
}

QRectF DrawingPath::localBounds() const
{
    // 获取路径本身的边界框
    QRectF pathBounds = m_path.boundingRect();
    
    // 如果有控制点，需要扩展边界框以包含所有控制点
    if (!m_controlPoints.isEmpty()) {
        QRectF controlBounds;
        bool firstPoint = true;
        
        for (const QPointF &point : m_controlPoints) {
            if (firstPoint) {
                controlBounds = QRectF(point, QSizeF(1, 1));
                firstPoint = false;
            } else {
                controlBounds = controlBounds.united(QRectF(point, QSizeF(1, 1)));
            }
        }
        
        // 为控制点添加一些边距，确保圆形控制点完全可见
        const qreal margin = 7.0; // 控制点半径4.0 + 额外边距
        controlBounds.adjust(-margin, -margin, margin, margin);
        
        // 返回路径边界框和控制点边界框的并集
        return pathBounds.united(controlBounds);
    }
    
    return pathBounds;
}

void DrawingPath::setPath(const QPainterPath &path)
{
    if (m_path != path) {
        prepareGeometryChange();
        m_path = path;
        
        // 保存原始路径元素信息，用于节点编辑
        m_pathElements.clear();
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element &elem = path.elementAt(i);
            m_pathElements.append(elem);
        }
        
        // 提取所有类型的元素作为控制点，但标记它们的类型
        m_controlPoints.clear();
        m_controlPointTypes.clear();
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element &elem = path.elementAt(i);
            m_controlPoints.append(QPointF(elem.x, elem.y));
            m_controlPointTypes.append(elem.type);
        }
        
        update();
    }
}

QPainterPath DrawingPath::transformedShape() const
{
    // 直接返回路径，应用变换
    QPainterPath path = m_path;
    path = m_transform.transform().map(path);
    path.setFillRule(Qt::WindingFill);
    return path;
}

QVector<QPointF> DrawingPath::getNodePoints() const
{
    return m_controlPoints; // 返回路径的控制点作为节点编辑点
}

void DrawingPath::setNodePoint(int index, const QPointF &pos)
{
    if (index >= 0 && index < m_controlPoints.size()) {
        m_controlPoints[index] = pos;
        updatePathFromControlPoints(); // 更新路径
    }
}

QPointF DrawingPath::constrainNodePoint(int index, const QPointF &pos) const
{
    // 路径控制点可以在任何位置，不需要特殊约束
    Q_UNUSED(index);
    return pos;
}

void DrawingPath::beginNodeDrag(int index)
{
    Q_UNUSED(index);
    // 路径不需要特殊的拖动开始处理
}

void DrawingPath::endNodeDrag(int index)
{
    Q_UNUSED(index);
    // 路径不需要特殊的拖动结束处理
}

void DrawingPath::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_showControlPolygon) {
        // 查找最近的控制点
        int nearestPoint = findNearestControlPoint(event->scenePos());
        if (nearestPoint != -1) {
            m_activeControlPoint = nearestPoint;
            m_dragStartPos = event->scenePos();
            event->accept();
            return;
        }
    }
    
    // 如果没有点击控制点，传递给基类处理
    QGraphicsItem::mousePressEvent(event);
}

void DrawingPath::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeControlPoint != -1 && (event->buttons() & Qt::LeftButton)) {
        // 拖动控制点
        QPointF newPos = event->scenePos();
        
        // 更新控制点位置
        if (m_activeControlPoint < m_controlPoints.size()) {
            // 将场景坐标转换为本地坐标，考虑DrawingTransform
            // 首先使用Qt的内置方法转换为图形本地坐标
            QPointF localPos = mapFromScene(newPos);
            // 然后应用DrawingTransform的逆变换来获取真正的本地坐标
            localPos = m_transform.transform().inverted().map(localPos);
            
            m_controlPoints[m_activeControlPoint] = localPos;
            
            // 更新路径
            updatePathFromControlPoints();
            update();
        }
        
        event->accept();
        return;
    }
    
    // 如果没有拖动控制点，传递给基类处理
    QGraphicsItem::mouseMoveEvent(event);
}

void DrawingPath::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_activeControlPoint != -1) {
        // 结束拖动
        m_activeControlPoint = -1;
        event->accept();
        return;
    }
    
    // 如果没有拖动控制点，传递给基类处理
    QGraphicsItem::mouseReleaseEvent(event);
}

int DrawingPath::findNearestControlPoint(const QPointF &scenePos) const
{
    int nearestIndex = -1;
    qreal minDistance = 10.0; // 阈值距离
    
    for (int i = 0; i < m_controlPoints.size(); ++i) {
        // 首先应用DrawingTransform变换
        QPointF transformedPoint = m_transform.transform().map(m_controlPoints[i]);
        // 然后使用Qt的内置方法将结果转换为场景坐标
        QPointF controlScenePos = mapToScene(transformedPoint);
        
        qreal distance = QLineF(scenePos, controlScenePos).length();
        
        if (distance < minDistance) {
            minDistance = distance;
            nearestIndex = i;
        }
    }
    
    return nearestIndex;
}

bool DrawingPath::isPointNearControlPoint(const QPointF &scenePos, const QPointF &controlPoint, qreal threshold) const
{
    return QLineF(scenePos, controlPoint).length() <= threshold;
}

void DrawingPath::updateFromNodePoints()
{
    updatePathFromControlPoints();
}

void DrawingPath::setControlPoints(const QVector<QPointF> &points)
{
    if (m_controlPoints != points) {
        m_controlPoints = points;
        updatePathFromControlPoints();
    }
}

void DrawingPath::updatePathFromControlPoints()
{
    if (m_controlPoints.isEmpty() || m_controlPointTypes.isEmpty()) {
        return;
    }
    
    QPainterPath newPath;
    
    // 根据控制点类型重建路径
    for (int i = 0; i < m_controlPoints.size(); ++i) {
        QPainterPath::ElementType type = m_controlPointTypes[i];
        QPointF point = m_controlPoints[i];
        
        if (type == QPainterPath::MoveToElement) {
            newPath.moveTo(point);
        } else if (type == QPainterPath::LineToElement) {
            newPath.lineTo(point);
        } else if (type == QPainterPath::CurveToElement) {
            // 曲线需要3个点：当前点和接下来的两个点
            if (i + 2 < m_controlPoints.size() && 
                i + 2 < m_controlPointTypes.size() &&
                m_controlPointTypes[i + 1] == QPainterPath::CurveToDataElement &&
                m_controlPointTypes[i + 2] == QPainterPath::CurveToDataElement) {
                newPath.cubicTo(point, m_controlPoints[i + 1], m_controlPoints[i + 2]);
                i += 2; // 跳过已经处理的两个控制点
            }
        }
        // CurveToDataElement在CurveToElement处理时已经考虑
    }
    
    // 直接更新内部路径，不调用setPath避免无限循环
    prepareGeometryChange();
    m_path = newPath;
    update();
}

void DrawingPath::setShowControlPolygon(bool show)
{
    m_showControlPolygon = show;
    update(); // 触发重绘
}

bool DrawingPath::showControlPolygon() const
{
    return m_showControlPolygon;
}

void DrawingPath::paintShape(QPainter *painter)
{
    painter->drawPath(m_path);
    
    // 如果启用了控制点连线，则绘制连接线
    if (m_showControlPolygon) {
        QPen oldPen = painter->pen();
        QBrush oldBrush = painter->brush();
        
        // 设置虚线样式
        QPen controlPen(Qt::DashLine);
        controlPen.setColor(QColor(100, 100, 255, 128)); // 半透明蓝色
        controlPen.setWidth(1);
        controlPen.setCosmetic(true); // 确保线宽不受变换影响
        
        painter->setPen(controlPen);
        painter->setBrush(Qt::NoBrush);  // 不填充
        
        // 绘制控制点连线 - 按照贝塞尔曲线的规范连接起点和控制点
        if (m_controlPoints.size() >= 2) {
            // 遍历所有贝塞尔段
            for (int i = 0; i < m_controlPoints.size() - 1; ) {
                // 检查是否可以形成一个三次贝塞尔曲线 (起点, 控制点1, 控制点2, 终点)
                if (i + 3 < m_controlPoints.size()) {
                    // 绘制起点到第一个控制点的连线
                    painter->drawLine(m_controlPoints[i], m_controlPoints[i+1]);
                    // 绘制第二个控制点到终点的连线
                    painter->drawLine(m_controlPoints[i+2], m_controlPoints[i+3]);
                    i += 3; // 移动到下一个段的起点
                } 
                // 检查是否可以形成一个二次贝塞尔曲线 (起点, 控制点, 终点)
                else if (i + 2 < m_controlPoints.size()) {
                    // 绘制起点到控制点的连线
                    painter->drawLine(m_controlPoints[i], m_controlPoints[i+1]);
                    // 绘制控制点到终点的连线
                    painter->drawLine(m_controlPoints[i+1], m_controlPoints[i+2]);
                    i += 2; // 移动到下一个段的起点
                }
                // 否则，是直线段
                else {
                    painter->drawLine(m_controlPoints[i], m_controlPoints[i+1]);
                    i += 1;
                }
            }
        }
        
        // 绘制控制点（圆形，以区别于方形节点）
        QPen pointPen(Qt::SolidLine);
        pointPen.setColor(QColor(100, 100, 255, 200)); // 更不透明的蓝色
        pointPen.setWidth(1);
        pointPen.setCosmetic(true);
        
        QBrush pointBrush(QColor(200, 200, 255, 180)); // 浅蓝色填充
        
        painter->setPen(pointPen);
        painter->setBrush(pointBrush);
        
        // 绘制所有控制点为圆形
        const qreal pointRadius = 4.0; // 控制点半径，加大1个像素
        for (const QPointF &point : m_controlPoints) {
            painter->drawEllipse(point, pointRadius, pointRadius);
        }
        
        // 恢复原始画笔和画刷
        painter->setPen(oldPen);
        painter->setBrush(oldBrush);
    }
}

// DrawingText
DrawingText::DrawingText(const QString &text, QGraphicsItem *parent)
    : DrawingShape(Text, parent)
    , m_text(text)
    , m_font(QFont("Arial", 12))
    , m_position(0, 0)
    , m_fontSize(12.0)
    , m_editing(false)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

QRectF DrawingText::localBounds() const
{
    QFontMetricsF metrics(m_font);
    // 使用tightBoundingRect获得更准确的边界框
    QRectF textRect = metrics.tightBoundingRect(m_text);
    // 调整位置，考虑基线偏移
    textRect.moveTopLeft(m_position + QPointF(0, -metrics.ascent()));
    // 增加更多边距，避免控制手柄遮挡文字，底部增加更多边距
    return textRect.adjusted(-8, -8, 8, 12); // 底部边距增加到12像素
}

void DrawingText::setText(const QString &text)
{
    if (m_text != text) {
        prepareGeometryChange();
        m_text = text;
        update();
    }
}

void DrawingText::setFont(const QFont &font)
{
    if (m_font != font) {
        prepareGeometryChange();
        m_font = font;
        m_fontSize = font.pointSizeF();
        update();
    }
}

void DrawingText::setPosition(const QPointF &pos)
{
    if (m_position != pos) {
        prepareGeometryChange();
        m_position = pos;
        update();
    }
}

void DrawingText::setPos(const QPointF &pos)
{
    // 重写setPos以确保位置变化时文本也跟着移动
    setPosition(pos);
    QGraphicsItem::setPos(pos);
}

QVector<QPointF> DrawingText::getNodePoints() const
{
    QVector<QPointF> points;
    points.reserve(2);
    
    // 1. 位置控制点：文本左上角
    points.append(m_position);
    
    // 2. 大小控制点：文本右下角
    QFontMetricsF metrics(m_font);
    QRectF textRect = metrics.boundingRect(m_text);
    points.append(m_position + QPointF(textRect.width(), textRect.height()));
    
    return points;
}

void DrawingText::setNodePoint(int index, const QPointF &pos)
{
    QPointF localPos = mapFromScene(pos);
    
    switch (index) {
        case 0: {
            // 位置控制：移动文本位置
            setPosition(localPos);
            break;
        }
        case 1: {
            // 大小控制：调整字体大小
            qreal deltaX = localPos.x() - m_position.x();
            qreal deltaY = localPos.y() - m_position.y();
            
            // 使用较大的变化值来调整字体大小
            qreal delta = qMax(qAbs(deltaX), qAbs(deltaY));
            if (delta > 5) { // 最小字体大小
                qreal newFontSize = qBound(8.0, delta, 200.0); // 8-200pt范围
                QFont newFont = m_font;
                newFont.setPointSizeF(newFontSize);
                setFont(newFont);
            }
            break;
        }
        default:
            return;
    }
}

QPointF DrawingText::constrainNodePoint(int index, const QPointF &pos) const
{
    switch (index) {
        case 0: {
            // 位置控制：允许自由移动
            return pos;
        }
        case 1: {
            // 大小控制：限制在合理范围内
            QPointF localPos = mapFromScene(pos);
            qreal minX = m_position.x() + 10; // 最小宽度
            qreal minY = m_position.y() + 10; // 最小高度
            localPos.setX(qMax(minX, localPos.x()));
            localPos.setY(qMax(minY, localPos.y()));
            return mapToScene(localPos);
        }
        default:
            return pos;
    }
}

void DrawingText::beginNodeDrag(int index)
{
    Q_UNUSED(index);
    // 文本不需要特殊的拖动开始处理
}

void DrawingText::endNodeDrag(int index)
{
    Q_UNUSED(index);
    // 文本不需要特殊的拖动结束处理
}

void DrawingText::paintShape(QPainter *painter)
{
    painter->setFont(m_font);
    painter->setPen(m_strokePen.color());
    painter->setBrush(Qt::NoBrush);
    
    // 使用正确的文本绘制位置
    painter->drawText(m_position, m_text);
    
    // 如果正在编辑，显示编辑指示器
    if (m_editing) {
        QFontMetricsF metrics(m_font);
        QRectF textRect = metrics.tightBoundingRect(m_text);
        // 调整编辑指示器位置，与边界框一致
        textRect.moveTopLeft(m_position + QPointF(0, -metrics.ascent()));
        
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(textRect);
    }
}

void DrawingText::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 检查是否点击了文本区域，使用与边界框相同的计算方式
        QFontMetricsF metrics(m_font);
        QRectF textRect = metrics.tightBoundingRect(m_text);
        textRect.moveTopLeft(m_position + QPointF(0, -metrics.ascent()));
        
        if (textRect.contains(event->pos())) {
            event->accept();
            return;
        }
    }
    
    QGraphicsItem::mousePressEvent(event);
}

void DrawingText::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 双击进入编辑模式
        m_editing = !m_editing;
        update();
        event->accept();
        return;
    }
    
    QGraphicsItem::mouseDoubleClickEvent(event);
}

// DrawingLine implementation
DrawingLine::DrawingLine(const QLineF &line, QGraphicsItem *parent)
    : DrawingShape(Line, parent)
    , m_line(line)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

QRectF DrawingLine::localBounds() const
{
    return QRectF(m_line.p1(), m_line.p2()).normalized().adjusted(-m_lineWidth/2, -m_lineWidth/2, m_lineWidth/2, m_lineWidth/2);
}

void DrawingLine::setLine(const QLineF &line)
{
    if (m_line != line) {
        prepareGeometryChange();
        m_line = line;
        update();
    }
}

QVector<QPointF> DrawingLine::getNodePoints() const
{
    return {m_line.p1(), m_line.p2()};
}

void DrawingLine::setNodePoint(int index, const QPointF &pos)
{
    if (index == 0) {
        setLine(QLineF(pos, m_line.p2()));
    } else if (index == 1) {
        setLine(QLineF(m_line.p1(), pos));
    }
}

QPointF DrawingLine::constrainNodePoint(int index, const QPointF &pos) const
{
    Q_UNUSED(index)
    return pos; // 直线的端点不需要约束
}

void DrawingLine::beginNodeDrag(int index)
{
    Q_UNUSED(index)
    // 直线拖动不需要特殊处理
}

void DrawingLine::endNodeDrag(int index)
{
    Q_UNUSED(index)
    // 直线拖动结束不需要特殊处理
}

void DrawingLine::paintShape(QPainter *painter)
{
    QPen pen = strokePen();
    pen.setWidthF(m_lineWidth);
    painter->setPen(pen);
    painter->drawLine(m_line);
}

// DrawingPolyline implementation
DrawingPolyline::DrawingPolyline(QGraphicsItem *parent)
    : DrawingShape(Polyline, parent)
{
    qDebug() << "DrawingPolyline::DrawingPolyline - m_closed initialized to:" << m_closed;
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

QRectF DrawingPolyline::localBounds() const
{
    if (m_points.isEmpty()) {
        return QRectF(0, 0, 0, 0);
    }
    
    qreal minX = m_points[0].x();
    qreal minY = m_points[0].y();
    qreal maxX = m_points[0].x();
    qreal maxY = m_points[0].y();
    
    for (const QPointF &point : m_points) {
        minX = qMin(minX, point.x());
        minY = qMin(minY, point.y());
        maxX = qMax(maxX, point.x());
        maxY = qMax(maxY, point.y());
    }
    
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

QPainterPath DrawingPolyline::shape() const
{
    QPainterPath path;
    if (m_points.size() < 2) {
        return path;
    }
    
    // 创建一个稍宽的路径用于选择
    path.moveTo(m_points.first());
    for (int i = 1; i < m_points.size(); ++i) {
        path.lineTo(m_points[i]);
    }
    
    if (m_closed) {
        path.closeSubpath();
    }
    
    // 使用笔宽创建描边路径，以便更容易选择
    QPainterPathStroker stroker;
    stroker.setWidth(qMax(m_lineWidth + 5.0, 8.0)); // 至少8像素宽
    return stroker.createStroke(path);
}

QPainterPath DrawingPolyline::transformedShape() const
{
    // 创建折线路径
    QPainterPath path;
    if (m_points.size() < 2) {
        return path;
    }
    
    path.moveTo(m_points.first());
    for (int i = 1; i < m_points.size(); ++i) {
        path.lineTo(m_points[i]);
    }
    
    // 折线不闭合，除非明确设置了闭合标志
    if (m_closed) {
        path.closeSubpath();
    }
    
    // 应用变换
    path = m_transform.transform().map(path);
    path.setFillRule(Qt::WindingFill);
    return path;
}

void DrawingPolyline::addPoint(const QPointF &point)
{
    m_points.append(point);
    prepareGeometryChange();
    update();
}

void DrawingPolyline::insertPoint(int index, const QPointF &point)
{
    if (index >= 0 && index <= m_points.size()) {
        m_points.insert(index, point);
        prepareGeometryChange();
        update();
    }
}

void DrawingPolyline::removePoint(int index)
{
    if (index >= 0 && index < m_points.size()) {
        m_points.removeAt(index);
        prepareGeometryChange();
        update();
    }
}

void DrawingPolyline::setPoint(int index, const QPointF &point)
{
    if (index >= 0 && index < m_points.size()) {
        m_points[index] = point;
        prepareGeometryChange();
        update();
    }
}

QPointF DrawingPolyline::point(int index) const
{
    if (index >= 0 && index < m_points.size()) {
        return m_points[index];
    }
    return QPointF();
}

QVector<QPointF> DrawingPolyline::getNodePoints() const
{
    return m_points;
}

void DrawingPolyline::setNodePoint(int index, const QPointF &pos)
{
    // pos是场景坐标，需要转换为本地坐标
    if (scene() && scene()->views().size() > 0) {
        QGraphicsView* view = scene()->views().first();
        DrawingView* drawingView = qobject_cast<DrawingView*>(view);
        if (drawingView) {
            // 将场景坐标转换为图形本地坐标
            QPointF localPos = mapFromScene(pos);
            // 应用变换的逆变换
            localPos = transform().transform().inverted().map(localPos);
            setPoint(index, localPos);
            return;
        }
    }
    // 如果无法获取视图，直接使用（可能不正确）
    setPoint(index, pos);
}

QPointF DrawingPolyline::constrainNodePoint(int index, const QPointF &pos) const
{
    Q_UNUSED(index)
    return pos; // 折线点不需要约束
}

void DrawingPolyline::beginNodeDrag(int index)
{
    Q_UNUSED(index)
    // 折线点拖动不需要特殊处理
}

void DrawingPolyline::endNodeDrag(int index)
{
    Q_UNUSED(index)
    // 折线点拖动结束不需要特殊处理
}

void DrawingPolyline::updateFromNodePoints()
{
    update();
}

void DrawingPolyline::paintShape(QPainter *painter)
{
    if (m_points.size() < 2) {
        return;
    }
    
    QPen pen = strokePen();
    pen.setWidthF(m_lineWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    
    // 直接绘制线段，不使用QPainterPath以避免意外的闭合
    for (int i = 0; i < m_points.size() - 1; ++i) {
        painter->drawLine(m_points[i], m_points[i + 1]);
    }
    
    // 如果明确标记为闭合，则绘制闭合线
    if (m_closed && m_points.size() > 2) {
        qDebug() << "DrawingPolyline: Warning - polyline is marked as closed!";
        qDebug() << "  Points count:" << m_points.size();
        qDebug() << "  Is selected:" << isSelected();
        qDebug() << "  Current tool:" << (scene() && scene()->views().size() > 0 ? 
            (qobject_cast<DrawingView*>(scene()->views().first()) ? 
                QString::fromUtf8(qobject_cast<DrawingView*>(scene()->views().first())->currentTool()->metaObject()->className()) : 
                "Unknown") : "No view");
        // 绘制闭合线
        painter->drawLine(m_points.last(), m_points.first());
        painter->setBrush(fillBrush());
    }
}

void DrawingPolyline::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 检查是否在节点编辑模式下
        DrawingView* view = qobject_cast<DrawingView*>(scene()->views().first());
        bool isNodeEditMode = false;
        if (view && view->currentTool()) {
            // 检查工具类型名称是否包含"node"
            QString toolName = QString::fromUtf8(view->currentTool()->metaObject()->className());
            isNodeEditMode = toolName.contains("NodeEdit", Qt::CaseInsensitive);
        }
        
        if (isNodeEditMode) {
            // 检查是否点击了现有的点
            for (int i = 0; i < m_points.size(); ++i) {
                if (QLineF(event->pos(), m_points[i]).length() < 5.0) {
                    m_activePoint = i;
                    m_dragStartPos = event->pos();
                    return;
                }
            }
            
            // 如果没有点击现有点，添加新点
            addPoint(event->pos());
        }
    }
    
    DrawingShape::mousePressEvent(event);
}

void DrawingPolyline::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activePoint >= 0 && (event->buttons() & Qt::LeftButton)) {
        setPoint(m_activePoint, event->pos());
        return;
    }
    
    DrawingShape::mouseMoveEvent(event);
}

void DrawingPolyline::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_activePoint >= 0) {
        m_activePoint = -1;
        return;
    }
    
    DrawingShape::mouseReleaseEvent(event);
}

// DrawingPolygon implementation
DrawingPolygon::DrawingPolygon(QGraphicsItem *parent)
    : DrawingShape(Polygon, parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

QRectF DrawingPolygon::localBounds() const
{
    if (m_points.isEmpty()) {
        return QRectF(0, 0, 0, 0);
    }
    
    qreal minX = m_points[0].x();
    qreal minY = m_points[0].y();
    qreal maxX = m_points[0].x();
    qreal maxY = m_points[0].y();
    
    for (const QPointF &point : m_points) {
        minX = qMin(minX, point.x());
        minY = qMin(minY, point.y());
        maxX = qMax(maxX, point.x());
        maxY = qMax(maxY, point.y());
    }
    
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

QPainterPath DrawingPolygon::shape() const
{
    QPainterPath path;
    if (m_points.size() < 3) {
        return path;
    }
    
    // 创建多边形路径
    path.moveTo(m_points.first());
    for (int i = 1; i < m_points.size(); ++i) {
        path.lineTo(m_points[i]);
    }
    path.closeSubpath(); // 多边形总是闭合的
    
    return path;
}

QPainterPath DrawingPolygon::transformedShape() const
{
    // 创建多边形路径
    QPainterPath path = shape();
    // 应用变换
    path = m_transform.transform().map(path);
    path.setFillRule(Qt::WindingFill);
    return path;
}

void DrawingPolygon::addPoint(const QPointF &point)
{
    m_points.append(point);
    prepareGeometryChange();
    update();
}

void DrawingPolygon::insertPoint(int index, const QPointF &point)
{
    if (index >= 0 && index <= m_points.size()) {
        m_points.insert(index, point);
        prepareGeometryChange();
        update();
    }
}

void DrawingPolygon::removePoint(int index)
{
    if (index >= 0 && index < m_points.size() && m_points.size() > 3) {
        m_points.removeAt(index);
        prepareGeometryChange();
        update();
    }
}

void DrawingPolygon::setPoint(int index, const QPointF &point)
{
    if (index >= 0 && index < m_points.size()) {
        m_points[index] = point;
        prepareGeometryChange();
        update();
    }
}

QPointF DrawingPolygon::point(int index) const
{
    if (index >= 0 && index < m_points.size()) {
        return m_points[index];
    }
    return QPointF();
}

QVector<QPointF> DrawingPolygon::getNodePoints() const
{
    return m_points;
}

void DrawingPolygon::setNodePoint(int index, const QPointF &pos)
{
    // pos是场景坐标，需要转换为本地坐标
    if (scene() && scene()->views().size() > 0) {
        QGraphicsView* view = scene()->views().first();
        DrawingView* drawingView = qobject_cast<DrawingView*>(view);
        if (drawingView) {
            // 将场景坐标转换为图形本地坐标
            QPointF localPos = mapFromScene(pos);
            // 应用变换的逆变换
            localPos = transform().transform().inverted().map(localPos);
            setPoint(index, localPos);
            return;
        }
    }
    // 如果无法获取视图，直接使用（可能不正确）
    setPoint(index, pos);
}

QPointF DrawingPolygon::constrainNodePoint(int index, const QPointF &pos) const
{
    Q_UNUSED(index)
    return pos; // 多边形点不需要约束
}

void DrawingPolygon::beginNodeDrag(int index)
{
    Q_UNUSED(index)
    // 多边形点拖动不需要特殊处理
}

void DrawingPolygon::endNodeDrag(int index)
{
    Q_UNUSED(index)
    // 多边形点拖动结束不需要特殊处理
}

void DrawingPolygon::updateFromNodePoints()
{
    update();
}

void DrawingPolygon::paintShape(QPainter *painter)
{
    if (m_points.size() < 3) {
        return;
    }
    
    painter->setPen(strokePen());
    painter->setBrush(fillBrush());
    
    QPainterPath path;
    path.moveTo(m_points.first());
    for (int i = 1; i < m_points.size(); ++i) {
        path.lineTo(m_points[i]);
    }
    path.closeSubpath();
    
    painter->drawPath(path);
}

void DrawingPolygon::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 检查是否在节点编辑模式下
        DrawingView* view = qobject_cast<DrawingView*>(scene()->views().first());
        bool isNodeEditMode = false;
        if (view && view->currentTool()) {
            // 检查工具类型名称是否包含"node"
            QString toolName = QString::fromUtf8(view->currentTool()->metaObject()->className());
            isNodeEditMode = toolName.contains("NodeEdit", Qt::CaseInsensitive);
        }
        
        if (isNodeEditMode) {
            // 检查是否点击了现有的点
            for (int i = 0; i < m_points.size(); ++i) {
                if (QLineF(event->pos(), m_points[i]).length() < 5.0) {
                    m_activePoint = i;
                    m_dragStartPos = event->pos();
                    return;
                }
            }
            
            // 如果没有点击现有点，添加新点
            addPoint(event->pos());
        }
    }
    
    DrawingShape::mousePressEvent(event);
}

void DrawingPolygon::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activePoint >= 0 && (event->buttons() & Qt::LeftButton)) {
        setPoint(m_activePoint, event->pos());
        return;
    }
    
    DrawingShape::mouseMoveEvent(event);
}

void DrawingPolygon::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_activePoint >= 0) {
        m_activePoint = -1;
        return;
    }
    
    DrawingShape::mouseReleaseEvent(event);
}



