#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QWidget>
#include <QDataStream>
#include <QIODevice>
#include <limits>
#include "../core/drawing-group.h"
#include "../core/drawing-shape.h"
#include "../ui/drawingscene.h"
// #include "selection-layer.h" // 已移除 - 老的选择层系统

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
    if (!item)
    {
        return;
    }

    // 🌟 保存子项的初始变换（参考control-frame）
    m_initialTransforms[item] = item->transform();

    // 检查是否在SVG导入中，如果是，需要特殊处理坐标
    bool isSvgImport = (item->scene() == nullptr);
    if (isSvgImport) {
        // SVG导入时，子元素的位置是相对于SVG坐标系的
        // 需要将其转换为相对于组的本地坐标
        // 由于组还没有变换，子元素应该保持其原始位置
        // 但需要确保坐标系一致
        item->setPos(item->pos()); // 保持原始位置
    } else {
        // 在设置父子关系之前，将子项的位置转换为相对于组的本地坐标
        // 获取子项在场景中的当前位置
        QPointF scenePos = item->scenePos();
        // 将场景位置转换为组的本地坐标
        QPointF localPos = this->mapFromScene(scenePos);
        // 设置子项在组内的本地位置
        item->setPos(localPos);
    }

    // 🌟 设置父子关系，这是使组合对象能够移动的关键
    item->setParentItem(this); // 设置父子关系

    // 🌟 关键修复：重置子项的变换，避免二次变换
    // 子项的位置已经转换为本地坐标，所以变换应该是单位矩阵
    // item->applyTransform(QTransform());

    // 保存到列表
    m_items.append(item);

    // 禁用子项的鼠标事件，让组合对象处理所有事件
    item->setFlag(QGraphicsItem::ItemIsMovable, false);
    item->setFlag(QGraphicsItem::ItemIsSelectable, false);

    // 计算所有子项在组坐标系中的边界框
    QRectF combinedBounds;
    bool first = true;

    for (DrawingShape *item : m_items)
    {
        if (item)
        {
            // 获取子项在组坐标系中的边界框
            QRectF itemBounds = item->boundingRect();
            // 将子项的本地边界框转换到组的坐标系中
            QRectF itemBoundsInGroup = item->mapRectToParent(itemBounds);

            if (first)
            {
                combinedBounds = itemBoundsInGroup;
                first = false;
            }
            else
            {
                combinedBounds |= itemBoundsInGroup;
            }
        }
    }

    m_currentBounds = combinedBounds;
}

void DrawingGroup::removeItem(DrawingShape *item)
{
    if (!item || !m_items.contains(item))
    {
        return;
    }

    QPointF itemScenePos = item->scenePos();
    // 🌟 解除父子关系前，恢复子项的原始变换
    if (m_initialTransforms.contains(item))
    {
        // item->applyTransform(m_initialTransforms[item]);
        m_initialTransforms.remove(item);
    }

    // 🌟 解除父子关系
    item->setParentItem(nullptr);
    item->setPos(itemScenePos);

    // 从列表移除
    m_items.removeOne(item);

    // 恢复子项的所有能力
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
}

QList<DrawingShape *> DrawingGroup::ungroup()
{
    QList<DrawingShape *> result;

    // 移除所有子项
    for (DrawingShape *item : m_items)
    {
        if (item)
        {
            QPointF itemScenePos = item->scenePos();
            // 🌟 解除父子关系前，恢复子项的原始变换
            if (m_initialTransforms.contains(item))
            {
                // item->applyTransform(m_initialTransforms[item]);
            }

            // 解除父子关系
            item->setParentItem(nullptr);

            // 保持子项的相对位置，而不是移动到组合位置
            // 子项的场景位置应该是组合位置加上它们在组合中的位置

            item->setPos(itemScenePos);
            // 恢复子项的所有能力
            item->setFlag(QGraphicsItem::ItemIsMovable, true);
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);

            result.append(item);
        }
    }

    // 清空列表和初始变换映射
    m_items.clear();
    m_initialTransforms.clear();

    return result;
}

