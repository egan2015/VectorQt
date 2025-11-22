#include "drawing-transform.h"
#include <QDebug>
#include <QtMath>

// DrawingTransform
DrawingTransform::DrawingTransform()
    : m_transform(QTransform())
    , m_sceneAnchor(QPointF(0, 0))
    , m_hasAnchor(false)
    , m_transformType(TransformType::Undefined)
{
}

void DrawingTransform::translate(const QPointF &delta)
{
    QTransform newTransform = m_transform;
    newTransform.translate(delta.x(), delta.y());
    m_transform = newTransform;
}

void DrawingTransform::rotate(double angle, const QPointF &center)
{
    QTransform newTransform = m_transform;
    newTransform.translate(center.x(), center.y());
    newTransform.rotateRadians(angle);  // 直接使用弧度
    newTransform.translate(-center.x(), -center.y());
    m_transform = newTransform;
}

void DrawingTransform::scale(double sx, double sy, const QPointF &center)
{
    QTransform newTransform = m_transform;
    newTransform.translate(center.x(), center.y());
    newTransform.scale(sx, sy);
    newTransform.translate(-center.x(), -center.y());
    m_transform = newTransform;
}

void DrawingTransform::shear(double sh, double sv, const QPointF &center)
{
    QTransform newTransform = m_transform;
    newTransform.translate(center.x(), center.y());
    newTransform.shear(sh, sv);
    newTransform.translate(-center.x(), -center.y());
    m_transform = newTransform;
}

void DrawingTransform::rotateAroundAnchor(double angle, AnchorPoint anchor, const QRectF &bounds)
{
    QPointF center = getAnchorPoint(anchor, bounds);
    rotate(angle, center);
}

void DrawingTransform::scaleAroundAnchor(double sx, double sy, AnchorPoint anchor, const QRectF &bounds)
{
    QPointF center = getAnchorPoint(anchor, bounds);
    scale(sx, sy, center);
}

void DrawingTransform::shearAroundAnchor(double sh, double sv, AnchorPoint anchor, const QRectF &bounds)
{
    QPointF center = getAnchorPoint(anchor, bounds);
    shear(sh, sv, center);
}

void DrawingTransform::beginTransform()
{
    m_savedTransform = m_transform;
}

void DrawingTransform::applyRelativeTransform(const QTransform &relTransform, const QPointF &center)
{
    QTransform finalTransform = QTransform::fromTranslate(-center.x(), -center.y()) * 
                               relTransform * 
                               QTransform::fromTranslate(center.x(), center.y());
    m_transform = m_transform * finalTransform;
}

void DrawingTransform::endTransform()
{
    // 变换结束，可以在这里触发信号或记录历史
}

void DrawingTransform::saveState()
{
    m_savedTransform = m_transform;
}

void DrawingTransform::restoreState()
{
    m_transform = m_savedTransform;
}

QRectF DrawingTransform::transformedBounds(const QRectF &localBounds) const
{
    // 获取矩形的四个角点
    QPointF corners[4] = {
        localBounds.topLeft(),
        localBounds.topRight(),
        localBounds.bottomRight(),
        localBounds.bottomLeft()
    };
    
    // 变换所有角点
    for (int i = 0; i < 4; ++i) {
        corners[i] = m_transform.map(corners[i]);
    }
    
    // 计算变换后的边界框
    double minX = corners[0].x(), maxX = corners[0].x();
    double minY = corners[0].y(), maxY = corners[0].y();
    
    for (int i = 1; i < 4; ++i) {
        minX = qMin(minX, corners[i].x());
        maxX = qMax(maxX, corners[i].x());
        minY = qMin(minY, corners[i].y());
        maxY = qMax(maxY, corners[i].y());
    }
    
    // 确保边界框有效（处理镜像情况）
    double width = maxX - minX;
    double height = maxY - minY;
    if (width < 0) width = -width;
    if (height < 0) height = -height;
    
    return QRectF(minX, minY, width, height);
}

