#include <QDebug>
#include "object-tree-item.h"
#include "drawing-layer.h"
#include "drawing-shape.h"

// ObjectTreeItem
ObjectTreeItem::ObjectTreeItem(ItemType type, ObjectTreeItem *parent)
    : QObject(parent), m_itemType(type), m_parent(parent), m_isDraggable(true), m_isDropTarget(true)
{
    if (m_parent)
    {
        m_parent->appendChild(this);
    }
}

ObjectTreeItem::~ObjectTreeItem()
{
    qDeleteAll(m_children);
}

void ObjectTreeItem::setParent(ObjectTreeItem *parent)
{
    if (m_parent)
    {
        m_parent->removeChild(this);
    }

    m_parent = parent;

    if (m_parent)
    {
        m_parent->appendChild(this);
    }
}

ObjectTreeItem *ObjectTreeItem::child(int row) const
{
    if (row < 0 || row >= m_children.count())
    {
        return nullptr;
    }
    return m_children.at(row);
}

int ObjectTreeItem::row() const
{
    if (!m_parent)
    {
        return 0;
    }

    return m_parent->m_children.indexOf(const_cast<ObjectTreeItem *>(this));
}

void ObjectTreeItem::appendChild(ObjectTreeItem *child)
{
    if (child && !m_children.contains(child))
    {
        m_children.append(child);
        child->m_parent = this;
        emit childAdded(child);
    }
}

void ObjectTreeItem::insertChild(int row, ObjectTreeItem *child)
{
    if (child && !m_children.contains(child))
    {
        if (row < 0)
        {
            row = 0;
        }
        else if (row > m_children.count())
        {
            row = m_children.count();
        }

        m_children.insert(row, child);
        child->m_parent = this;
        emit childAdded(child);
    }
}

void ObjectTreeItem::removeChild(ObjectTreeItem *child)
{
    if (m_children.removeOne(child))
    {
        child->m_parent = nullptr;
        emit childRemoved(child);
    }
}

ObjectTreeItem *ObjectTreeItem::takeChild(int row)
{
    if (row < 0 || row >= m_children.count())
    {
        return nullptr;
    }

    ObjectTreeItem *child = m_children.takeAt(row);
    child->m_parent = nullptr;
    emit childRemoved(child);
    return child;
}

// LayerTreeItem
LayerTreeItem::LayerTreeItem(DrawingLayer *layer, ObjectTreeItem *parent)
    : ObjectTreeItem(LayerItem, parent), m_layer(layer)
{
    setDraggable(true);
    setDropTarget(true);

    if (m_layer)
    {
        connect(m_layer, &DrawingLayer::nameChanged, this, &LayerTreeItem::onLayerPropertyChanged);
        connect(m_layer, &DrawingLayer::visibilityChanged, this, &LayerTreeItem::onLayerPropertyChanged);
    }
}

QString LayerTreeItem::name() const
{
    return m_layer ? m_layer->name() : QString();
}

void LayerTreeItem::setName(const QString &name)
{
    if (m_layer)
    {
        m_layer->setName(name);
    }
}

bool LayerTreeItem::isVisible() const
{
    return m_layer ? m_layer->isVisible() : true;
}

void LayerTreeItem::setVisible(bool visible)
{
    if (m_layer)
    {
        m_layer->setVisible(visible);
    }
}

bool LayerTreeItem::isLocked() const
{
    return m_layer ? m_layer->isLocked() : false;
}

void LayerTreeItem::setLocked(bool locked)
{
    if (m_layer)
    {
        m_layer->setLocked(locked);
    }
}

QIcon LayerTreeItem::icon() const
{
    // TODO: 返回图层图标
    return QIcon(); // 暂时返回空图标
}

void LayerTreeItem::onLayerPropertyChanged()
{
    emit itemChanged();
}

// ShapeTreeItem
ShapeTreeItem::ShapeTreeItem(DrawingShape *shape, ObjectTreeItem *parent)
    : ObjectTreeItem(ShapeItem, parent), m_shape(shape)
{
    setDraggable(true);
    setDropTarget(false);

    if (m_shape)
    {
        // 根据图形类型生成默认名称
        switch (m_shape->shapeType())
        {
        case DrawingShape::Rectangle:
            m_defaultName = "矩形";
            break;
        case DrawingShape::Ellipse:
            m_defaultName = "椭圆";
            break;
        case DrawingShape::Path:
            m_defaultName = "路径";
            break;
        case DrawingShape::Line:
            m_defaultName = "直线";
            break;
        case DrawingShape::Polyline:
            m_defaultName = "折线";
            break;
        case DrawingShape::Polygon:
            m_defaultName = "多边形";
            break;
        case DrawingShape::Text:
            m_defaultName = "文本";
            break;
        default:
            m_defaultName = "图形";
            break;
        }
    }
}

QString ShapeTreeItem::name() const
{
    return m_defaultName;
}

void ShapeTreeItem::setName(const QString &name)
{
    m_defaultName = name;
}

bool ShapeTreeItem::isVisible() const
{
    return m_shape ? m_shape->isVisible() : true;
}

void ShapeTreeItem::setVisible(bool visible)
{
    if (m_shape)
    {
        m_shape->setVisible(visible);
    }
}

bool ShapeTreeItem::isLocked() const
{
    // TODO: 实现图形锁定功能
    return false;
}

void ShapeTreeItem::setLocked(bool locked)
{
    // TODO: 实现图形锁定功能
    Q_UNUSED(locked)
}

QIcon ShapeTreeItem::icon() const
{
    // TODO: 根据图形类型返回对应图标
    return QIcon(); // 暂时返回空图标
}

void ShapeTreeItem::onShapePropertyChanged()
{
    emit itemChanged();
}