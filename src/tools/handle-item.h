#ifndef HANDLE_ITEM_H
#define HANDLE_ITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include "../tools/handle-types.h"

/**
 * @brief 专门的手柄图形基类
 * @details 提供统一的手柄样式、状态管理和交互效果
 */
class HandleItemBase
{
public:
    enum HandleStyle {
        Square,         // 方形手柄
        Circle,         // 圆形手柄
        Diamond,        // 菱形手柄
        Cross,          // 十字手柄
        RotateCircle,   // 带箭头的空心圆（旋转手柄专用）
        Pixmap          // 图标手柄
    };
    
    enum HandleState {
        Normal,     // 正常状态
        Hover,      // 悬停状态
        Active      // 激活状态
    };

protected:
    HandleItemBase(TransformHandle::HandleType type, HandleStyle style = Square);
    virtual ~HandleItemBase() = default;

public:
    // 获取手柄类型
    TransformHandle::HandleType handleType() const { return m_handleType; }
    
    // 设置手柄状态
    virtual void setState(HandleState state) = 0;
    HandleState state() const { return m_state; }
    
    // 设置手柄样式
    virtual void setStyle(HandleStyle style) { m_style = style; updateAppearance(); }
    HandleStyle style() const { return m_style; }
    
    // 设置特定颜色（用于选择工具的红色和绿色十字）
    void setSpecificColor(const QColor &color) { m_specificColor = color; updateAppearance(); }
    
    // 设置大小
    virtual void setSize(qreal size) = 0;
    qreal size() const { return m_size; }
    
    // 颜色管理
    void setNormalColor(const QColor &color) { m_normalColor = color; updateAppearance(); }
    void setHoverColor(const QColor &color) { m_hoverColor = color; updateAppearance(); }
    void setActiveColor(const QColor &color) { m_activeColor = color; updateAppearance(); }

protected:
    // 更新外观
    virtual void updateAppearance() = 0;
    
    // 获取当前状态对应的颜色
    QColor currentColor() const;

protected:
    TransformHandle::HandleType m_handleType;
    HandleStyle m_style;
    HandleState m_state;
    qreal m_size;
    
    QColor m_normalColor;
    QColor m_hoverColor;
    QColor m_activeColor;
    QColor m_specificColor;  // 特定颜色（用于红色和绿色十字）
};

/**
 * @brief 自定义绘制的手柄图形项
 */
class CustomHandleItem : public QGraphicsItem, public HandleItemBase
{
public:
    explicit CustomHandleItem(TransformHandle::HandleType type, QGraphicsItem *parent = nullptr);
    
    // QGraphicsItem接口
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    
    // 重写基类方法
    void setState(HandleState state) override;
    void setSize(qreal size) override;
    
    // 鼠标事件处理
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

protected:
    void updateAppearance() override;
    void drawSquareHandle(QPainter *painter);
    void drawCircleHandle(QPainter *painter);
    void drawDiamondHandle(QPainter *painter);
    void drawCrossHandle(QPainter *painter);
    void drawRotateCircleHandle(QPainter *painter);

private:
    QRectF m_boundingRect;
};

#endif // HANDLE_ITEM_H