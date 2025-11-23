#include <QMouseEvent>
#include "../tools/drawing-tool.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"

DrawingTool::DrawingTool(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
{
}

void DrawingTool::activate(DrawingScene *scene, DrawingView *view)
{
    m_scene = scene;
    m_view = view;
}

void DrawingTool::deactivate()
{
    m_scene = nullptr;
    m_view = nullptr;
}

bool DrawingTool::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool DrawingTool::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool DrawingTool::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

