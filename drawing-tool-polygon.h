#ifndef DRAWING_TOOL_POLYGON_H
#define DRAWING_TOOL_POLYGON_H

#include "toolbase.h"
#include "drawing-shape.h"

class DrawingToolPolygon : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingToolPolygon(QObject *parent = nullptr);
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos) override;

private:
    DrawingPolygon *m_currentPolygon;
    bool m_drawing;
};

#endif // DRAWING_TOOL_POLYGON_H