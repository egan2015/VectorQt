#include "drawing-group.h"
#include "drawing-shape.h"
#include "../ui/drawingscene.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>

DrawingGroup::DrawingGroup(QGraphicsItem *parent)
    : DrawingShape(DrawingShape::Group, parent)
{
    // 🌟 设置标准标志，让 Qt 处理变换
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    // 🌟 关键：不设置 ItemHasNoContents，让变换正常传播
}

DrawingGroup::~DrawingGroup()
{
    // 清空列表，Qt 会自动管理子对象的生命周期
    m_items.clear();
}

void DrawingGroup::addItem(DrawingShape *item)
{
    if (!item) {
        return;
    }
    
    qDebug() << "Adding item to group - Qt will handle coordinate conversion";
    
    // 🌟 核心：仅设置父子关系，Qt 自动处理：
    // 1. 坐标转换：item->scenePos() → 相对于组的本地坐标
    // 2. 变换传播：组的变换自动应用到 item
    // 3. 边界计算：childrenBoundingRect() 自动包含 item
    
    item->setParentItem(this);
    m_items.append(item);
    
    // 让子对象不再响应独立的事件，由组统一处理
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
    
    qDebug() << "Removing item from group - Qt will restore coordinates";
    
    // 🌟 核心：仅解除父子关系，Qt 自动处理：
    // 1. 坐标恢复：本地坐标 → 场景坐标
    // 2. 变换恢复：移除父对象变换的影响
    // 3. 事件恢复：恢复独立的事件处理能力
    
    item->setParentItem(nullptr);
    m_items.removeOne(item);
    
    // 恢复子对象的能力
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    
    // 更新几何
    prepareGeometryChange();
    update();
}

QList<DrawingShape*> DrawingGroup::ungroup()
{
    QList<DrawingShape*> result;
    
    // 🌟 批量解除父子关系，Qt 自动处理所有坐标转换
    for (DrawingShape *item : m_items) {
        if (item) {
            item->setParentItem(nullptr);
            item->setFlag(QGraphicsItem::ItemIsMovable, true);
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            result.append(item);
        }
    }
    
    // 清空列表
    m_items.clear();
    
    // 更新几何
    prepareGeometryChange();
    update();
    
    return result;
}

QRectF DrawingGroup::boundingRect() const
{
    // 🌟 使用 Qt 的标准方法，自动计算所有子对象的组合边界
    QRectF bounds = childrenBoundingRect();
    
    if (bounds.isEmpty()) {
        return QRectF(0, 0, 1, 1);
    }
    
    return bounds;
}

QRectF DrawingGroup::localBounds() const
{
    // 🌟 对于组对象，本地边界就是边界框
    return boundingRect();
}

void DrawingGroup::paintShape(QPainter *painter)
{
    // 🌟 组对象本身不需要绘制，只显示子对象
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
    // 🌟 简化的鼠标事件处理，让 Qt 处理标准交互
    if (event->button() == Qt::LeftButton) {
        setSelected(true);
    }
    
    QGraphicsItem::mousePressEvent(event);
}

void DrawingGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // 🌟 让 Qt 处理拖动，变换会自动传播到子对象
    QGraphicsItem::mouseMoveEvent(event);
}

void DrawingGroup::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // 🌟 让 Qt 处理释放事件
    QGraphicsItem::mouseReleaseEvent(event);
}

QVariant DrawingGroup::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value)
{
    // 🌟 简化的变化处理，Qt 自动处理变换传播
    if (change == ItemTransformHasChanged || change == ItemPositionHasChanged) {
        // 变换发生变化时，Qt 会自动更新所有子对象
        // 我们只需要通知视图更新
        prepareGeometryChange();
        update();
    }
    
    return QGraphicsItem::itemChange(change, value);
}