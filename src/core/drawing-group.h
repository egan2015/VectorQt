#ifndef DRAWING_GROUP_H
#define DRAWING_GROUP_H

#include <QGraphicsItemGroup>
#include <QList>
#include "../core/drawing-shape.h"

class DrawingShape;

/**
 * 绘图组 - 基于 Qt Graphics View 框架的简洁实现
 * 
 * 核心原理：
 * 1. 使用 Qt 的父子关系管理坐标系统
 * 2. 变换自动从父对象传播到子对象
 * 3. 不需要手动管理复杂的矩阵运算
 * 
 * Qt 自动处理：
 * - 坐标转换：本地坐标 → 父对象坐标 → 场景坐标
 * - 变换传播：父对象变换自动应用到所有子对象
 * - 边界计算：childrenBoundingRect() 自动计算组合边界
 */
class DrawingGroup : public DrawingShape
{
public:
    explicit DrawingGroup(QGraphicsItem *parent = nullptr);
    ~DrawingGroup();
    
    // 🌟 核心方法：添加/移除子元素
    void addItem(DrawingShape *item);
    void removeItem(DrawingShape *item);
    QList<DrawingShape*> items() const { return m_items; }
    
    // 🌟 取消组合：解除父子关系
    QList<DrawingShape*> ungroup();
    
    // 重写DrawingShape的必要方法
    QRectF localBounds() const override;
    void paintShape(QPainter *painter) override;
    
    // QGraphicsItem重写
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

protected:
    // 变换通知
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    
    // 鼠标事件处理（简化版）
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QList<DrawingShape*> m_items;  // 子对象列表
    
    // 🌟 Qt 已经处理的功能，我们不再需要：
    // ❌ QHash<DrawingShape*, QTransform> m_initialTransforms;  // Qt 自动管理
    // ❌ 复杂的变换方法（applyScale, applyRotation等）      // 使用标准Qt API
    // ❌ 手柄感知变换（applyTransformWithHandle）             // 由选择工具处理
    // ❌ 抓取机制（grabTransform/ungrabTransform）           // Qt 自动处理
    // ❌ 手动边界计算（m_currentBounds）                      // 使用childrenBoundingRect()
};

#endif // DRAWING_GROUP_H