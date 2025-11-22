#ifndef DRAWING_TOOL_LINE_H
#define DRAWING_TOOL_LINE_H

#include "../core/toolbase.h"
#include "../core/drawing-shape.h"

class DrawingToolLine : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingToolLine(QObject *parent = nullptr);
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::LineCursor; }

private:
    DrawingLine *m_currentLine;
    QPointF m_startPoint;
    bool m_drawing;
    
    // 辅助方法
    DrawingShape* createShape(const QPointF &pos);
    void updateShape(const QPointF &startPos, const QPointF &currentPos);
};

#endif // DRAWING_TOOL_LINE_H