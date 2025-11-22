#ifndef SIMPLIFIED_GROUP_TRANSFORM_H
#define SIMPLIFIED_GROUP_TRANSFORM_H

#include "../core/drawing-shape.h"
#include <QGraphicsItemGroup>
#include <QDebug>

/**
 * @brief 基于 Qt 标准做法的简化组合变换实现
 * 
 * Qt 的 QGraphicsItemGroup 已经处理了大部分复杂的变换逻辑：
 * 1. 父对象的变换自动传播到子对象
 * 2. 坐标系统自动管理（本地→父对象→场景）
 * 3. 不需要手动计算矩阵乘法
 * 
 * 我们只需要：
 * 1. 设置正确的父子关系
 * 2. 让 Qt 处理所有变换
 * 3. 在需要时获取组合边界框
 */
class SimplifiedGroupTransform : public QGraphicsItemGroup
{
public:
    explicit SimplifiedGroupTransform(QGraphicsItem *parent = nullptr)
        : QGraphicsItemGroup(parent)
    {
        // 启用标准标志
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    }
    
    // 🌟 简化的添加方法 - 仅设置父子关系
    void addItem(DrawingShape *item) {
        if (!item) return;
        
        // Qt 会自动处理坐标转换
        // 子对象的 scenePos() 会自动转换为相对于组的本地坐标
        item->setParentItem(this);
        
        // 让子对象不再响应独立的事件
        item->setFlag(QGraphicsItem::ItemIsMovable, false);
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
        
        qDebug() << "Added item to group, Qt handles transforms automatically";
    }
    
    // 🌟 简化的变换应用 - 直接使用 Qt 的标准方法
    void applyTransform(const QTransform &transform) {
        // 直接设置变换，Qt 会自动传播到所有子对象
        setTransform(transform);
        
        // 不需要手动更新子对象！
        // Qt 会自动调用每个子对象的 itemChanged() 方法
    }
    
    // 🌟 获取组合边界框（Qt 自动计算）
    QRectF getGroupBounds() const {
        return childrenBoundingRect();  // Qt 已经提供了这个方法
    }
    
    // 🌟 坐标转换演示
    void demonstrateCoordinateSystems() {
        qDebug() << "=== Qt Coordinate Systems Demo ===";
        
        // 场景坐标
        QPointF scenePos = this->scenePos();
        qDebug() << "Group scene position:" << scenePos;
        
        // 本地坐标（相对于父对象）
        QPointF localPos = pos();
        qDebug() << "Group local position:" << localPos;
        
        // 变换矩阵
        QTransform currentTransform = transform();
        qDebug() << "Group transform matrix:" << currentTransform;
        
        // 子对象坐标转换
        for (QGraphicsItem *child : childItems()) {
            QPointF childScenePos = child->scenePos();
            QPointF childLocalPos = child->pos();
            qDebug() << "Child scene:" << childScenePos << "local:" << childLocalPos;
        }
    }
};

#endif // SIMPLIFIED_GROUP_TRANSFORM_H