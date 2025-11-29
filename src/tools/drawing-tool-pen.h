#ifndef DRAWING_TOOL_PEN_H
#define DRAWING_TOOL_PEN_H

#include <QPointF>
#include <QList>
#include <QPainterPath>
#include "../core/toolbase.h"
#include "../core/brush-engine.h"

class DrawingScene;
class DrawingView;
class DrawingShape;
class DrawingPath;
class QGraphicsPathItem;
class QGraphicsEllipseItem;

/**
 * 钢笔工具
 * 用于绘制精确的贝塞尔曲线路径
 */
class DrawingToolPen : public ToolBase
{
    Q_OBJECT

public:
    enum PenMode
    {
        AnchorMode,  // 锚点模式：点击添加直线锚点
        CurveMode,   // 曲线模式：拖动添加曲线锚点
        EditMode,    // 编辑模式：编辑已有路径
        FreeDrawMode // 自由绘制模式：使用画笔引擎
    };

    explicit DrawingToolPen(QObject *parent = nullptr);

    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;

    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool keyPressEvent(QKeyEvent *event) override;

    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::BezierCursor; }

public slots:
    // 响应颜色变化
    void onStrokeColorChanged(const QColor &color);
    void onFillColorChanged(const QColor &color);

    // 笔刷配置
    void setBrushProfile(const QString &profileName);
    void setBrushWidth(qreal width);
    void setPressureSensitivity(qreal sensitivity);
    void togglePressureSupport(bool enabled);

    // 获取当前设置
    qreal strokeWidthForPanel() const { return m_strokeWidth; }
    bool pressureSupportEnabled() const { return m_pressureSupport; }
    int pressureSensitivityForPanel() const { return static_cast<int>(m_pressureSensitivity * 100); }

private:
    // 添加锚点
    void addAnchorPoint(const QPointF &scenePos);

    // 开始曲线锚点
    void beginCurveAnchor(const QPointF &scenePos);

    // 更新曲线锚点
    void updateCurveAnchor(const QPointF &scenePos);

    // 结束曲线锚点
    void endCurveAnchor(const QPointF &scenePos);

    // 自由绘制相关
    void beginFreeDraw(const QPointF &scenePos);
    void updateFreeDraw(const QPointF &scenePos);
    void endFreeDraw();

    // 笔锋效果
    void applyPenTipEffect(QPainterPath &path);
    void createTaperedPath(const QVector<QPointF> &points, QVector<qreal> &widths);

    // 创建路径图形
    void createPathShape();

    // 更新预览路径
    void updatePreviewPath();

    // 清理当前路径
    void clearCurrentPath();

    // 结束当前路径
    void finishPath();

    // 获取当前描边颜色
    QColor getCurrentStrokeColor() const;

    // 获取当前填充颜色
    QColor getCurrentFillColor() const;

    // 绘制锚点
    void drawAnchorPoint(const QPointF &pos, bool isSelected = false);

    // 绘制控制点
    void drawControlPoint(const QPointF &pos);

    // 绘制控制线
    void drawControlLine(const QPointF &anchor, const QPointF &control);

    // 更新锚点显示
    void updateAnchorDisplay();

    // 查找最近的锚点
    QPointF findNearestAnchor(const QPointF &scenePos, qreal threshold = 10.0);

    // 检查是否接近第一个锚点（用于闭合路径）
    bool isNearFirstAnchor(const QPointF &scenePos, qreal threshold = 10.0);

private:
    DrawingScene *m_scene;
    DrawingView *m_view;

    PenMode m_mode;
    bool m_isDrawing;
    bool m_isDragging;

    // 画笔引擎
    BrushEngine *m_brushEngine;
    DrawingPath *m_currentPath;

    // 路径数据
    QList<QPointF> m_anchorPoints;  // 锚点列表
    QList<QPointF> m_controlPoints; // 控制点列表（每个锚点对应两个控制点）
    QPainterPath m_penPath;         // 钢笔路径（用于锚点模式）
    QPointF m_currentAnchor;        // 当前锚点
    QPointF m_currentControl;       // 当前控制点
    QPointF m_dragStart;            // 拖动起始点

    // 自由绘制数据
    QVector<QPointF> m_freeDrawPoints; // 自由绘制的点
    QVector<qreal> m_pressures;        // 压力值
    QPointF m_lastPoint;               // 上一个点，用于计算距离
    QElapsedTimer m_timer;             // 计时器，用于计算速度

    // 预览项
    QGraphicsPathItem *m_previewPathItem;
    QList<QGraphicsEllipseItem *> m_anchorItems;
    QList<QGraphicsEllipseItem *> m_controlItems;
    QList<QGraphicsLineItem *> m_controlLineItems;

    // 样式设置
    QColor m_currentStrokeColor;
    QColor m_currentFillColor;
    qreal m_strokeWidth;

    // 设置
    bool m_autoClose;            // 自动闭合路径
    bool m_showControlPoints;    // 显示控制点
    bool m_pressureSupport;      // 是否支持压感
    qreal m_pressureSensitivity; // 压感灵敏度
};

#endif // DRAWING_TOOL_PEN_H