#include "drawing-group.h"
#include "drawing-shape.h"
#include "drawing-transform.h"

#include "drawingscene.h"
// #include "selection-layer.h" // 已移除 - 老的选择层系统
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QWidget>
#include <limits>

DrawingGroup::DrawingGroup(QGraphicsItem *parent)
    : DrawingShape(DrawingShape::Group, parent)
{
    // 设置标志，确保组合对象可以接收鼠标事件
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    // 🌟 移除ItemHasNoContents标志，避免阻止变换传播
    // setFlag(QGraphicsItem::ItemHasNoContents, true);
}

DrawingGroup::~DrawingGroup()
{
    // 先清空列表，避免在析构过程中访问
    m_items.clear();
    
    // QGraphicsItemGroup会自动清理子对象
    // 不需要手动删除子对象，它们由scene管理
}

void DrawingGroup::addItem(DrawingShape *item)
{
    if (!item) {
        return;
    }
    
    // 🌟 保存子项的初始变换（参考control-frame）
    m_initialTransforms[item] = item->transform().transform();
    
    // 在设置父子关系之前，将子项的位置转换为相对于组的本地坐标
    // 获取子项在场景中的当前位置
    QPointF scenePos = item->scenePos();
    // 将场景位置转换为组的本地坐标
    QPointF localPos = this->mapFromScene(scenePos);
    // 设置子项在组内的本地位置
    item->setPos(localPos);
    
    // 🌟 设置父子关系，这是使组合对象能够移动的关键
    item->setParentItem(this);  // 设置父子关系
    
    // 🌟 关键修复：重置子项的变换，避免二次变换
    // 子项的位置已经转换为本地坐标，所以变换应该是单位矩阵
    item->setTransform(DrawingTransform());
    
    // 保存到列表
    m_items.append(item);
    
    // 禁用子项的鼠标事件，让组合对象处理所有事件
    item->setFlag(QGraphicsItem::ItemIsMovable, false);
    item->setFlag(QGraphicsItem::ItemIsSelectable, false);
    
    // 更新几何
    prepareGeometryChange();
    update();
}

void DrawingGroup::removeItem(DrawingShape *item)
{
    if (!item || !m_items.contains(item)) {
        return;
    }
    
    // 🌟 解除父子关系前，恢复子项的原始变换
    if (m_initialTransforms.contains(item)) {
        item->setTransform(DrawingTransform(m_initialTransforms[item]));
        m_initialTransforms.remove(item);
    }
    
    // 🌟 解除父子关系
    item->setParentItem(nullptr);
    
    // 从列表移除
    m_items.removeOne(item);
    
    // 恢复子项的所有能力
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    
    // 更新几何
    prepareGeometryChange();
    update();
}

QList<DrawingShape*> DrawingGroup::ungroup()
{
    QList<DrawingShape*> result;
    
    // 获取组合对象的场景位置
    QPointF groupScenePos = scenePos();
    
    // 移除所有子项
    for (DrawingShape *item : m_items) {
        if (item) {
            // 🌟 解除父子关系前，恢复子项的原始变换
            if (m_initialTransforms.contains(item)) {
                item->setTransform(DrawingTransform(m_initialTransforms[item]));
            }
            
            // 解除父子关系
            item->setParentItem(nullptr);
            
            // 恢复子项的所有能力
            item->setFlag(QGraphicsItem::ItemIsMovable, true);
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            
            // 保持子项的相对位置，而不是移动到组合位置
            // 子项的场景位置应该是组合位置加上它们在组合中的位置
            QPointF itemScenePos = mapToScene(item->pos());
            item->setPos(itemScenePos);
            
            result.append(item);
        }
    }
    
    // 清空列表和初始变换映射
    m_items.clear();
    m_initialTransforms.clear();
    
    return result;
}

QRectF DrawingGroup::boundingRect() const
{
    if (m_items.isEmpty()) {
        return QRectF(0, 0, 1, 1);
    }
    
    // 计算所有子项在组坐标系中的边界框
    QRectF combinedBounds;
    bool first = true;
    
    for (DrawingShape *item : m_items) {
        if (item) {
            // 获取子项在组坐标系中的边界框
            QRectF itemBounds = item->boundingRect();
            // 将子项的本地边界框转换到组的坐标系中
            QRectF itemBoundsInGroup = item->mapRectToParent(itemBounds);
            
            if (first) {
                combinedBounds = itemBoundsInGroup;
                first = false;
            } else {
                combinedBounds |= itemBoundsInGroup;
            }
        }
    }
    
    if (combinedBounds.isEmpty()) {
        return QRectF(0, 0, 1, 1);
    }
    
    return combinedBounds;
}