QPointF DrawingTransform::mapToScene(const QPointF &localPos, const QRectF &localBounds) const
{
    // 将本地坐标映射到场景坐标
    return m_transform.map(localPos);
}

QPointF DrawingTransform::mapToLocal(const QPointF &scenePos, const QRectF &localBounds) const
{
    // 将场景坐标映射到本地坐标（逆变换）
    return m_transform.inverted().map(scenePos);
}

double DrawingTransform::rotation() const
{
    // 从变换矩阵提取旋转角度（弧度）
    return qAtan2(m_transform.m21(), m_transform.m11());
}

QPointF DrawingTransform::scale() const
{
    // 从变换矩阵提取缩放因子
    double sx = qSqrt(m_transform.m11() * m_transform.m11() + m_transform.m21() * m_transform.m21());
    double sy = qSqrt(m_transform.m12() * m_transform.m12() + m_transform.m22() * m_transform.m22());
    return QPointF(sx, sy);
}

QPointF DrawingTransform::shear() const
{
    // 从变换矩阵提取倾斜因子
    return QPointF(m_transform.m12(), m_transform.m21());
}

QPointF DrawingTransform::translation() const
{
    // 从变换矩阵提取平移
    return QPointF(m_transform.m31(), m_transform.m32());
}

QPointF DrawingTransform::getAnchorPoint(AnchorPoint anchor, const QRectF &bounds) const
{
    switch (anchor) {
        case TopLeft: return bounds.topLeft();
        case TopCenter: return QPointF(bounds.center().x(), bounds.top());
        case TopRight: return bounds.topRight();
        case CenterLeft: return QPointF(bounds.left(), bounds.center().y());
        case Center: return bounds.center();
        case CenterRight: return QPointF(bounds.right(), bounds.center().y());
        case BottomLeft: return bounds.bottomLeft();
        case BottomCenter: return QPointF(bounds.center().x(), bounds.bottom());
        case BottomRight: return bounds.bottomRight();
        case Custom: return bounds.center(); // 默认使用中心
    }
}

// TransformOperation
TransformOperation::TransformOperation(Type type, const QVariantList &params)
    : m_type(type)
    , m_params(params)
{
}

void TransformOperation::apply(DrawingTransform &transform) const
{
    switch (m_type) {
        case Translate: {
            QPointF delta = m_params[0].toPointF();
            transform.translate(delta);
            break;
        }
        case Rotate: {
            double angle = m_params[0].toDouble();
            QPointF center = m_params[1].toPointF();
            transform.rotate(angle, center);
            break;
        }
        case Scale: {
            double sx = m_params[0].toDouble();
            double sy = m_params[1].toDouble();
            QPointF center = m_params[2].toPointF();
            transform.scale(sx, sy, center);
            break;
        }
        case Shear: {
            double sh = m_params[0].toDouble();
            double sv = m_params[1].toDouble();
            QPointF center = m_params[2].toPointF();
            transform.shear(sh, sv, center);
            break;
        }
    }
}

TransformOperation TransformOperation::inverse() const
{
    switch (m_type) {
        case Translate: {
            QPointF delta = m_params[0].toPointF();
            return TransformOperation(Translate, {QPointF(-delta.x(), -delta.y())});
        }
        case Rotate: {
            double angle = m_params[0].toDouble();
            QPointF center = m_params[1].toPointF();
            return TransformOperation(Rotate, {-angle, center});
        }
        case Scale: {
            double sx = m_params[0].toDouble();
            double sy = m_params[1].toDouble();
            QPointF center = m_params[2].toPointF();
            if (sx != 0 && sy != 0) {
                return TransformOperation(Scale, {1.0/sx, 1.0/sy, center});
            }
            break;
        }
        case Shear: {
            double sh = m_params[0].toDouble();
            double sv = m_params[1].toDouble();
            QPointF center = m_params[2].toPointF();
            return TransformOperation(Shear, {-sh, -sv, center});
        }
    }
    return *this; // 无法创建逆操作时返回自身
}