#include "patheditor.h"
#include "drawing-shape.h"
#include <QDebug>
#include <QVector2D>
#include <qmath.h>

PathEditor::PathEditor(QObject *parent)
    : QObject(parent)
{
}

QPainterPath PathEditor::booleanOperation(const QPainterPath &path1, 
                                         const QPainterPath &path2, 
                                         BooleanOperation op)
{
    QPainterPath result;
    
    // 检查路径是否为空
    if (path1.isEmpty() && path2.isEmpty()) {
        return result;
    }
    if (path1.isEmpty()) {
        return path2;
    }
    if (path2.isEmpty()) {
        return path1;
    }
    
    qDebug() << "Performing boolean operation, op:" << op;
    
    // 创建路径副本并确保使用正确的填充规则
    QPainterPath p1 = path1;
    QPainterPath p2 = path2;
    
    // 对于布尔运算，使用 OddEvenFill 填充规则通常更可靠
    p1.setFillRule(Qt::OddEvenFill);
    p2.setFillRule(Qt::OddEvenFill);
    
    // 执行布尔运算
    switch (op) {
    case Union:
        qDebug() << "Performing union operation";
        result = p1.united(p2);
        break;
    case Intersection:
        qDebug() << "Performing intersection operation";
        result = p1.intersected(p2);
        break;
    case Subtraction:
        qDebug() << "Performing subtraction operation";
        result = p1.subtracted(p2);
        break;
    case Xor:
        qDebug() << "Performing XOR operation";
        // XOR 可以通过 (A U B) - (A ∩ B) 实现
        result = p1.united(p2).subtracted(p1.intersected(p2));
        break;
    }
    
    // 确保结果也使用正确的填充规则
    result.setFillRule(Qt::OddEvenFill);
    
    qDebug() << "Boolean operation result isEmpty:" << result.isEmpty();
    qDebug() << "Result elementCount:" << result.elementCount();
    
    return result;
}

QPainterPath PathEditor::simplifyPath(const QPainterPath &path, qreal tolerance)
{
    if (path.elementCount() < 3) {
        return path;
    }
    
    // 将路径转换为点列表
    QList<QPointF> points;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element &elem = path.elementAt(i);
        if (elem.type == QPainterPath::MoveToElement || 
            elem.type == QPainterPath::LineToElement) {
            points.append(QPointF(elem.x, elem.y));
        }
    }
    
    // 使用Douglas-Peucker算法简化
    QList<QPointF> simplified = douglasPeucker(points, tolerance);
    
    // 重建路径
    QPainterPath result;
    if (simplified.size() > 0) {
        result.moveTo(simplified[0]);
        for (int i = 1; i < simplified.size(); ++i) {
            result.lineTo(simplified[i]);
        }
        if (path.fillRule() == Qt::WindingFill) {
            result.closeSubpath();
        }
    }
    
    return result;
}

QPainterPath PathEditor::smoothPath(const QPainterPath &path, qreal smoothness)
{
    if (path.elementCount() < 3) {
        return path;
    }
    
    QPainterPath result;
    QList<QPointF> points;
    
    // 提取路径点
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element &elem = path.elementAt(i);
        if (elem.type == QPainterPath::MoveToElement || 
            elem.type == QPainterPath::LineToElement) {
            points.append(QPointF(elem.x, elem.y));
        }
    }
    
    if (points.size() < 3) {
        return path;
    }
    
    // 创建平滑路径
    result.moveTo(points[0]);
    
    for (int i = 1; i < points.size() - 1; ++i) {
        QPointF prev = points[i - 1];
        QPointF curr = points[i];
        QPointF next = points[i + 1];
        
        // 计算控制点
        QPointF cp1 = curr + (next - prev) * (smoothness * 0.15);
        QPointF cp2 = curr - (next - prev) * (smoothness * 0.15);
        
        result.cubicTo(cp1, cp2, curr);
    }
    
    // 添加最后一个点
    if (points.size() > 1) {
        result.lineTo(points.last());
    }
    
    return result;
}