QRectF DrawingGroup::localBounds() const
{

    return m_currentBounds;
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

    // 🌟 调用QGraphicsItem的基类方法，确保拖动功能正常工作
    DrawingShape::mousePressEvent(event);
}

void DrawingGroup::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // 调用QGraphicsItem的基类方法
    DrawingShape::mouseMoveEvent(event);
}

void DrawingGroup::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // 调用QGraphicsItem的基类方法
    DrawingShape::mouseReleaseEvent(event);
}

void DrawingGroup::applyTransform(const QTransform &transform, const QPointF &anchor)
{
    // 实现统一变换：对组内每个子对象应用相同的变换
    // 这与选择工具对多个选中对象进行统一变换的方式一致

    // 添加安全检查，确保组对象仍然有效
    if (!scene())
    {
        qDebug() << "Warning: DrawingGroup::applyTransform called on group not in scene";
        return;
    }

    if (m_items.isEmpty())
    {
        // 如果没有子对象，只对组对象本身应用变换
        DrawingShape::applyTransform(transform, anchor);
        return;
    }

    // 如果提供了锚点，使用提供的锚点，否则使用组的中心点作为锚点
    QPointF transformAnchor;
    if (!anchor.isNull())
    {
        transformAnchor = anchor;
    }
    else
    {
        // 计算组的边界框中心点作为默认锚点
        QRectF groupBounds = boundingRect();
        QPointF center = groupBounds.center();
        // 将锚点从组坐标系转换为场景坐标系
        transformAnchor = mapToScene(center);
    }

    // 创建子对象列表的副本，避免迭代器失效
    QList<DrawingShape *> itemsCopy = m_items;

    // 在应用变换前，验证所有子对象的有效性
    QList<DrawingShape *> validItems;
    for (DrawingShape *item : itemsCopy)
    {
        if (item && item->scene() && item->scene() == scene())
        { // 确保对象存在且在同一个场景中
            validItems.append(item);
        }
        else
        {
            qDebug() << "Warning: Invalid item found in group during transform";
        }
    }

    // 再次检查有效项目列表
    if (validItems.isEmpty())
    {
        qDebug() << "Warning: No valid items found for transform in DrawingGroup";
        DrawingShape::applyTransform(transform, anchor);
        return;
    }
    // 对所有子对象应用统一的变换
    for (DrawingShape *item : validItems)
    {
        // 将父对象坐标系的锚点转换为子对象的本地坐标系
        QPointF localAnchor = item->mapFromParent(transformOriginPoint());

        // 构造相对于锚点的变换矩阵
        // 按照规范：变换矩阵必须按照 Translate(-anchor) * Transform * Translate(anchor) 的顺序构造
        QTransform anchoredTransform;
        anchoredTransform.translate(localAnchor.x(), localAnchor.y());
        anchoredTransform = transform * anchoredTransform;
        anchoredTransform.translate(-localAnchor.x(), -localAnchor.y());

        // 直接对子对象应用变换，使用场景坐标锚点
        item->applyTransform(m_initialTransforms[item] * anchoredTransform, localAnchor);
    }

    // 同时也对组对象本身应用变换以保持一致性
    DrawingShape::applyTransform(transform, anchor);
}

// DrawingGroup 序列化方法
QByteArray DrawingGroup::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    
    // 写入类型标识
    stream << static_cast<int>(DrawingShape::Group);
    
    // 写入位置信息
    stream << pos();
    stream << scale();
    stream << rotation();
    stream << transform();
    stream << zValue();
    stream << isVisible();
    stream << isEnabled();
    stream << fillBrush();
    stream << strokePen();
    stream << opacity();
    stream << id();
    
    // 写入子对象数量
    stream << m_items.count();
    qDebug() << "DrawingGroup::serialize: serializing group with" << m_items.count() << "children";
    
    // 序列化每个子对象
    for (DrawingShape *item : m_items) {
        if (item) {
            qDebug() << "DrawingGroup::serialize: serializing child item type:" << item->shapeType() << "pos:" << item->pos();
            // 子对象的位置已经是相对于组合对象的本地坐标
            QByteArray itemData = item->serialize();
            stream << itemData;
        }
    }
    
    return data;
}

