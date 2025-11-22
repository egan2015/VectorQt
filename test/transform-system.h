#ifndef TRANSFORM_SYSTEM_H
#define TRANSFORM_SYSTEM_H

#include <QPointF>
#include <QSizeF>
#include <QRectF>
#include <QList>
#include <QVariant>
#include <memory>
#include <QDebug>
#include <QTransform>

/**
 * 新的变换系统 - 基于Inkscape的设计理念
 * 将变换分解为独立操作，每个对象保持本地坐标
 */

// 前向声明
class TransformObject;

/**
 * 变换操作基类
 */
class TransformOperation
{
public:
    enum Type {
        Translate,
        Rotate,
        Scale,
        Shear,
        Matrix  // 用于复杂的矩阵变换
    };
    
    TransformOperation(Type type) : m_type(type) {}
    virtual ~TransformOperation() {}
    
    Type type() const { return m_type; }
    
    // 应用到点
    virtual QPointF apply(const QPointF &point) const = 0;
    
    // 获取逆操作
    virtual std::shared_ptr<TransformOperation> inverse() const = 0;
    
    // 转换为QTransform（用于渲染）
    virtual QTransform toTransform() const = 0;
    
    // 克隆操作
    virtual std::shared_ptr<TransformOperation> clone() const = 0;

protected:
    Type m_type;
};

/**
 * 平移操作
 */
class TranslateOperation : public TransformOperation
{
public:
    TranslateOperation(const QPointF &delta) 
        : TransformOperation(Translate), m_delta(delta) {}
    
    QPointF apply(const QPointF &point) const override {
        return point + m_delta;
    }
    
    std::shared_ptr<TransformOperation> inverse() const override {
        return std::make_shared<TranslateOperation>(-m_delta);
    }
    
    QTransform toTransform() const override {
        return QTransform::fromTranslate(m_delta.x(), m_delta.y());
    }
    
    std::shared_ptr<TransformOperation> clone() const override {
        return std::make_shared<TranslateOperation>(m_delta);
    }
    
    QPointF delta() const { return m_delta; }
    
private:
    QPointF m_delta;
};

/**
 * 旋转操作
 */
class RotateOperation : public TransformOperation
{
public:
    RotateOperation(double angle, const QPointF &center = QPointF())
        : TransformOperation(Rotate), m_angle(angle), m_center(center) {}
    
    QPointF apply(const QPointF &point) const override {
        // 先平移到原点
        QPointF p = point - m_center;
        // 旋转
        double cos_a = qCos(m_angle * M_PI / 180.0);
        double sin_a = qSin(m_angle * M_PI / 180.0);
        QPointF rotated(p.x() * cos_a - p.y() * sin_a,
                       p.x() * sin_a + p.y() * cos_a);
        // 平移回去
        return rotated + m_center;
    }
    
    std::shared_ptr<TransformOperation> inverse() const override {
        return std::make_shared<RotateOperation>(-m_angle, m_center);
    }
    
    QTransform toTransform() const override {
        QTransform t;
        t.translate(m_center.x(), m_center.y());
        t.rotate(m_angle);
        t.translate(-m_center.x(), -m_center.y());
        return t;
    }
    
    std::shared_ptr<TransformOperation> clone() const override {
        return std::make_shared<RotateOperation>(m_angle, m_center);
    }
    
    double angle() const { return m_angle; }
    QPointF center() const { return m_center; }
    
private:
    double m_angle;
    QPointF m_center;
};

/**
 * 缩放操作
 */
class ScaleOperation : public TransformOperation
{
public:
    ScaleOperation(double sx, double sy, const QPointF &center = QPointF())
        : TransformOperation(Scale), m_sx(sx), m_sy(sy), m_center(center) {}
    
    QPointF apply(const QPointF &point) const override {
        QPointF p = point - m_center;
        p.setX(p.x() * m_sx);
        p.setY(p.y() * m_sy);
        return p + m_center;
    }
    
    std::shared_ptr<TransformOperation> inverse() const override {
        return std::make_shared<ScaleOperation>(
            ScaleOperation(1.0/m_sx, 1.0/m_sy, m_center));
    }
    
    QTransform toTransform() const override {
        QTransform t;
        t.translate(m_center.x(), m_center.y());
        t.scale(m_sx, m_sy);
        t.translate(-m_center.x(), -m_center.y());
        return t;
    }
    
