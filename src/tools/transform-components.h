#ifndef TRANSFORM_COMPONENTS_H
#define TRANSFORM_COMPONENTS_H

#include <QTransform>
#include <QPointF>
#include <cmath>

/**
 * 简洁的变换分量系统 - 参考 Inkscape 设计
 * 每个变换都是独立的轻量级类型，可组合成 QTransform
 */

// 独立的平移操作
struct Translate
{
    QPointF delta;
    
    explicit Translate(const QPointF& d = QPointF(0, 0)) : delta(d) {}
    
    QTransform toTransform() const {
        return QTransform::fromTranslate(delta.x(), delta.y());
    }
    
    // 逆操作
    Translate inverse() const {
        return Translate(-delta);
    }
};

// 独立的旋转操作
struct Rotate
{
    double angle;      // 角度（度）
    QPointF center;    // 旋转中心
    
    explicit Rotate(double a = 0.0, const QPointF& c = QPointF(0, 0)) 
        : angle(a), center(c) {}
    
    QTransform toTransform() const {
        QTransform t;
        t.translate(center.x(), center.y());
        t.rotate(angle);
        t.translate(-center.x(), -center.y());
        return t;
    }
    
    // 逆操作
    Rotate inverse() const {
        return Rotate(-angle, center);
    }
};

// 独立的缩放操作
struct Scale
{
    QPointF factors;   // 缩放因子 (sx, sy)
    QPointF center;    // 缩放中心
    
    explicit Scale(const QPointF& f = QPointF(1.0, 1.0), const QPointF& c = QPointF(0, 0)) 
        : factors(f), center(c) {}
    
    QTransform toTransform() const {
        QTransform t;
        t.translate(center.x(), center.y());
        t.scale(factors.x(), factors.y());
        t.translate(-center.x(), -center.y());
        return t;
    }
    
    // 逆操作
    Scale inverse() const {
        return Scale(QPointF(1.0/factors.x(), 1.0/factors.y()), center);
    }
};

// 独立的斜切操作
struct Shear
{
    QPointF factors;   // 斜切因子 (sh, sv)
    QPointF center;    // 斜切中心
    
    explicit Shear(const QPointF& f = QPointF(0.0, 0.0), const QPointF& c = QPointF(0, 0)) 
        : factors(f), center(c) {}
    
    QTransform toTransform() const {
        QTransform t;
        t.translate(center.x(), center.y());
        t.shear(factors.x(), factors.y());
        t.translate(-center.x(), -center.y());
        return t;
    }
    
    // 逆操作
    Shear inverse() const {
        // 计算逆斜切矩阵
        double det = 1.0 - factors.x() * factors.y();
        if (std::abs(det) < 1e-10) {
            // 奇异矩阵，返回无斜切
            return Shear(QPointF(0, 0), center);
        }
        double inv_sh = -factors.x() / det;
        double inv_sv = -factors.y() / det;
        return Shear(QPointF(inv_sh, inv_sv), center);
    }
};

// 运算符重载 - 支持与 QTransform 的组合
inline QTransform operator*(const QTransform& lhs, const Translate& rhs) {
    return lhs * rhs.toTransform();
}

inline QTransform operator*(const QTransform& lhs, const Rotate& rhs) {
    return lhs * rhs.toTransform();
}

inline QTransform operator*(const QTransform& lhs, const Scale& rhs) {
    return lhs * rhs.toTransform();
}

inline QTransform operator*(const QTransform& lhs, const Shear& rhs) {
    return lhs * rhs.toTransform();
}

// 运算符重载 - 支持操作之间的组合
inline Scale operator*(const Scale& a, const Scale& b) {
    // 缩放因子相乘，使用第二个缩放的中心
    return Scale(QPointF(a.factors.x() * b.factors.x(), 
                        a.factors.y() * b.factors.y()), b.center);
}

// 变换操作之间的组合
inline QTransform operator*(const Translate& lhs, const Rotate& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Translate& lhs, const Scale& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Translate& lhs, const Shear& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Rotate& lhs, const Scale& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Rotate& lhs, const Shear& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Scale& lhs, const Shear& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

// 添加更多组合运算符（逆序）
inline QTransform operator*(const Rotate& lhs, const Translate& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Scale& lhs, const Translate& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Scale& lhs, const Rotate& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Shear& lhs, const Translate& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Shear& lhs, const Rotate& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

inline QTransform operator*(const Shear& lhs, const Scale& rhs) {
    return lhs.toTransform() * rhs.toTransform();
}

// QTransform 的 *= 运算符重载 - 支持直接叠加变换分量
inline QTransform& operator*=(QTransform& lhs, const Translate& rhs) {
    lhs = lhs * rhs.toTransform();
    return lhs;
}

inline QTransform& operator*=(QTransform& lhs, const Rotate& rhs) {
    lhs = lhs * rhs.toTransform();
    return lhs;
}

inline QTransform& operator*=(QTransform& lhs, const Scale& rhs) {
    lhs = lhs * rhs.toTransform();
    return lhs;
}

inline QTransform& operator*=(QTransform& lhs, const Shear& rhs) {
    lhs = lhs * rhs.toTransform();
    return lhs;
}

inline Translate operator+(const Translate& a, const Translate& b) {
    return Translate(a.delta + b.delta);
}

inline Translate operator-(const Translate& a, const Translate& b) {
    return Translate(a.delta - b.delta);
}

// 便利函数 - 创建常用的变换组合
inline QTransform createTransform(const QPointF& translate, 
                                 double rotate, 
                                 const QPointF& scale,
                                 const QPointF& center = QPointF(0, 0)) {
    return QTransform() 
         * Translate(translate)
         * Rotate(rotate, center)
         * Scale(scale, center);
}

#endif // TRANSFORM_COMPONENTS_H