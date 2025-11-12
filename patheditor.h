#ifndef PATH_EDITOR_H
#define PATH_EDITOR_H

#include <QObject>
#include <QPainterPath>
#include <QList>
#include <QPointF>

class DrawingPath;

/**
 * 路径编辑器 - 处理复杂路径操作
 */
class PathEditor : public QObject
{
    Q_OBJECT

public:
    enum BooleanOperation {
        Union,          // 合并
        Intersection,   // 相交
        Subtraction,    // 减去
        Xor             // 异或
    };
    
    enum PathOperation {
        Simplify,       // 简化路径
        Smooth,         // 平滑路径
        ConvertToCurve, // 转换为曲线
        Offset,         // 偏移路径
        Outline         // 描边路径
    };

public:
    explicit PathEditor(QObject *parent = nullptr);
    
    // 布尔运算
    static QPainterPath booleanOperation(const QPainterPath &path1, 
                                        const QPainterPath &path2, 
                                        BooleanOperation op);
    
    // 路径操作
    static QPainterPath simplifyPath(const QPainterPath &path, qreal tolerance = 0.5);
    static QPainterPath smoothPath(const QPainterPath &path, qreal smoothness = 0.5);
    static QPainterPath convertToCurve(const QPainterPath &path);
    static QPainterPath offsetPath(const QPainterPath &path, qreal distance);
    static QPainterPath outlinePath(const QPainterPath &path, qreal width);
    
    // 路径分析
    static bool pathsIntersect(const QPainterPath &path1, const QPainterPath &path2);
    static QPainterPath getIntersection(const QPainterPath &path1, const QPainterPath &path2);
    static QList<QPointF> getIntersectionPoints(const QPainterPath &path1, const QPainterPath &path2);
    
    // 路径转换
    static QPainterPath fromPolygon(const QList<QPointF> &points, bool closed = false);
    static QList<QPointF> toPolygon(const QPainterPath &path, qreal flatness = 0.5);
    
    // 路径裁剪
    static QPainterPath clipPath(const QPainterPath &path, const QRectF &clipRect);
    static QPainterPath clipPath(const QPainterPath &path, const QPainterPath &clipPath);
    
    // 高级功能
    static QPainterPath createArrow(const QPointF &start, const QPointF &end, qreal headLength = 10);
    static QPainterPath createStar(const QPointF &center, qreal radius, int points = 5);
    static QPainterPath createGear(const QPointF &center, qreal radius, int teeth = 8);

signals:
    void operationCompleted();

private:
    // 辅助函数
    static bool pointInPolygon(const QPointF &point, const QList<QPointF> &polygon);
    static QList<QPointF> douglasPeucker(const QList<QPointF> &points, qreal epsilon);
    static QPointF bezierPoint(const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, qreal t);
};

#endif // PATH_EDITOR_H