#ifndef DRAWING_SHAPE_H
#define DRAWING_SHAPE_H

#include <QGraphicsItem>
#include <QTransform>
#include <QPen>
#include <QBrush>
#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <QGraphicsSceneMouseEvent>
#include <QFont>
#include <memory>
#include "drawing-transform.h"

class DrawingDocument;
class EditHandleManager;
class SelectionIndicator;

// 添加EditHandle的前向声明以避免循环包含
class EditHandle;

/**
 * 几何形状基类 - 参考Inkscape的SPShape设计
 * 负责管理几何数据、变换和渲染
 */
class DrawingShape : public QGraphicsItem
{
public:
    enum ShapeType {
        Rectangle,
        Ellipse,
        Path,
        Line,
        Polyline,
        Polygon,
        Text,
        Group
    };
    
    
    
    public:
    DrawingShape(ShapeType type, QGraphicsItem *parent = nullptr);
    ~DrawingShape();
    
    // 几何变换接口 - 使用改进的变换系统
    virtual void setTransform(const DrawingTransform &transform);
    DrawingTransform transform() const { return m_transform; }
    
    // 锚点相关的变换方法
    void rotateAroundAnchor(double angle, DrawingTransform::AnchorPoint anchor = DrawingTransform::Center);
    void scaleAroundAnchor(double sx, double sy, DrawingTransform::AnchorPoint anchor = DrawingTransform::Center);
    void shearAroundAnchor(double sh, double sv, DrawingTransform::AnchorPoint anchor = DrawingTransform::Center);
    
    // 获取本地边界框（未变换）
    virtual QRectF localBounds() const = 0;
    
    // QGraphicsItem重写
    int type() const override { 
        if (m_type == Group) {
            return UserType + 1; // DrawingGroup类型
        }
        return UserType + 2; // 其他DrawingShape类型
    }
    
    // 几何属性
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    
    // 获取可用于布尔运算的路径（考虑变换）
    virtual QPainterPath transformedShape() const;
    
    // 样式属性
    void setFillBrush(const QBrush &brush) { m_fillBrush = brush; update(); }
    QBrush fillBrush() const { return m_fillBrush; }
    
    void setStrokePen(const QPen &pen) { m_strokePen = pen; update(); }
    QPen strokePen() const { return m_strokePen; }
    
    // 网格对齐支持
    void setGridAlignmentEnabled(bool enabled) { m_gridAlignmentEnabled = enabled; }
    bool isGridAlignmentEnabled() const { return m_gridAlignmentEnabled; }
    
    // 文档关联
    void setDocument(DrawingDocument *doc) { m_document = doc; }
    DrawingDocument* document() const { return m_document; }
    
    // 形状类型
    ShapeType shapeType() const { return m_type; }
    
    // 编辑把手相关
    void setEditHandlesEnabled(bool enabled);
    bool isEditHandlesEnabled() const { return m_editHandlesEnabled; }
    EditHandleManager* editHandleManager() const { return m_handleManager; }
    
    // 选择边框显示控制
    void setShowSelectionIndicator(bool show) { m_showSelectionIndicator = show; update(); }
    bool showSelectionIndicator() const { return m_showSelectionIndicator; }
    
    // 节点编辑相关 - 为每个图形类提供编辑点接口
    virtual QVector<QPointF> getNodePoints() const { return QVector<QPointF>(); }
    virtual void setNodePoint(int index, const QPointF &pos) { Q_UNUSED(index); Q_UNUSED(pos); }
    virtual void updateFromNodePoints() {}
    virtual int getNodePointCount() const { return 0; }
    // 处理节点拖动时的位置约束
    virtual QPointF constrainNodePoint(int index, const QPointF &pos) const { Q_UNUSED(index); return pos; }
    // 开始拖动节点
    virtual void beginNodeDrag(int index) { Q_UNUSED(index); }
    // 结束拖动节点
    virtual void endNodeDrag(int index) { Q_UNUSED(index); }

// 渲染
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    
    // 子类需要实现的绘制方法（在本地坐标系中）
    virtual void paintShape(QPainter *painter) = 0;
    
    ShapeType m_type;
    DrawingTransform m_transform;  // 改进的变换系统
    QBrush m_fillBrush;
    QPen m_strokePen;
    DrawingDocument *m_document = nullptr;
    
