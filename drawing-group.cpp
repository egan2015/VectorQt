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
    
    // 保存子对象的原始场景位置
    QPointF scenePos = item->scenePos();
    
    // 设置父项关系
    item->setParentItem(this);
    
    // 🌟 对于SVG导入的元素，需要转换到组的本地坐标系
    // 使用mapFromScene正确转换位置
    QPointF localPos = this->mapFromScene(scenePos);
    item->setPos(localPos);
    
    // 🌟 不重置子项的变换，保持原始变换
    // item->setTransform(DrawingTransform());
    
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
    
    // 解除父子关系
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
            // 解除父子关系
            item->setParentItem(nullptr);
            
            // 恢复子项的所有能力
            item->setFlag(QGraphicsItem::ItemIsMovable, true);
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            
            // 设置子项的位置到组合对象的场景位置
            item->setPos(groupScenePos);
            
            result.append(item);
        }
    }
    
    // 清空列表
    m_items.clear();
    
    return result;
}

QRectF DrawingGroup::boundingRect() const
{
    if (m_items.isEmpty()) {
        return QRectF(0, 0, 1, 1);
    }
    
    QRectF combinedBounds;
    bool first = true;
    
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            // 获取子项的边界框（包含变换）
            QRectF itemBounds = item->boundingRect();
            // 将子项的边界框转换到组合对象的坐标系
            QTransform itemTransform = item->itemTransform(this);
            QRectF transformedBounds = itemTransform.mapRect(itemBounds);
            
            if (first) {
                combinedBounds = transformedBounds;
                first = false;
            } else {
                combinedBounds |= transformedBounds;
            }
        }
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
    
    // 🌟 总是调用基类，确保拖动功能正常工作
    DrawingShape::mousePressEvent(event);
}

void DrawingGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // 直接调用基类方法
    DrawingShape::mouseMoveEvent(event);
}

void DrawingGroup::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // 直接调用基类方法
    DrawingShape::mouseReleaseEvent(event);
}

void DrawingGroup::setTransform(const DrawingTransform &transform)
{
    // 🌟 参考control-frame的统一旋转逻辑
    if (m_items.isEmpty()) {
        DrawingShape::setTransform(transform);
        QGraphicsItem::setTransform(transform.transform());
        return;
    }
    
    // 🌟 1. 计算整体边界框和中心点（参考control-frame）
    QRectF totalBounds;
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            totalBounds |= item->boundingRect().translated(item->pos());
        }
    }
    
    if (totalBounds.isEmpty()) {
        DrawingShape::setTransform(transform);
        QGraphicsItem::setTransform(transform.transform());
        return;
    }
    
    // 🌟 2. 设置统一的变换中心和锚点（参考control-frame）
    QPointF lockCenter = totalBounds.center();
    QPointF lockAnchor = totalBounds.topLeft(); // 默认使用左上角作为锚点
    
    // 🌟 3. 为每个子项计算相对于统一中心的局部变换矩阵（用于旋转）
    QHash<DrawingShape*, QTransform> T0;
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            QPointF axisLocal = item->mapFromScene(lockCenter);
            QTransform t0;
            t0.translate(axisLocal.x(), axisLocal.y());
            T0[item] = t0;
        }
    }
    
    // 🌟 4. 分析变换矩阵来确定操作类型
    QTransform transformMatrix = transform.transform();
    
    // 检查是否是纯旋转（矩阵的行列式接近1，且没有缩放）
    qreal det = transformMatrix.determinant();
    bool isRotation = qFuzzyCompare(det, 1.0) && 
                     qFuzzyCompare(transformMatrix.m11(), 1.0) && 
                     qFuzzyCompare(transformMatrix.m22(), 1.0) &&
                     (!qFuzzyIsNull(transformMatrix.m12()) || !qFuzzyIsNull(transformMatrix.m21()));
    
    // 🌟 5. 如果是旋转操作，使用control-frame的统一旋转逻辑
    if (isRotation) {
        // 提取旋转角度
        qreal angle = qAtan2(transformMatrix.m12(), transformMatrix.m11());
        
        // 🌟 关键：为所有子项应用相同的旋转角度，使用统一的旋转中心
        for (DrawingShape *item : m_items) {
            if (item && item->parentItem() == this) {
                QTransform R;
                R.translate(T0[item].dx(), T0[item].dy());
                R.rotateRadians(angle);
                R.translate(-T0[item].dx(), -T0[item].dy());
                
                // 应用到初始变换上，保持相对关系
                QTransform combinedTransform = R * m_initialTransforms[item];
                item->setTransform(DrawingTransform(combinedTransform));
            }
        }
    } else {
        // 🌟 6. 缩放操作，使用之前的逻辑
        qreal sx = transformMatrix.m11();
        qreal sy = transformMatrix.m22();
        
        // 限制缩放范围
        sx = qBound(0.01, sx, 100.0);
        sy = qBound(0.01, sy, 100.0);
        
        // 智能锚点选择
        DrawingTransform::AnchorPoint anchor = DrawingTransform::Center;
        QPointF translation = QPointF(transformMatrix.dx(), transformMatrix.dy());
        
        if (!qFuzzyIsNull(translation.x()) || !qFuzzyIsNull(translation.y())) {
            if (translation.x() > 0 && translation.y() > 0) {
                anchor = DrawingTransform::TopLeft;
            } else if (translation.x() < 0 && translation.y() > 0) {
                anchor = DrawingTransform::TopRight;
            } else if (translation.x() > 0 && translation.y() < 0) {
                anchor = DrawingTransform::BottomLeft;
            } else if (translation.x() < 0 && translation.y() < 0) {
                anchor = DrawingTransform::BottomRight;
            }
        }
        
        // 为每个子项应用缩放
        for (DrawingShape *item : m_items) {
            if (item && item->parentItem() == this) {
                DrawingTransform newTransform = m_initialTransforms[item];
                newTransform.scaleAroundAnchor(sx, sy, anchor, totalBounds);
                item->setTransform(newTransform);
            }
        }
    }
    
    // 🌟 7. 应用到组合对象自身
    DrawingShape::setTransform(transform);
    QGraphicsItem::setTransform(transform.transform());
    
    // 更新几何
    prepareGeometryChange();
    update();
    
    // 🌟 强制更新编辑手柄位置 - 关键修复
    // 老的手柄系统已移除，不再需要更新
    // if (editHandleManager()) {
    //     editHandleManager()->updateHandles();
    // }
}

