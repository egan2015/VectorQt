#ifndef DRAWING_TOOL_BRUSH_H
#define DRAWING_TOOL_BRUSH_H

#include <QPointF>
#include <QVector>
#include "../core/toolbase.h"
#include "../core/brush-engine.h"

class DrawingPath;
class QMouseEvent;
class DrawingThrottle;
class BrushEngine;

/**
 * 画笔工具 - 自由绘制
 */
class DrawingToolBrush : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingToolBrush(QObject *parent = nullptr);
    
    // 工具属性
    QString toolName() const override { return "画笔"; }
    QString toolDescription() const override { return "自由绘制工具"; }
    QString toolCategory() const override { return "绘制"; }
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::BrushCursor; }
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 基础设置
    void setBrushWidth(qreal width) { m_brushWidth = width; }
    void setSmoothness(qreal smoothness) { m_smoothness = smoothness; }
    
    // 获取当前状态
    qreal brushWidth() const { return m_brushWidth; }
    qreal smoothness() const { return m_smoothness; }
    int brushOpacity() const { return 100; }  // TODO: 实现不透明度
    int brushSizeForPanel() const { return static_cast<int>(m_brushWidth); }
    int brushSmoothingForPanel() const { return static_cast<int>(m_smoothness * 100); }

private:
    // 平滑路径（保留兼容性）
    QVector<QPointF> smoothPath(const QVector<QPointF> &points);
    
    DrawingPath *m_currentPath;
    DrawingThrottle *m_throttle;  // 事件节流器（暂不使用）
    QVector<QPointF> m_points;
    QPointF m_lastPoint;
    qreal m_brushWidth;
    qreal m_smoothness;
    bool m_drawing;
};

#endif // DRAWING_TOOL_BRUSH_H