QRectF DrawingGroup::localBounds() const
{
    return boundingRect();
}

void DrawingGroup::paintShape(QPainter *painter)
{
    // 不绘制任何内容，只显示子对象
    Q_UNUSED(painter);
}

QPainterPath DrawingGroup::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}



void DrawingGroup::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // qDebug() << "DrawingGroup::mousePressEvent called on" << this;
    
    // 左键自动选中
    if (event->button() == Qt::LeftButton) {
        setSelected(true);
    }
    
    // 🌟 调用QGraphicsItem的基类方法，确保拖动功能正常工作
    QGraphicsItem::mousePressEvent(event);
}

void DrawingGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // 调用QGraphicsItem的基类方法
    QGraphicsItem::mouseMoveEvent(event);
}

void DrawingGroup::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // 调用QGraphicsItem的基类方法
    QGraphicsItem::mouseReleaseEvent(event);
}

void DrawingGroup::setTransform(const DrawingTransform &transform)
{
    // 🌟 简化变换逻辑，直接调用基类方法
    DrawingShape::setTransform(transform);
    QGraphicsItem::setTransform(transform.transform());
}
void DrawingGroup::applyScale(const QPointF &anchor, qreal sx, qreal sy)
{
    if (m_items.isEmpty()) {
        return;
    }
    
    // 🌟 简化缩放逻辑：直接在组合对象上应用缩放
    // 将锚点转换为组合对象的本地坐标
    QPointF anchorLocal = mapFromScene(anchor);
    
    // 创建新的变换
    DrawingTransform newTransform = m_transform;
    
    // 应用缩放
    newTransform.scale(sx, sy, anchorLocal);
    
    // 设置新变换
    setTransform(newTransform);
}

void DrawingGroup::grabTransform()
{
    // 🌟 保存所有子项的当前变换状态（参考control-frame的grab方法）
    m_currentTransforms.clear();
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            m_currentTransforms[item] = item->transform().transform();
        }
    }
    
    // 保存当前整体边界框（轴对齐边界框）
    QRectF currentBounds;
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            // 获取每个子项的边界框并转换到组合对象坐标系
            QRectF itemBounds = item->boundingRect(); // 获取本地边界框
            QRectF transformedBounds = item->itemTransform(this).mapRect(itemBounds);
            if (currentBounds.isEmpty()) {
                currentBounds = transformedBounds;
            } else {
                currentBounds |= transformedBounds;
            }
        }
    }
    m_currentBounds = currentBounds;
}

void DrawingGroup::ungrabTransform()
{
    // 🌟 释放变换状态（参考control-frame的ungrab方法）
    m_currentTransforms.clear();
}

