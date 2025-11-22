#include "../tools/drawing-tool-fill.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-shape.h"
#include "../ui/mainwindow.h"
#include "../ui/colorpalette.h"
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QBrush>

DrawingToolFill::DrawingToolFill(QObject *parent)
    : ToolBase(parent)
    , m_currentFillColor(Qt::blue)
    , m_tolerance(32)  // 默认容差值
{
}

void DrawingToolFill::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    
    // 获取当前填充颜色
    m_currentFillColor = getCurrentFillColor();
}

void DrawingToolFill::deactivate()
{
    ToolBase::deactivate();
}

bool DrawingToolFill::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        // 每次点击时重新获取当前颜色
        m_currentFillColor = getCurrentFillColor();
        
        // 查找点击位置的封闭图形
        DrawingShape *shape = findEnclosedShape(scenePos);
        if (shape) {
            // 使用当前填充颜色（填充工具现在只处理纯色填充）
            shape->setFillBrush(QBrush(m_currentFillColor));
            m_scene->setModified(true);
            return true;
        }
    }
    
    return false;
}

bool DrawingToolFill::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool DrawingToolFill::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

DrawingShape* DrawingToolFill::findEnclosedShape(const QPointF &scenePos)
{
    if (!m_scene) {
        return nullptr;
    }
    
    // 获取点击位置的所有图形
    QList<QGraphicsItem*> items = m_scene->items(scenePos);
    
    // qDebug() << "Fill tool: clicked at" << scenePos << "found" << items.size() << "items";
    
    // 从上到下查找第一个可填充的图形
    for (QGraphicsItem *item : items) {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            // 检查图形类型是否支持填充
            DrawingShape::ShapeType type = shape->shapeType();
            // qDebug() << "Fill tool: found shape type" << type;
            
            if (type == DrawingShape::Rectangle || 
                type == DrawingShape::Ellipse || 
                type == DrawingShape::Polygon) {
                // 将场景坐标转换为图形本地坐标
                QPointF localPos = shape->mapFromScene(scenePos);
                // qDebug() << "Fill tool: scenePos=" << scenePos << "localPos=" << localPos;
                
                // 检查点是否在图形内部
                bool contains = shape->shape().contains(localPos);
                // qDebug() << "Fill tool: shape contains point?" << contains;
                if (contains) {
                    return shape;
                }
            }
            // 对于路径，也检查是否封闭
            else if (type == DrawingShape::Path) {
                // TODO: 检查路径是否封闭
                if (shape->shape().contains(scenePos)) {
                    return shape;
                }
            }
        }
    }
    
    return nullptr;
}

QColor DrawingToolFill::getCurrentFillColor() const
{
    // 尝试从主窗口获取颜色面板
    if (m_scene && m_scene->views().size() > 0) {
        QGraphicsView* view = m_scene->views().first();
        // 获取主窗口
        QWidget* mainWindow = view->parentWidget();
        while (mainWindow && !qobject_cast<MainWindow*>(mainWindow)) {
            mainWindow = mainWindow->parentWidget();
        }
        
        if (MainWindow* mainWnd = qobject_cast<MainWindow*>(mainWindow)) {
            return mainWnd->getCurrentFillColor();
        }
    }
    
    return Qt::blue; // 默认颜色
}

void DrawingToolFill::onFillColorChanged(const QColor &color)
{
    m_currentFillColor = color;
}

void DrawingToolFill::setTolerance(int tolerance)
{
    m_tolerance = qBound(0, tolerance, 255);  // 限制在0-255范围内
}