void DrawingGroup::deserialize(const QByteArray &data)
{
    QDataStream stream(data);
    
    // 读取类型标识
    int typeValue;
    stream >> typeValue;
    
    // 读取位置信息
    QPointF position;
    stream >> position;
    setPos(position);
    
    qreal scaleValue;
    stream >> scaleValue;
    setScale(scaleValue);
    
    qreal rotationValue;
    stream >> rotationValue;
    setRotation(rotationValue);
    
    QTransform transform;
    stream >> transform;
    setTransform(transform);
    
    qreal zValue;
    stream >> zValue;
    setZValue(zValue);
    
    bool visible;
    stream >> visible;
    setVisible(visible);
    
    bool enabled;
    stream >> enabled;
    setEnabled(enabled);
    
    QBrush fillBrush;
    stream >> fillBrush;
    setFillBrush(fillBrush);
    
    QPen strokePen;
    stream >> strokePen;
    setStrokePen(strokePen);
    
    qreal opacity;
    stream >> opacity;
    setOpacity(opacity);
    
    QString id;
    stream >> id;
    setId(id);
    
    // 读取子对象数量
    int itemCount;
    stream >> itemCount;
    qDebug() << "DrawingGroup::deserialize: reading group with" << itemCount << "children";
    
    // 清空现有子对象
    m_items.clear();
    
    // 反序列化每个子对象
    for (int i = 0; i < itemCount; ++i) {
        QByteArray itemData;
        stream >> itemData;
        
        // 读取形状类型
        QDataStream typeStream(itemData);
        int typeValue;
        typeStream >> typeValue;
        DrawingShape::ShapeType shapeType = static_cast<DrawingShape::ShapeType>(typeValue);
        
        // 创建对应的形状对象
        DrawingShape *item = nullptr;
        switch (shapeType) {
            case DrawingShape::Rectangle:
                item = new DrawingRectangle();
                break;
            case DrawingShape::Ellipse:
                item = new DrawingEllipse();
                break;
            case DrawingShape::Line:
                item = new DrawingLine();
                break;
            case DrawingShape::Path:
                item = new DrawingPath();
                break;
            case DrawingShape::Polyline:
                item = new DrawingPolyline();
                break;
            case DrawingShape::Polygon:
                item = new DrawingPolygon();
                break;
            case DrawingShape::Text:
                item = new DrawingText();
                break;
            case DrawingShape::Group:
                item = new DrawingGroup();
                break;
            default:
                continue;
        }
        
        if (item) {
            item->deserialize(itemData);
            qDebug() << "DrawingGroup::deserialize: deserialized child item type:" << shapeType << "pos:" << item->pos() << "visible:" << item->isVisible();
            
            // 直接设置父子关系，不调用addItem以避免坐标转换
            item->setParentItem(this);
            m_items.append(item);
            
            // 确保子对象可见
            item->setVisible(true);
            
            // 禁用子项的鼠标事件，让组合对象处理所有事件
            item->setFlag(QGraphicsItem::ItemIsMovable, false);
            item->setFlag(QGraphicsItem::ItemIsSelectable, false);
            
            qDebug() << "DrawingGroup::deserialize: child item parent set, visible:" << item->isVisible();
        }
    }
    
    update();
}

DrawingShape* DrawingGroup::clone() const
{
    qDebug() << "DrawingGroup::clone() called";
    
    // 先序列化当前对象的数据
    QByteArray data = serialize();
    qDebug() << "DrawingGroup::clone: serialized data size:" << data.size();
    
    // 创建新的组合对象
    DrawingGroup *copy = new DrawingGroup();
    
    // 反序列化数据到新对象
    copy->deserialize(data);
    qDebug() << "DrawingGroup::clone() completed, child count:" << copy->items().count();
    return copy;
}

QVariant DrawingGroup::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value)
{
    return DrawingShape::itemChange(change, value);
}
