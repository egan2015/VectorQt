#ifndef TOOLBASE_H
#define TOOLBASE_H

#include <QObject>
#include <QPointF>
#include <QGraphicsItem>

// Qt命名空间中的前置声明
QT_BEGIN_NAMESPACE
class QMouseEvent;
QT_END_NAMESPACE

class DrawingScene;
class DrawingView;
class DrawingRectangle;
class DrawingEllipse;
class DrawingShape;

class ToolBase : public QObject
{
    Q_OBJECT

public:
    explicit ToolBase(QObject *parent = nullptr);
    virtual ~ToolBase() = default;
    
    virtual void activate(DrawingScene *scene, DrawingView *view);
    virtual void deactivate();
    
    virtual bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos);
    
    DrawingScene* scene() const { return m_scene; }
    DrawingView* view() const { return m_view; }

signals:
    // 形状创建完成信号
    void shapeFinished(DrawingShape *shape);

protected:
    DrawingScene *m_scene;
    DrawingView *m_view;
};

class LegacySelectTool : public ToolBase
{
    Q_OBJECT

public:
    explicit LegacySelectTool(QObject *parent = nullptr);
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos) override;

private:
    bool m_dragging;
    QPointF m_startPos;
};

class LegacyRectangleTool : public ToolBase
{
    Q_OBJECT

public:
    explicit LegacyRectangleTool(QObject *parent = nullptr);
    virtual ~LegacyRectangleTool();
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;

private:
    bool m_drawing;
    QPointF m_startPos;
    DrawingRectangle *m_previewItem;
    DrawingRectangle *m_currentItem;  // 当前正在创建的矩形
};

class LegacyEllipseTool : public ToolBase
{
    Q_OBJECT

public:
    explicit LegacyEllipseTool(QObject *parent = nullptr);
    virtual ~LegacyEllipseTool();
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;

private:
    bool m_drawing;
    QPointF m_startPos;
    DrawingEllipse *m_previewItem;
    DrawingEllipse *m_currentItem;  // 当前正在创建的椭圆
};

#endif // TOOLBASE_H