    std::shared_ptr<TransformOperation> clone() const override {
        return std::make_shared<ScaleOperation>(m_sx, m_sy, m_center);
    }
    
    double scaleX() const { return m_sx; }
    double scaleY() const { return m_sy; }
    QPointF center() const { return m_center; }
    
private:
    double m_sx, m_sy;
    QPointF m_center;
};

/**
 * 斜切操作
 */
class ShearOperation : public TransformOperation
{
public:
    ShearOperation(double sh, double sv, const QPointF &center = QPointF())
        : TransformOperation(Shear), m_sh(sh), m_sv(sv), m_center(center) {}
    
    QPointF apply(const QPointF &point) const override {
        QPointF p = point - m_center;
        // 应用斜切
        QPointF sheared(p.x() + m_sh * p.y(), 
                       m_sv * p.x() + p.y());
        return sheared + m_center;
    }
    
    std::shared_ptr<TransformOperation> inverse() const override {
        // 计算逆斜切矩阵
        double det = 1.0 - m_sh * m_sv;
        if (qAbs(det) < 1e-10) {
            // 奇异矩阵，返回恒等变换
            return std::make_shared<ShearOperation>(0, 0, m_center);
        }
        double inv_sh = -m_sh / det;
        double inv_sv = -m_sv / det;
        return std::make_shared<ShearOperation>(inv_sh, inv_sv, m_center);
    }
    
    QTransform toTransform() const override {
        QTransform t;
        t.translate(m_center.x(), m_center.y());
        t.shear(m_sh, m_sv);
        t.translate(-m_center.x(), -m_center.y());
        return t;
    }
    
    std::shared_ptr<TransformOperation> clone() const override {
        return std::make_shared<ShearOperation>(m_sh, m_sv, m_center);
    }
    
    double shearX() const { return m_sh; }
    double shearY() const { return m_sv; }
    QPointF center() const { return m_center; }
    
private:
    double m_sh, m_sv;  // sh: 水平斜切, sv: 垂直斜切
    QPointF m_center;
};

/**
 * 变换对象 - 管理一个对象的所有变换
 */
class TransformObject
{
public:
    TransformObject(const QRectF &localBounds);
    
    // 获取本地边界（不变）
    QRectF localBounds() const { return m_localBounds; }
    
    // 应用变换操作
    void addOperation(std::shared_ptr<TransformOperation> op);
    void removeOperation(int index);
    void clearOperations();
    
    // 获取变换后的边界
    QRectF transformedBounds() const;
    
    // 坐标转换
    QPointF mapToScene(const QPointF &localPos) const;
    QPointF mapToLocal(const QPointF &scenePos) const;
    
    // 获取组合变换矩阵（用于渲染）
    QTransform combinedTransform() const;
    
    // 获取当前变换参数
    QPointF translation() const;
    double rotation() const;
    QPointF scale() const;
    QPointF shear() const;
    
    // 操作历史
    const QList<std::shared_ptr<TransformOperation>>& operations() const { return m_operations; }
    
    // 保存/恢复状态
    void saveState();
    void restoreState();
    
private:
    QRectF m_localBounds;  // 原始本地边界
    QList<std::shared_ptr<TransformOperation>> m_operations;  // 变换操作序列
    QList<std::shared_ptr<TransformOperation>> m_savedState;   // 保存的状态
    
    // 缓存的组合变换
    mutable QTransform m_cachedTransform;
    mutable bool m_transformDirty;
    
    void updateCache() const;
};

/**
 * 变换管理器 - 管理多个对象的变换
 */
class TransformManager
{
public:
    TransformManager();
    
    // 添加/移除对象
    void addObject(TransformObject *obj);
    void removeObject(TransformObject *obj);
    void clearObjects();
    
    // 批量操作
    void translateSelection(const QPointF &delta);
    void rotateSelection(double angle, const QPointF &center);
    void scaleSelection(double sx, double sy, const QPointF &center);
    void shearSelection(double sh, double sv, const QPointF &center);
    
    // 获取选中的边界
    QRectF selectionBounds() const;
    
    // 撤销/重做支持
    void saveSelectionState();
    void restoreSelectionState();
    
private:
    QList<TransformObject*> m_objects;
    QList<QList<std::shared_ptr<TransformOperation>>> m_selectionHistory;
};

#endif // TRANSFORM_SYSTEM_H