QPainterPath PathEditor::convertToCurve(const QPainterPath &path)
{
    if (path.elementCount() < 2) {
        return path;
    }
    
    QPainterPath result;
    QList<QPointF> points;
    
    // 提取路径点
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element &elem = path.elementAt(i);
        if (elem.type == QPainterPath::MoveToElement || 
            elem.type == QPainterPath::LineToElement) {
            points.append(QPointF(elem.x, elem.y));
        }
    }
    
    if (points.size() < 2) {
        return path;
    }
    
    result.moveTo(points[0]);
    
    if (points.size() == 2) {
        result.lineTo(points[1]);
    } else {
        // 将直线段转换为曲线
        for (int i = 1; i < points.size(); ++i) {
            QPointF prev = points[i - 1];
            QPointF curr = points[i];
            QPointF next = (i < points.size() - 1) ? points[i + 1] : points[i];
            
            // 计算控制点
            QPointF cp1 = prev + (curr - prev) * 0.67;
            QPointF cp2 = curr - (next - curr) * 0.33;
            
            result.cubicTo(cp1, cp2, curr);
        }
    }
    
    return result;
}

QPainterPath PathEditor::offsetPath(const QPainterPath &path, qreal distance)
{
    QPainterPathStroker stroker;
    stroker.setWidth(distance * 2);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    
    QPainterPath offset = stroker.createStroke(path);
    return offset;
}

QPainterPath PathEditor::outlinePath(const QPainterPath &path, qreal width)
{
    QPainterPathStroker stroker;
    stroker.setWidth(width);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    
    return stroker.createStroke(path);
}

bool PathEditor::pathsIntersect(const QPainterPath &path1, const QPainterPath &path2)
{
    return !path1.intersected(path2).isEmpty();
}

QPainterPath PathEditor::getIntersection(const QPainterPath &path1, const QPainterPath &path2)
{
    return path1.intersected(path2);
}

QList<QPointF> PathEditor::getIntersectionPoints(const QPainterPath &path1, const QPainterPath &path2)
{
    QList<QPointF> intersections;
    
    // 简化实现：使用边界框检测
    QRectF bounds1 = path1.controlPointRect();
    QRectF bounds2 = path2.controlPointRect();
    
    if (!bounds1.intersects(bounds2)) {
        return intersections;
    }
    
    // 更精确的检测需要更复杂的算法，这里返回边界框交点作为示例
    QRectF intersection = bounds1.intersected(bounds2);
    if (!intersection.isEmpty()) {
        intersections.append(intersection.center());
    }
    
    return intersections;
}

QPainterPath PathEditor::fromPolygon(const QList<QPointF> &points, bool closed)
{
    QPainterPath path;
    
    if (points.size() > 0) {
        path.moveTo(points[0]);
        for (int i = 1; i < points.size(); ++i) {
            path.lineTo(points[i]);
        }
        if (closed) {
            path.closeSubpath();
        }
    }
    
    return path;
}

QList<QPointF> PathEditor::toPolygon(const QPainterPath &path, qreal flatness)
{
    QList<QPointF> points;
    
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element &elem = path.elementAt(i);
        if (elem.type == QPainterPath::MoveToElement || 
            elem.type == QPainterPath::LineToElement) {
            points.append(QPointF(elem.x, elem.y));
        }
    }
    
    return points;
}

QPainterPath PathEditor::createArrow(const QPointF &start, const QPointF &end, qreal headLength)
{
    QPainterPath arrow;
    
    // 箭头主线
    arrow.moveTo(start);
    arrow.lineTo(end);
    
    // 计算箭头头部
    QVector2D direction(end - start);
    direction.normalize();
    QVector2D perpendicular(-direction.y(), direction.x());
    
    QPointF p1 = end - direction.toPointF() * headLength + perpendicular.toPointF() * headLength * 0.5;
    QPointF p2 = end - direction.toPointF() * headLength - perpendicular.toPointF() * headLength * 0.5;
    
    // 箭头头部
    arrow.moveTo(p1);
    arrow.lineTo(end);
    arrow.lineTo(p2);
    
    return arrow;
}

QPainterPath PathEditor::createStar(const QPointF &center, qreal radius, int points)
{
    QPainterPath star;
    
    qreal angleStep = M_PI * 2 / (points * 2);
    qreal innerRadius = radius * 0.4;
    
    for (int i = 0; i < points * 2; ++i) {
        qreal angle = i * angleStep - M_PI / 2;
        qreal r = (i % 2 == 0) ? radius : innerRadius;
        QPointF point = center + QPointF(qCos(angle) * r, qSin(angle) * r);
        
        if (i == 0) {
            star.moveTo(point);
        } else {
            star.lineTo(point);
        }
    }
    
    star.closeSubpath();
    return star;
}