void DrawingGroup::applyScale(const QPointF &anchor, qreal sx, qreal sy)
{
    if (m_items.isEmpty()) {
        return;
    }
    
    // 将锚点转换为组合对象的本地坐标
    QPointF anchorLocal = mapFromScene(anchor);
    
    // 将缩放应用到组合对象本身
    QTransform groupTransform = m_transform.transform();
    QTransform scaleTransform;
    scaleTransform.translate(anchorLocal.x(), anchorLocal.y());
    scaleTransform.scale(sx, sy);
    scaleTransform.translate(-anchorLocal.x(), -anchorLocal.y());
    
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
    
    // 手柄类型映射到锚点（与drawing-edit-handles一致）
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
    
    // 🌟 8. 为每个子项应用变换（完全模仿单个图形的正确逻辑）
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            // 🌟 关键：使用DrawingTransform的scaleAroundAnchor方法，完全模仿单个图形的正确逻辑
            // 基于当前变换状态，但使用DrawingTransform的专业处理
            DrawingTransform baseTransform = m_currentTransforms.contains(item) ? 
                                           DrawingTransform(m_currentTransforms[item]) : 
                                           m_initialTransforms[item];
            
            // 🌟 关键：为每个子项计算相对于锚点的局部变换
            // 将场景坐标中的锚点转换到每个子项的本地坐标系
            QPointF anchorLocal = item->mapFromScene(anchorScenePos);
            
            // 创建缩放变换矩阵 - 使用相对于锚点的正确变换
            QTransform scaleTransform;
            scaleTransform.translate(anchorLocal.x(), anchorLocal.y());
            scaleTransform.scale(sx, sy);
            scaleTransform.translate(-anchorLocal.x(), -anchorLocal.y());
            
            // 应用到基础变换上
            QTransform combinedTransform = scaleTransform * baseTransform.transform();
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
    
    // 🌟 为所有子项应用相同的旋转角度，使用统一的旋转中心
    for (DrawingShape *item : m_items) {
        if (item && item->parentItem() == this) {
            QTransform R;
            R.translate(T0[item].dx(), T0[item].dy());
            R.rotateRadians(angleDelta);
            R.translate(-T0[item].dx(), -T0[item].dy());
            
            // 🌟 关键：应用到当前变换状态，而不是初始变换
            QTransform baseTransform = m_currentTransforms.contains(item) ? 
                                     m_currentTransforms[item] : m_initialTransforms[item];
            QTransform combinedTransform = R * baseTransform;
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
    if (change == ItemTransformChange || change == ItemTransformHasChanged) {
        QTransform newTransform = value.value<QTransform>();
        
        for (DrawingShape *item : m_items) {
            if (item && item->parentItem() == this) {
                // 🌟 应用相同的变换，但保持相对位置
                // 注意：DrawingShape使用DrawingTransform，需要转换
                DrawingTransform drawTransform;
                drawTransform.setTransform(newTransform);
                item->setTransform(drawTransform);
            }
        }
        
        // 更新边界
        prepareGeometryChange();
        update();
        
        // 老的手柄系统已移除，不再需要更新
        // if (editHandleManager()) {
        //     editHandleManager()->updateHandles();
        // }
    }
    
    // 位置变化也需要更新（虽然 Qt 应该自动处理）
    else if (change == ItemPositionChange || change == ItemPositionHasChanged) {
        prepareGeometryChange();
        update();
        
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
    
    return DrawingShape::itemChange(change, value);
}