void DrawingGroup::applyTransformWithHandle(int handleType, const QPointF &initialHandlePos, const QPointF &currentHandlePos)
{
    if (m_items.isEmpty()) {
        return;
    }
    
    // 🌟 基于手柄类型的精确变换处理（参考control-frame逻辑）
    
    // 1. 根据手柄类型确定锚点
    DrawingTransform::AnchorPoint anchor = DrawingTransform::Center;
    
    // 手柄类型映射到锚点
    // 关键：拖动手柄时，固定对角的锚点
    switch (handleType) {
        case 1: // TopLeft
            anchor = DrawingTransform::BottomRight;
            break;
        case 2: // TopCenter
            anchor = DrawingTransform::BottomCenter;
            break;
        case 3: // TopRight
            anchor = DrawingTransform::BottomLeft;
            break;
        case 4: // CenterLeft
            anchor = DrawingTransform::CenterRight;
            break;
        case 5: // CenterRight
            anchor = DrawingTransform::CenterLeft;
            break;
        case 6: // BottomLeft
            anchor = DrawingTransform::TopRight;
            break;
        case 7: // BottomCenter
            anchor = DrawingTransform::TopCenter;
            break;
        case 8: // BottomRight
            anchor = DrawingTransform::TopLeft;
            break;
        default:
            anchor = DrawingTransform::Center;
            break;
    }
    
    // 🌟 2. 使用保存的原始边界框（在grabTransform时保存的）
    QRectF totalBounds = m_currentBounds;
    
    if (totalBounds.isEmpty()) {
        return;
    }
    
    // 🌟 3. 获取固定锚点位置（关键：基于原始状态的边界框）
    QPointF fixedAnchor;
    switch (anchor) {
        case DrawingTransform::TopLeft:
            fixedAnchor = totalBounds.topLeft();
            break;
        case DrawingTransform::TopCenter:
            fixedAnchor = QPointF(totalBounds.center().x(), totalBounds.top());
            break;
        case DrawingTransform::TopRight:
            fixedAnchor = totalBounds.topRight();
            break;
        case DrawingTransform::CenterLeft:
            fixedAnchor = QPointF(totalBounds.left(), totalBounds.center().y());
            break;
        case DrawingTransform::Center:
            fixedAnchor = totalBounds.center();
            break;
        case DrawingTransform::CenterRight:
            fixedAnchor = QPointF(totalBounds.right(), totalBounds.center().y());
            break;
        case DrawingTransform::BottomLeft:
            fixedAnchor = totalBounds.bottomLeft();
            break;
        case DrawingTransform::BottomCenter:
            fixedAnchor = QPointF(totalBounds.center().x(), totalBounds.bottom());
            break;
        case DrawingTransform::BottomRight:
            fixedAnchor = totalBounds.bottomRight();
            break;
        default:
            fixedAnchor = totalBounds.center();
            break;
    }
    
    // 将固定锚点转换为场景坐标
    QPointF anchorScenePos = mapToScene(fixedAnchor);
    
    // 🌟 4. 计算相对于锚点的向量（关键步骤）
    QPointF initialVec = initialHandlePos - anchorScenePos;
    QPointF currentVec = currentHandlePos - anchorScenePos;
    
    // 🌟 5. 计算缩放因子
    double sx = 1.0, sy = 1.0;
    if (!qFuzzyIsNull(initialVec.x())) {
        sx = currentVec.x() / initialVec.x();
    }
    if (!qFuzzyIsNull(initialVec.y())) {
        sy = currentVec.y() / initialVec.y();
    }
    
    // 🌟 6. 根据手柄类型限制缩放方向
    switch (handleType) {
        case 2: // TopCenter
        case 7: // BottomCenter
            sx = 1.0; // 只垂直缩放
            break;
        case 4: // CenterLeft
        case 5: // CenterRight
            sy = 1.0; // 只水平缩放
            break;
        case 1: // TopLeft
        case 3: // TopRight
        case 6: // BottomLeft
        case 8: // BottomRight
            // 角手柄：两个方向都缩放
            break;
        default:
            break;
    }
    
    // 🌟 7. 限制缩放范围
    sx = qBound(0.01, sx, 100.0);
    sy = qBound(0.01, sy, 100.0);
    
    // 🌟 简化缩放逻辑：直接在组合对象上应用缩放
    // 将锚点转换为组合对象的本地坐标
    QPointF anchorLocal = mapFromScene(anchorScenePos);
    
    // 创建新的变换
    DrawingTransform newTransform = m_transform;
    
    // 应用缩放
    newTransform.scale(sx, sy, anchorLocal);
    
    // 设置新变换
    setTransform(newTransform);
    
    // 🌟 更新编辑手柄位置
    // 老的手柄系统已移除，不再需要更新
    // if (editHandleManager()) {
    //     editHandleManager()->updateHandles();
    // }
}

void DrawingGroup::applyRotationWithHandle(const QPointF &center, double angleDelta)
{
    if (m_items.isEmpty()) {
        return;
    }
    
    // 🌟 使用control-frame的统一旋转逻辑
    // 计算整体边界框
    QRectF totalBounds;
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            totalBounds |= item->boundingRect().translated(item->pos());
        }
    }
    
    if (totalBounds.isEmpty()) {
        return;
    }
    
    // 设置统一的旋转中心
    QPointF lockCenter = center.isNull() ? totalBounds.center() : center;
    
    // 为每个子项计算相对于统一中心的局部变换矩阵
    QHash<DrawingShape*, QTransform> T0;
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            QPointF axisLocal = item->mapFromScene(lockCenter);
            QTransform t0;
            t0.translate(axisLocal.x(), axisLocal.y());
            T0[item] = t0;
        }
    }
    
    // 将旋转中心转换为组的本地坐标
    QPointF localCenter = mapFromScene(lockCenter);
    
    // 对组合对象本身应用旋转变换
    QTransform groupTransform = m_transform.transform();
    QTransform rotationTransform;
    rotationTransform.translate(localCenter.x(), localCenter.y());
    rotationTransform.rotateRadians(angleDelta);
    rotationTransform.translate(-localCenter.x(), -localCenter.y());
    
    setTransform(DrawingTransform(rotationTransform * groupTransform));
    
    // 更新几何
    prepareGeometryChange();
    update();
    
    // 🌟 更新编辑手柄位置
    // 老的手柄系统已移除，不再需要更新
    // if (editHandleManager()) {
    //     editHandleManager()->updateHandles();
    // }
}