    // 编辑把手系统
    bool m_editHandlesEnabled = false;
    EditHandleManager *m_handleManager = nullptr;
    
    // 选择边框显示控制
    bool m_showSelectionIndicator = true;
    
    // 网格对齐
    bool m_gridAlignmentEnabled = false;
    
    // 避免递归吸附的标志
    bool m_applyingSnap = false;
};

// DrawingRectangle
/**
 * 矩形形状 - 支持affine变换
 */
class DrawingRectangle : public DrawingShape
{
public:
    explicit DrawingRectangle(QGraphicsItem *parent = nullptr);
    explicit DrawingRectangle(const QRectF &rect, QGraphicsItem *parent = nullptr);
    
    QRectF localBounds() const override;
    QPainterPath shape() const override;
    QPainterPath transformedShape() const override;
    
    // 矩形属性
    void setRectangle(const QRectF &rect);
    QRectF rectangle() const { return m_rect; }
    
    // 圆角半径
    void setCornerRadius(qreal radius);
    qreal cornerRadius() const { return m_cornerRadius; }
    
    // 参考../qdraw的比例控制方式
    void setCornerRadiusRatios(qreal ratioX, qreal ratioY) { 
        m_fRatioX = ratioX; 
        m_fRatioY = ratioY; 
        // 更新实际的圆角半径
        m_cornerRadius = qMin(m_rect.width() * m_fRatioX, m_rect.height() * m_fRatioY);
        update(); 
    }
    qreal cornerRadiusRatioX() const { return m_fRatioX; }
    qreal cornerRadiusRatioY() const { return m_fRatioY; }
    
    // 编辑点相关 - 矩形的2个控制点（圆角和尺寸）
    QVector<QPointF> getNodePoints() const override;
    void setNodePoint(int index, const QPointF &pos) override;
    QPointF constrainNodePoint(int index, const QPointF &pos) const override;
    void beginNodeDrag(int index) override;
    void endNodeDrag(int index) override;
    int getNodePointCount() const override { return 2; }

protected:
    void paintShape(QPainter *painter) override;

private:
    QRectF m_rect;
    qreal m_cornerRadius = 0.0;  // 圆角半径
    
    // 参考../qdraw的比例控制方式
    qreal m_fRatioX = 1/10.0;    // 水平圆角比例（相对于宽度）
    qreal m_fRatioY = 1/3.0;     // 垂直圆角比例（相对于高度）
    
    // 用于流畅圆角编辑的临时变量
    qreal m_dragStartRatioX = 0.0;      // 拖动开始时的水平比例
    qreal m_dragStartRatioY = 0.0;      // 拖动开始时的垂直比例
    int m_dragMode = 0;                 // 拖动模式：0=无，1=圆角控制
};

/**
 * 椭圆形状 - 支持affine变换
 */
class DrawingEllipse : public DrawingShape
{
public:
    explicit DrawingEllipse(QGraphicsItem *parent = nullptr);
    explicit DrawingEllipse(const QRectF &rect, QGraphicsItem *parent = nullptr);
    
    QRectF localBounds() const override;
    QPainterPath shape() const override;
    QPainterPath transformedShape() const override;
    
    // 椭圆属性
    void setEllipse(const QRectF &rect);
    QRectF ellipse() const { return m_rect; }
    
    // 椭圆弧度支持
    void setStartAngle(qreal angle) { m_startAngle = angle; update(); }
    qreal startAngle() const { return m_startAngle; }
    void setSpanAngle(qreal angle) { m_spanAngle = angle; update(); }
    qreal spanAngle() const { return m_spanAngle; }
    
    // 编辑点相关 - 椭圆的控制点（尺寸控制点和角度控制点）
    QVector<QPointF> getNodePoints() const override;
    void setNodePoint(int index, const QPointF &pos) override;
    QPointF constrainNodePoint(int index, const QPointF &pos) const override;
    void beginNodeDrag(int index) override;
    void endNodeDrag(int index) override;
    int getNodePointCount() const override { 
        // 基础2个尺寸控制点，如果不是完整椭圆则再加2个角度控制点
        return (m_spanAngle != 360) ? 4 : 2; 
    }

protected:
    void paintShape(QPainter *painter) override;

private:
    QRectF m_rect;
    qreal m_startAngle = 40;    // 开始角度（用于椭圆弧），与../qdraw一致
    qreal m_spanAngle = 400;   // 跨度角度（用于椭圆弧），与../qdraw一致
    
