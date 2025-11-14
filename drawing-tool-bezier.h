#ifndef DRAWING_TOOL_BEZIER_H
#define DRAWING_TOOL_BEZIER_H

#include "toolbase.h"
#include <QVector>
#include <QPointF>

class DrawingScene;
class QPainterPath;
class QGraphicsSceneMouseEvent;
class QStyleOptionGraphicsItem;
class QWidget;
class QPainter;
class DrawingView;
class DrawingPath;

/**
 * 贝塞尔曲线绘制工具
 * 支持通过点击添加控制点来创建贝塞尔曲线
 */
class DrawingBezierTool : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingBezierTool(QObject *parent = nullptr);
    ~DrawingBezierTool() override;

    // 事件处理 - 重写基类方法
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::BezierCursor; }
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 激活/停用
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;

private:
    // 更新当前路径
    void updatePath();
    
    // 完成绘制
    void finishDrawing();
    
    // 预览项
    DrawingPath *m_previewItem;
    bool m_isDrawing;
    
    QPainterPath *m_currentPath;
    QVector<QPointF> m_controlPoints;
    
    // 当前创建的图形项
    DrawingPath *m_currentItem;
};

#endif // DRAWING_TOOL_BEZIER_H