void DrawingGroup::applyScaleWithHandle(int handleType, const QPointF &initialHandlePos, const QPointF &currentHandlePos)
{
    if (m_items.isEmpty()) {
        return;
    }
    
    // 计算组合对象的中心点
    QRectF totalBounds;
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            QRectF itemBounds = item->boundingRect().translated(item->pos());
            QRectF transformedBounds = item->itemTransform(this).mapRect(itemBounds);
            if (totalBounds.isEmpty()) {
                totalBounds = transformedBounds;
            } else {
                totalBounds |= transformedBounds;
            }
        }
    }
    
    if (totalBounds.isEmpty()) {
        return;
    }
    
    QPointF center = totalBounds.center();
    QPointF centerScenePos = mapToScene(center);
    
    // 计算缩放因子
    QPointF initialVec = initialHandlePos - centerScenePos;
    QPointF currentVec = currentHandlePos - centerScenePos;
    
    double sx = 1.0, sy = 1.0;
    if (!qFuzzyIsNull(initialVec.x())) {
        sx = currentVec.x() / initialVec.x();
    }
    if (!qFuzzyIsNull(initialVec.y())) {
        sy = currentVec.y() / initialVec.y();
    }
    
    // 根据手柄类型限制缩放方向
    switch (handleType) {
        case 2: // TopCenter
        case 7: // BottomCenter
            sx = 1.0; // 只垂直缩放
            break;
        case 4: // CenterLeft
        case 5: // CenterRight
            sy = 1.0; // 只水平缩放
            break;
        default:
            break;
    }
    
    // 限制缩放范围
    sx = qBound(0.01, sx, 100.0);
    sy = qBound(0.01, sy, 100.0);
    
    // 将缩放应用到组合对象本身，而不是分别应用到每个子项
    QTransform groupTransform = m_transform.transform();
    QTransform scaleTransform;
    scaleTransform.translate(center.x(), center.y());
    scaleTransform.scale(sx, sy);
    scaleTransform.translate(-center.x(), -center.y());
    
    setTransform(DrawingTransform(scaleTransform * groupTransform));
    
    // 更新几何
    prepareGeometryChange();
    update();
    
    // 更新编辑手柄位置
    // 老的手柄系统已移除，不再需要更新
    // if (editHandleManager()) {
    //     editHandleManager()->updateHandles();
    // }
}

void DrawingGroup::applyRotation(qreal angle, const QPointF &center)
{
    if (m_items.isEmpty()) {
        return;
    }
    
    // 🌟 对每个子项应用旋转，使用中心点（参考control-frame）
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            QPointF centerLocal = item->mapFromScene(center);
            QTransform R;
            R.translate(centerLocal.x(), centerLocal.y());
            R.rotate(angle);
            R.translate(-centerLocal.x(), -centerLocal.y());
            
            // 应用到初始变换上
            QTransform combinedTransform = R * m_initialTransforms[item];
            item->setTransform(DrawingTransform(combinedTransform));
        }
    }
    
    // 更新几何
    prepareGeometryChange();
    update();
    
    // 🌟 更新编辑手柄位置
    // 老的手柄系统已移除，不再需要更新
    // if (editHandleManager()) {
    //     editHandleManager()->updateHandles();
    // }
}

QVariant DrawingGroup::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value)
{
    // 🌟 在变换发生变化时，同步到所有子项
    if (change == ItemTransformHasChanged) {
        // 更新边界
        prepareGeometryChange();
        update();
    }
    
    // 位置变化也需要更新（虽然 Qt 应该自动处理）
    else if (change == ItemPositionChange || change == ItemPositionHasChanged) {
        // prepareGeometryChange();
        // update();
        
        // 老的手柄系统已移除，不再需要更新
        // if (editHandleManager()) {
        //     editHandleManager()->updateHandles();
        // }
    }
    
    // 老的手柄系统已移除，不再需要更新手柄显示
    else if (change == ItemSelectedHasChanged) {
        // if (editHandleManager()) {
        //     if (isSelected()) {
        //         editHandleManager()->showHandles();
        //     } else {
        //         editHandleManager()->hideHandles();
        //     }
        // }
    }
    
    return QGraphicsItem::itemChange(change, value);
}