    // 用于流畅角度编辑的临时变量
    qreal m_dragStartAngle = 0;      // 拖动开始时的角度
    qreal m_dragStartSpan = 0;       // 拖动开始时的跨度
    int m_dragMode = 0;              // 拖动模式：0=无，1=起始角度，2=结束角度
};

/**
 * 路径形状 - 支持复杂路径和变换
 */
class DrawingPath : public DrawingShape
{
public:
    explicit DrawingPath(QGraphicsItem *parent = nullptr);
    
    QRectF localBounds() const override;
    void setPath(const QPainterPath &path);
    QPainterPath path() const { return m_path; }
    
    // 重写变换形状方法
    QPainterPath transformedShape() const override;
    
    // 控制点相关
    void setControlPoints(const QVector<QPointF> &points);
    QVector<QPointF> controlPoints() const { return m_controlPoints; }
    void updatePathFromControlPoints();
    
    // 控制点连线显示
    void setShowControlPolygon(bool show);
    bool showControlPolygon() const;
    
    // 编辑点相关 - 路径的控制点
    QVector<QPointF> getNodePoints() const override;
    void setNodePoint(int index, const QPointF &pos) override;
    QPointF constrainNodePoint(int index, const QPointF &pos) const override;
    void beginNodeDrag(int index) override;
    void endNodeDrag(int index) override;
    void updateFromNodePoints() override;
    int getNodePointCount() const override { return m_controlPoints.size(); }

protected:
    void paintShape(QPainter *painter) override;
    
    // 重写鼠标事件处理以支持控制点交互
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    
private:
    // 控制点交互相关
    int findNearestControlPoint(const QPointF &scenePos) const;
    bool isPointNearControlPoint(const QPointF &scenePos, const QPointF &controlPoint, qreal threshold = 10.0) const;
    
    QPainterPath m_path;
    QVector<QPainterPath::Element> m_pathElements; // 原始路径元素，保存曲线信息
    QVector<QPointF> m_controlPoints;  // 控制点，用于编辑
    QVector<QPainterPath::ElementType> m_controlPointTypes; // 控制点类型
    bool m_showControlPolygon = false; // 是否显示控制点连线
    int m_activeControlPoint = -1;     // 当前活动的控制点索引
    QPointF m_dragStartPos;           // 拖动开始位置
};

/**
 * 文本形状 - 支持SVG文本元素
 */
class DrawingText : public DrawingShape
{
public:
    explicit DrawingText(const QString &text = QString(), QGraphicsItem *parent = nullptr);
    
    QRectF localBounds() const override;
    
    // 文本属性
    void setText(const QString &text);
    QString text() const { return m_text; }
    
    void setFont(const QFont &font);
    QFont font() const { return m_font; }
    
    void setPosition(const QPointF &pos);
    QPointF position() const { return m_position; }
    
    // 重写setPos以确保位置变化时文本也跟着移动
    void setPos(const QPointF &pos);
    
    // 编辑点相关 - 文本的控制点（位置和大小）
    QVector<QPointF> getNodePoints() const override;
    void setNodePoint(int index, const QPointF &pos) override;
    QPointF constrainNodePoint(int index, const QPointF &pos) const override;
    void beginNodeDrag(int index) override;
    void endNodeDrag(int index) override;
    int getNodePointCount() const override { return 2; }

protected:
    void paintShape(QPainter *painter) override;
    
    // 重写鼠标事件以支持文本编辑
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString m_text;
    QFont m_font;
    QPointF m_position;
    qreal m_fontSize;  // 字体大小，用于节点编辑
    bool m_editing;    // 是否正在编辑
};

/**
 * 直线形状 - 支持affine变换
 */
class DrawingLine : public DrawingShape
{
public:
    explicit DrawingLine(const QLineF &line = QLineF(0, 0, 100, 100), QGraphicsItem *parent = nullptr);
    
