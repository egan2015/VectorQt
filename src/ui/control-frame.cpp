#include "../ui/control-frame.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QDebug>

ControlFrame::ControlFrame(QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_handle(None)
    , m_mouseDown(false)
{
    setZValue(1e9);
}

void ControlFrame::sync(const QList<QGraphicsItem *> &items)
{
    m_items = items;
    for (auto *item : items) {
        if (!m_initialTransforms.contains(item)) {
            m_initialTransforms[item] = item->transform();
        }
    }
    updateShapes();
    prepareGeometryChange();
    update();
}

QRectF ControlFrame::boundingRect() const
{
    if (m_items.isEmpty()) {
        return {};
    }
    
    QRectF r;
    for (auto *item : m_items) {
        r |= item->sceneBoundingRect();
    }
    r = r.translated(-pos());
    return r.adjusted(-10, -30, 10, 10);
}

void ControlFrame::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (m_items.isEmpty()) {
        return;
    }
    
    QRectF r;
    for (auto *item : m_items) {
        r |= item->sceneBoundingRect();
    }
    r = r.translated(-pos());

    // Draw dashed rectangle
    painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
    painter->drawRect(r);

    // Draw 8 square handles
    painter->setBrush(Qt::blue);
    for (int i = 0; i < 8; ++i) {
        painter->drawRect(m_shapes[i]);
    }

    // Draw rotation handle (circle)
    painter->drawEllipse(m_shapes[8]);
}

void ControlFrame::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    // Snapshot 8 handles
    m_handles.resize(9);
    QRectF r;
    for (auto *item : m_items) {
        r |= item->sceneBoundingRect();
    }
    
    m_handles[1] = r.topLeft();
    m_handles[2] = QPointF(r.center().x(), r.top());
    m_handles[3] = r.topRight();
    m_handles[4] = QPointF(r.left(), r.center().y());
    m_handles[5] = QPointF(r.right(), r.center().y());
    m_handles[6] = r.bottomLeft();
    m_handles[7] = QPointF(r.center().x(), r.bottom());
    m_handles[8] = r.bottomRight();

    m_handle = handleAt(e->scenePos());

    switch (m_handle) {
    case Rotate:
        setCursor(Qt::SizeAllCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    default:
        unsetCursor();
        break;
    }

    static const int opposite[9] = {0, 8, 7, 6, 5, 4, 3, 2, 1};
    int self = static_cast<int>(m_handle);
    int opp = opposite[self];
    m_lockAnchor = m_handles[opp];

    if (m_handle == None) {
        QGraphicsObject::mousePressEvent(e);
        return;
    }
    
    m_startScene = e->scenePos();
    m_initialAABB = r;
    m_lockCenter = m_initialAABB.center();
    
    for (auto *item : m_items) {
        QPointF axisLocal = item->mapFromScene(m_lockCenter);
        QTransform t0;
        t0.translate(axisLocal.x(), axisLocal.y());
        m_T0[item] = t0;
        m_initialTransforms[item] = item->transform();
    }

    m_mouseDown = true;
}

void ControlFrame::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (m_mouseDown && !m_items.isEmpty()) {
        updateShapes();
    }
    
    if (m_handle == None) {
        return;
    }

    QPointF cur = e->scenePos();
    if (m_handle == Rotate) {
        QPointF cScene = m_lockCenter;
        QLineF l1(cScene, m_startScene), l2(cScene, cur);
        qreal delta = l1.angleTo(l2);

        for (auto *item : m_items) {
            QTransform R;
            R.translate(m_T0[item].dx(), m_T0[item].dy());
            R.rotate(-delta);
            R.translate(-m_T0[item].dx(), -m_T0[item].dy());
            item->setTransform(R * m_initialTransforms[item]);
            item->update();
        }
        updateShapes();
        return;
    }
    
    // Scaling
    static const int opposite[9] = {0, 8, 7, 6, 5, 4, 3, 2, 1};
    int self = static_cast<int>(m_handle);
    int opp = opposite[self];
    if (opp < 1 || opp > 8) {
        return;
    }
    
    QPointF anchorScene = m_lockAnchor;
    QPointF initVec = m_handles[self] - anchorScene;
    QPointF realVec = cur - anchorScene;
    
    if (qFuzzyIsNull(initVec.x()) || qFuzzyIsNull(initVec.y())) {
        return;
    }
    
    qreal sx = realVec.x() / initVec.x();
    qreal sy = realVec.y() / initVec.y();

    const qreal minScaleAbs = 0.01;
    const qreal maxScaleAbs = 100.0;

    sx = qBound(-maxScaleAbs, sx, maxScaleAbs);
    if (qAbs(sx) < minScaleAbs) {
        sx = sx > 0 ? minScaleAbs : -minScaleAbs;
    }

    sy = qBound(-maxScaleAbs, sy, maxScaleAbs);
    if (qAbs(sy) < minScaleAbs) {
        sy = sy > 0 ? minScaleAbs : -minScaleAbs;
    }
    
    for (auto *item : m_items) {
        QPointF anchorLocal = item->mapFromScene(anchorScene);
        QTransform t;
        t.translate(anchorLocal.x(), anchorLocal.y());
        t.scale(sx, sy);
        t.translate(-anchorLocal.x(), -anchorLocal.y());
        item->setTransform(t * m_initialTransforms[item]);
        item->update();
    }
    
    if (m_mouseDown) {
        updateShapes();
    }
}

void ControlFrame::mouseReleaseEvent(QGraphicsSceneMouseEvent *)
{
    unsetCursor();
    m_mouseDown = false;
}

void ControlFrame::updateShapes()
{
    m_shapes.clear();
    QRectF r;
    for (auto *item : m_items) {
        r |= item->sceneBoundingRect();
    }
    r = r.translated(-pos());
    
    const QList<QPointF> centers = {
        r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight(),
        QPointF(r.center().x(), r.top()),
        QPointF(r.center().x(), r.bottom()),
        QPointF(r.left(), r.center().y()),
        QPointF(r.right(), r.center().y())
    };
    
    for (const QPointF &c : centers) {
        m_shapes.append(QRectF(c - QPointF(6, 6), QSize(8, 8)));
    }

    m_shapes.append(QRectF(QPointF(r.center().x() - 6, r.top() - 27), QSize(8, 8)));
}

ControlFrame::Handle ControlFrame::handleAt(const QPointF &sp) const
{
    QPointF p = mapFromScene(sp);
    for (int i = 0; i < m_shapes.size(); ++i) {
        if (i < 8) {
            if (m_shapes[i].contains(p)) {
                return static_cast<Handle>(i + 1);
            }
        } else {
            if (m_shapes[i].contains(p)) {
                return Rotate;
            }
        }
    }
    return None;
}

QPointF ControlFrame::anchorForHandle(Handle h) const
{
    QRectF r = m_initialAABB;
    switch (h) {
    case TopLeft:
        return r.bottomRight();
    case Top:
        return r.bottomLeft() + QPointF(r.width(), 0);
    case TopRight:
        return r.bottomLeft();
    case Left:
        return r.topRight() + QPointF(0, r.height());
    case Right:
        return r.topLeft();
    case BottomLeft:
        return r.topRight();
    case Bottom:
        return r.topLeft() + QPointF(0, r.height());
    case BottomRight:
        return r.topLeft();
    default:
        return {};
    }
}

