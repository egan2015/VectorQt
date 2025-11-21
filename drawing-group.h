#ifndef DRAWING_GROUP_H
#define DRAWING_GROUP_H

#include <QGraphicsItemGroup>
#include <QList>
#include <QHash>
#include <QRectF>
#include <QPointF>
#include <QPainter>
#include "drawing-shape.h"


// class EditHandleManager; // 已弃用 - 使用新的手柄系统

class DrawingShape;
class DrawingScene;

/**
 * 绘图组 - 类似 SVG 的 g 元素
 * 使用统一变换矩阵，保持内部元素坐标一致性
 */
class DrawingGroup : public DrawingShape
{
public:
    explicit DrawingGroup(QGraphicsItem *parent = nullptr);
    ~DrawingGroup();
    
    // 添加/移除子元素
    void addItem(DrawingShape *item);
    void removeItem(DrawingShape *item);
    QList<DrawingShape*> items() const { return m_items; }
    
    // 取消组合
    QList<DrawingShape*> ungroup();
    
    // 重写DrawingShape的必要方法
    QRectF localBounds() const override;
    void paintShape(QPainter *painter) override;
    
    // QGraphicsItem重写
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    
    // 🌟 重写setTransform方法，确保变换传播到子项
    void setTransform(const DrawingTransform &transform) override;
    
    // 🌟 添加专门的变换方法（参考control-frame）
    void applyScale(const QPointF &anchor, qreal sx, qreal sy);
    void applyRotation(qreal angle, const QPointF &center);
    
    // 🌟 添加支持手柄类型的完整变换方法（参考drawing-edit-handles）
    void applyScaleWithHandle(int handleType, const QPointF &initialHandlePos, const QPointF &currentHandlePos);
    void applyRotationWithHandle(const QPointF &center, double angleDelta);
    
    // 🌟 添加抓取机制（参考control-frame）
    void grabTransform();
    void ungrabTransform();
    
    // 🌟 添加手柄感知的变换方法（更精确的控制）
    void applyTransformWithHandle(int handleType, const QPointF &initialHandlePos, const QPointF &currentHandlePos);

protected:
    // 变换通知
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    
    // 鼠标事件处理
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QList<DrawingShape*> m_items;
    QHash<DrawingShape*, QTransform> m_initialTransforms; // 保存初始变换
    
    // 🌟 添加变换控制相关变量（参考control-frame）
    QPointF m_lockAnchor;      // 缩放锚点
    QPointF m_lockCenter;      // 旋转中心
    QHash<DrawingShape*, QTransform> m_T0; // 初始变换矩阵（用于旋转）
    
    // 🌟 添加抓取机制相关变量
    QHash<DrawingShape*, QTransform> m_currentTransforms; // 当前变换状态（用于拖拽）
    QRectF m_currentBounds; // 当前边界框（用于拖拽）
};

#endif // DRAWING_GROUP_H