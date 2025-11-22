#ifndef DRAWING_TOOL_H
#define DRAWING_TOOL_H

#include <QObject>
#include <QMouseEvent>
#include <QPointF>
#include <QGraphicsItem>

class DrawingScene;
class DrawingView;

class DrawingTool : public QObject
{
    Q_OBJECT

public:
    explicit DrawingTool(QObject *parent = nullptr);
    virtual ~DrawingTool() = default;
    
    virtual void activate(DrawingScene *scene, DrawingView *view);
    virtual void deactivate();
    
    virtual bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos);
    
    DrawingScene* scene() const { return m_scene; }
    DrawingView* view() const { return m_view; }

protected:
    DrawingScene *m_scene;
    DrawingView *m_view;
};

#endif // DRAWING_TOOL_H