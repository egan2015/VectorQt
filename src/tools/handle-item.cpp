#include <QGraphicsSceneHoverEvent>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include "handle-item.h"

// HandleItemBase 实现
HandleItemBase::HandleItemBase(TransformHandle::HandleType type, HandleStyle style)
    : m_handleType(type), m_style(style), m_state(Normal), m_size(8.0), m_normalColor(255, 255, 255, 180) // 半透明白色
      ,
      m_hoverColor(100, 149, 237, 200) // 半透明蓝色
      ,
      m_activeColor(255, 100, 100, 220) // 半透明红色
      ,
      m_specificColor(Qt::transparent) // 透明，表示使用默认颜色
{
}

QColor HandleItemBase::currentColor() const
{
    switch (m_state)
    {
    case Normal:
        return m_normalColor;
    case Hover:
        return m_hoverColor;
    case Active:
        return m_activeColor;
    }
    return m_normalColor;
}

// CustomHandleItem 实现
CustomHandleItem::CustomHandleItem()
    : QGraphicsItem(nullptr), HandleItemBase(TransformHandle::None, Square), m_boundingRect(-4.0, -4.0, 8.0, 8.0) // 使用固定大小，稍后在setSize中更新
{
    // 启用半透明效果
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setAcceptHoverEvents(true);
}

CustomHandleItem::CustomHandleItem(TransformHandle::HandleType type, QGraphicsItem *parent)
    : QGraphicsItem(parent), HandleItemBase(type, Square), m_boundingRect(-m_size / 2, -m_size / 2, m_size, m_size)
{
    // 启用半透明效果
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setAcceptHoverEvents(true);

    // 根据手柄类型设置样式
    switch (type)
    {
    case TransformHandle::Center:
        m_style = Circle;
        // 为中心手柄设置更淡的浅蓝色
        setSpecificColor(QColor(173, 216, 230, 160)); // 更淡的半透明浅蓝色
        break;
    case TransformHandle::Rotate:
        m_style = RotateCircle;
        break;
    default:
        m_style = Square;
        break;
    }

    updateAppearance();
}

QRectF CustomHandleItem::boundingRect() const
{
    return m_boundingRect;
}

void CustomHandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing, true);

    QColor color;

    // 如果设置了特定颜色（用于选择工具的红色和绿色十字，以及中心手柄的浅蓝色），使用特定颜色
    if (m_specificColor != Qt::transparent)
    {
        color = m_specificColor;
        // 特定颜色也需要状态调整
        if (m_state == Hover)
        {
            color = color.lighter(120);
        }
        else if (m_state == Active)
        {
            // 对于特定颜色，激活时稍微变亮
            color = color.lighter(110);
        }
    }
    else
    {
        color = currentColor();

        // 根据状态调整颜色
        if (m_state == Hover)
        {
            color = color.lighter(120);
        }
        else if (m_state == Active)
        {
            color = QColor(52, 152, 219); // 明亮的蓝色
        }
    }

    QPen pen(Qt::black, 1);
    if (m_state == Active)
    {
        pen.setWidth(2);
    }
    else if (m_state == Hover)
    {
        pen.setColor(QColor(0, 100, 200));
        pen.setWidth(2);
    }

    painter->setPen(pen);
    painter->setBrush(QBrush(color));

    // 根据样式绘制不同形状
    switch (m_style)
    {
    case Square:
        drawSquareHandle(painter);
        break;
    case Circle:
        drawCircleHandle(painter);
        break;
    case Diamond:
        drawDiamondHandle(painter);
        break;
    case Cross:
        drawCrossHandle(painter);
        break;
    case RotateCircle:
        drawRotateCircleHandle(painter);
        break;
    default:
        drawSquareHandle(painter);
        break;
    }
}

void CustomHandleItem::setState(HandleState state)
{
    if (m_state != state)
    {
        m_state = state;
        update();
    }
}

void CustomHandleItem::setSize(qreal size)
{
    m_size = size;
    m_boundingRect = QRectF(-size / 2, -size / 2, size, size);
    update();
}

void CustomHandleItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    setState(Hover);
}

void CustomHandleItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    setState(Normal);
}

void CustomHandleItem::setHandleType(TransformHandle::HandleType type)
{
    m_handleType = type;

    // 根据手柄类型设置样式
    switch (type)
    {
    case TransformHandle::Center:
        m_style = Circle;
        break;
    case TransformHandle::Rotate:
        m_style = RotateCircle;
        break;
    default:
        m_style = Square;
        break;
    }

    updateAppearance();
}