QPainterPath PathEditor::createGear(const QPointF &center, qreal radius, int teeth)
{
    QPainterPath gear;
    
    qreal angleStep = M_PI * 2 / teeth;
    qreal toothWidth = angleStep * 0.4;
    qreal toothHeight = radius * 0.2;
    
    for (int i = 0; i < teeth; ++i) {
        qreal baseAngle = i * angleStep;
        
        // 齿根
        QPointF p1 = center + QPointF(qCos(baseAngle - toothWidth/2) * radius, 
                                      qSin(baseAngle - toothWidth/2) * radius);
        QPointF p2 = center + QPointF(qCos(baseAngle + toothWidth/2) * radius, 
                                      qSin(baseAngle + toothWidth/2) * radius);
        
        // 齿顶
        QPointF p3 = center + QPointF(qCos(baseAngle + toothWidth/2) * (radius + toothHeight), 
                                      qSin(baseAngle + toothWidth/2) * (radius + toothHeight));
        QPointF p4 = center + QPointF(qCos(baseAngle - toothWidth/2) * (radius + toothHeight), 
                                      qSin(baseAngle - toothWidth/2) * (radius + toothHeight));
        
        if (i == 0) {
            gear.moveTo(p1);
            gear.lineTo(p2);
            gear.lineTo(p3);
            gear.lineTo(p4);
        } else {
            gear.lineTo(p1);
            gear.lineTo(p2);
            gear.lineTo(p3);
            gear.lineTo(p4);
        }
    }
    
    gear.closeSubpath();
    return gear;
}

bool PathEditor::pointInPolygon(const QPointF &point, const QList<QPointF> &polygon)
{
    if (polygon.size() < 3) {
        return false;
    }
    
    bool inside = false;
    int j = polygon.size() - 1;
    
    for (int i = 0; i < polygon.size(); ++i) {
        if (((polygon[i].y() > point.y()) != (polygon[j].y() > point.y())) &&
            (point.x() < (polygon[j].x() - polygon[i].x()) * (point.y() - polygon[i].y()) / 
                         (polygon[j].y() - polygon[i].y()) + polygon[i].x())) {
            inside = !inside;
        }
        j = i;
    }
    
    return inside;
}

QList<QPointF> PathEditor::douglasPeucker(const QList<QPointF> &points, qreal epsilon)
{
    if (points.size() <= 2) {
        return points;
    }
    
    // 找到距离直线最远的点
    qreal maxDistance = 0;
    int maxIndex = 0;
    
    for (int i = 1; i < points.size() - 1; ++i) {
        qreal distance = 0;
        // 计算点到直线的距离
        QPointF p1 = points[0];
        QPointF p2 = points.last();
        QPointF p = points[i];
        
        qreal A = p.y() - p1.y();
        qreal B = p1.x() - p.x();
        qreal C = p.x() - p1.x();
        qreal D = p.y() - p1.y();
        qreal dot = A * A + B * B;
        qreal len_sq = C * C + D * D;
        qreal param = -1;
        
        if (len_sq != 0) {
            param = dot / len_sq;
        }
        
        QPointF xx;
        if (param < 0) {
            xx = p1;
        } else if (param > 1) {
            xx = p2;
        } else {
            xx = QPointF(p1.x() + param * C, p1.y() + param * D);
        }
        
        qreal dx = p.x() - xx.x();
        qreal dy = p.y() - xx.y();
        distance = qSqrt(dx * dx + dy * dy);
        
        if (distance > maxDistance) {
            maxDistance = distance;
            maxIndex = i;
        }
    }
    
    // 如果最大距离大于阈值，递归简化
    if (maxDistance > epsilon) {
        QList<QPointF> left = points.mid(0, maxIndex + 1);
        QList<QPointF> right = points.mid(maxIndex);
        
        QList<QPointF> leftResult = douglasPeucker(left, epsilon);
        QList<QPointF> rightResult = douglasPeucker(right, epsilon);
        
        leftResult.removeLast();
        leftResult.append(rightResult);
        
        return leftResult;
    } else {
        return QList<QPointF>() << points.first() << points.last();
    }
}

QPointF PathEditor::bezierPoint(const QPointF &p0, const QPointF &p1, 
                                const QPointF &p2, const QPointF &p3, qreal t)
{
    qreal u = 1 - t;
    qreal tt = t * t;
    qreal uu = u * u;
    qreal ttt = tt * t;
    
    QPointF point = uu * p0 + 3 * uu * t * p1 + 3 * u * tt * p2 + ttt * p3;
    return point;
}

QPainterPath PathEditor::clipPath(const QPainterPath &path, const QRectF &clipRect)
{
    QPainterPath clipPath;
    clipPath.addRect(clipRect);
    return path.intersected(clipPath);
}

QPainterPath PathEditor::clipPath(const QPainterPath &path, const QPainterPath &clipPath)
{
    return path.intersected(clipPath);
}

#include "patheditor.moc"