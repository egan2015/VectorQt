#include "handle-item.h"
#include <QGraphicsSceneHoverEvent>
#include <QPen>
#include <QBrush>

// HandleItemBase 实现
HandleItemBase::HandleItemBase(TransformHandle::HandleType type, HandleStyle style)
    : m_handleType(type)
    , m_style(style)
    , m_state(Normal)
    , m_size(8.0)
    , m_normalColor(255, 255, 255, 180)      // 半透明白色
    , m_hoverColor(100, 149, 237, 200)      // 半透明蓝色
    , m_activeColor(255, 100, 100, 220)     // 半透明红色
{
}

QColor HandleItemBase::currentColor() const
{
    switch (m_state) {
    case Normal:
        return m_normalColor;
    case Hover:
        return m_hoverColor;
    case Active:
        return m_activeColor;
    }
    return m_normalColor;
}

// SquareHandleItem 实现
SquareHandleItem::SquareHandleItem(TransformHandle::HandleType type, QGraphicsItem *parent)
    : QGraphicsRectItem(0, 0, 8, 8, parent)
    , HandleItemBase(type, Square)
{
    // 启用半透明效果
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setAcceptHoverEvents(true);
    updateAppearance();
}

void SquareHandleItem::setState(HandleState state)
{
    if (m_state != state) {
        m_state = state;
        updateAppearance();
    }
}

void SquareHandleItem::setSize(qreal size)
{
    m_size = size;
    setRect(0, 0, size, size);
    updateAppearance();
}

void SquareHandleItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    setState(Hover);
}

void SquareHandleItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    setState(Normal);
}

void SquareHandleItem::updateAppearance()
{
    QColor color = currentColor();
    setBrush(QBrush(color));
    setPen(QPen(Qt::black, 1));
    
    // 根据状态调整边框
    if (m_state == Active) {
        setPen(QPen(Qt::black, 2));
    } else if (m_state == Hover) {
        setPen(QPen(Qt::darkBlue, 1.5));
    }
}

// CircleHandleItem 实现
CircleHandleItem::CircleHandleItem(TransformHandle::HandleType type, QGraphicsItem *parent)
    : QGraphicsEllipseItem(0, 0, 8, 8, parent)
    , HandleItemBase(type, Circle)
{
    // 启用半透明效果
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setAcceptHoverEvents(true);
    updateAppearance();
}

void CircleHandleItem::setState(HandleState state)
{
    if (m_state != state) {
        m_state = state;
        updateAppearance();
    }
}

void CircleHandleItem::setSize(qreal size)
{
    m_size = size;
    setRect(0, 0, size, size);
    updateAppearance();
}

void CircleHandleItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    setState(Hover);
}

void CircleHandleItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    setState(Normal);
}

void CircleHandleItem::updateAppearance()
{
    QColor color = currentColor();
    setBrush(QBrush(color));
    setPen(QPen(Qt::black, 1));
    
    // 根据状态调整边框
    if (m_state == Active) {
        setPen(QPen(Qt::black, 2));
    } else if (m_state == Hover) {
        setPen(QPen(Qt::darkBlue, 1.5));
    }
}