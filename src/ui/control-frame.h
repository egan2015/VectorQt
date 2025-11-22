#ifndef CONTROL_FRAME_H
#define CONTROL_FRAME_H

#include <QGraphicsObject>
#include <QList>
#include <QRectF>
#include <QPointF>

class QGraphicsItem;

class ControlFrame : public QGraphicsObject
{
    Q_OBJECT

public:
    enum Handle
    {
        None = 0,
        TopLeft,
        Top,
        TopRight,
        Left,
        Right,
        BottomLeft,
        Bottom,
        BottomRight,
        Rotate
    };

    explicit ControlFrame(QGraphicsItem *parent = nullptr);
    
    void sync(const QList<QGraphicsItem *> &items);
    
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void updateShapes();
    Handle handleAt(const QPointF &scenePos) const;
    QPointF anchorForHandle(Handle handle) const;
    
    QList<QGraphicsItem *> m_items;
    QHash<QGraphicsItem *, QTransform> m_initialTransforms;
    Handle m_handle;
    QPointF m_startScene;
    QRectF m_initialAABB;
    QPointF m_lockAnchor;
    QVector<QPointF> m_handles;
    QPointF m_lockCenter;
    QHash<QGraphicsItem *, QTransform> m_T0;
    QVector<QRectF> m_shapes;
    bool m_mouseDown;
};

#endif // CONTROL_FRAME_H