void CustomHandleItem::updateAppearance()
{
    update();
}

void CustomHandleItem::drawSquareHandle(QPainter *painter)
{
    QRectF rect(-m_size / 2, -m_size / 2, m_size, m_size);
    painter->drawRect(rect);

    // 添加内部装饰线条
    QColor lineColor = painter->pen().color();
    lineColor.setAlpha(100);
    painter->setPen(QPen(lineColor, 0.5));

    qreal inset = m_size * 0.3;
    QRectF innerRect(-m_size / 2 + inset, -m_size / 2 + inset, m_size - 2 * inset, m_size - 2 * inset);
    painter->drawRect(innerRect);
}

void CustomHandleItem::drawCircleHandle(QPainter *painter)
{
    QRectF rect(-m_size / 2, -m_size / 2, m_size, m_size);
    painter->drawEllipse(rect);

    // 添加内部装饰圆圈
    QColor lineColor = painter->pen().color();
    lineColor.setAlpha(100);
    painter->setPen(QPen(lineColor, 0.5));

    qreal inset = m_size * 0.3;
    QRectF innerRect(-m_size / 2 + inset, -m_size / 2 + inset, m_size - 2 * inset, m_size - 2 * inset);
    painter->drawEllipse(innerRect);
}

void CustomHandleItem::drawDiamondHandle(QPainter *painter)
{
    QPolygonF diamond;
    diamond << QPointF(0, -m_size / 2)  // 上
            << QPointF(m_size / 2, 0)   // 右
            << QPointF(0, m_size / 2)   // 下
            << QPointF(-m_size / 2, 0); // 左

    painter->drawPolygon(diamond);

    // 添加内部装饰菱形
    QColor lineColor = painter->pen().color();
    lineColor.setAlpha(100);
    painter->setPen(QPen(lineColor, 0.5));

    qreal inset = m_size * 0.3;
    QPolygonF innerDiamond;
    innerDiamond << QPointF(0, -m_size / 2 + inset)
                 << QPointF(m_size / 2 - inset, 0)
                 << QPointF(0, m_size / 2 - inset)
                 << QPointF(-m_size / 2 + inset, 0);
    painter->drawPolygon(innerDiamond);
}

void CustomHandleItem::drawCrossHandle(QPainter *painter)
{
    qreal halfSize = m_size / 2.0;
    qreal thickness = m_size * 0.08; // 十字线条粗细（更细）
    qreal length = m_size * 0.8;     // 十字线条长度（更长）

    // 绘制垂直线
    QRectF verticalRect(-thickness / 2, -length / 2, thickness, length);
    painter->drawRect(verticalRect);

    // 绘制水平线
    QRectF horizontalRect(-length / 2, -thickness / 2, length, thickness);
    painter->drawRect(horizontalRect);

    // 添加中心圆点装饰
    QColor centerColor = painter->pen().color();
    centerColor.setAlpha(150);
    painter->setPen(QPen(centerColor, 0.5));
    painter->setBrush(QBrush(centerColor));

    qreal centerSize = m_size * 0.1; // 中心点更小
    QRectF centerRect(-centerSize / 2, -centerSize / 2, centerSize, centerSize);
    painter->drawEllipse(centerRect);
}

void CustomHandleItem::drawRotateCircleHandle(QPainter *painter)
{
    qreal radius = m_size / 2.0;

    // 使用半透明的Visio风格绿色
    QColor visioGreen = QColor(173, 216, 230, 160); // 半透明Visio绿色
    painter->setBrush(QBrush(visioGreen));
    //painter->setPen(QPen(QColor(0, 100, 0, 200), 1)); // 半透明深绿色边框

    // 绘制半透明实心圆
    QRectF circleRect(-radius, -radius, m_size, m_size);
    painter->drawEllipse(circleRect);

    // 添加内部装饰圆圈（更透明）
    QColor innerColor = QColor(0, 80, 0, 100); // 更透明的深绿色
    painter->setPen(QPen(innerColor, 0.5));
    painter->setBrush(Qt::NoBrush);

    qreal innerRadius = radius * 0.6;
    QRectF innerRect(-innerRadius, -innerRadius, innerRadius * 2, innerRadius * 2);
    painter->drawEllipse(innerRect);
}