    QRectF localBounds() const override;
    void setLine(const QLineF &line);
    QLineF line() const { return m_line; }
    
    // 线条属性
    void setLineWidth(qreal width) { m_lineWidth = width; update(); }
    qreal lineWidth() const { return m_lineWidth; }
    
    // 编辑点相关 - 直线的两个端点
    QVector<QPointF> getNodePoints() const override;
    void setNodePoint(int index, const QPointF &pos) override;
    QPointF constrainNodePoint(int index, const QPointF &pos) const override;
    void beginNodeDrag(int index) override;
    void endNodeDrag(int index) override;
    int getNodePointCount() const override { return 2; }

protected:
    void paintShape(QPainter *painter) override;
    
private:
    QLineF m_line;
    qreal m_lineWidth = 1.0;
};

/**
 * 折线形状 - 支持多个连接的线段
 */
class DrawingPolyline : public DrawingShape
{
public:
    explicit DrawingPolyline(QGraphicsItem *parent = nullptr);
    
    QRectF localBounds() const override;
    QPainterPath shape() const override;
    
    // 重写变换形状方法
    QPainterPath transformedShape() const override;
    
    // 点操作
    void addPoint(const QPointF &point);
    void insertPoint(int index, const QPointF &point);
    void removePoint(int index);
    void setPoint(int index, const QPointF &point);
    QPointF point(int index) const;
    int pointCount() const { return m_points.size(); }
    void clearPoints() { m_points.clear(); update(); }
    
    // 线条属性
    void setLineWidth(qreal width) { m_lineWidth = width; update(); }
    qreal lineWidth() const { return m_lineWidth; }
    
    // 闭合属性
    void setClosed(bool closed) { 
        qDebug() << "DrawingPolyline::setClosed called with:" << closed << "from" << m_closed;
        m_closed = closed; 
        update(); 
    }
    bool isClosed() const { return m_closed; }
    
    // 编辑点相关 - 折线的所有顶点
    QVector<QPointF> getNodePoints() const override;
    void setNodePoint(int index, const QPointF &pos) override;
    QPointF constrainNodePoint(int index, const QPointF &pos) const override;
    void beginNodeDrag(int index) override;
    void endNodeDrag(int index) override;
    int getNodePointCount() const override { return m_points.size(); }
    void updateFromNodePoints() override;

protected:
    void paintShape(QPainter *painter) override;
    
    // 重写鼠标事件以支持点编辑
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    
private:
    QVector<QPointF> m_points;
    qreal m_lineWidth = 1.0;
    bool m_closed = false;
    int m_activePoint = -1;
    QPointF m_dragStartPos;
};

/**
 * 多边形形状 - 闭合的多边形
 */
class DrawingPolygon : public DrawingShape
{
public:
    explicit DrawingPolygon(QGraphicsItem *parent = nullptr);
    
    QRectF localBounds() const override;
    QPainterPath shape() const override;
    
    // 重写变换形状方法
    QPainterPath transformedShape() const override;
    
    // 点操作
    void addPoint(const QPointF &point);
    void insertPoint(int index, const QPointF &point);
    void removePoint(int index);
    void setPoint(int index, const QPointF &point);
    QPointF point(int index) const;
    int pointCount() const { return m_points.size(); }
    void clearPoints() { m_points.clear(); update(); }
    
    // 填充属性
    void setFillRule(Qt::FillRule rule) { m_fillRule = rule; update(); }
    Qt::FillRule fillRule() const { return m_fillRule; }
    
    // 编辑点相关 - 多边形的所有顶点
    QVector<QPointF> getNodePoints() const override;
    void setNodePoint(int index, const QPointF &pos) override;
    QPointF constrainNodePoint(int index, const QPointF &pos) const override;
    void beginNodeDrag(int index) override;
    void endNodeDrag(int index) override;
    int getNodePointCount() const override { return m_points.size(); }
    void updateFromNodePoints() override;

protected:
    void paintShape(QPainter *painter) override;
    
    // 重写鼠标事件以支持点编辑
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    
private:
    QVector<QPointF> m_points;
    Qt::FillRule m_fillRule = Qt::OddEvenFill;
    int m_activePoint = -1;
    QPointF m_dragStartPos;
};

#endif // DRAWING_SHAPE_H