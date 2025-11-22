#ifndef DRAWING_TOOL_POLYLINE_H
#define DRAWING_TOOL_POLYLINE_H

#include "../core/toolbase.h"
#include "../core/drawing-shape.h"

class DrawingToolPolyline : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingToolPolyline(QObject *parent = nullptr);
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::PolylineCursor; }
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos) override;

private:
    DrawingPolyline *m_currentPolyline;
    bool m_drawing;
};

#endif // DRAWING_TOOL_POLYLINE_H