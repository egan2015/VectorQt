#include <QGraphicsScene>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include "drawing-tool-eraser.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-shape.h"

DrawingToolEraser::DrawingToolEraser(QObject *parent)
    : ToolBase(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_mode(WholeErase)
    , m_eraserSize(20.0)
    , m_isErasing(false)
    , m_previewItem(nullptr)
{
}

void DrawingToolEraser::activate(DrawingScene *scene, DrawingView *view)
{
    m_scene = scene;
    m_view = view;
    
    // 设置自定义光标
    if (m_view) {
        m_view->setCursor(Qt::BlankCursor); // 隐藏系统光标，使用自定义预览
    }
}

void DrawingToolEraser::deactivate()
{
    hideEraserCursor();
    
    // 恢复默认光标
    if (m_view) {
        m_view->setCursor(Qt::ArrowCursor);
    }
    
    m_scene = nullptr;
    m_view = nullptr;
}

bool DrawingToolEraser::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || event->button() != Qt::LeftButton) return false;
    
    m_isErasing = true;
    
    try {
        // 查找擦除区域内的图形
        QList<DrawingShape*> shapesToErase = findShapesInArea(scenePos, m_eraserSize);
        qDebug() << "Found" << shapesToErase.size() << "shapes to erase";
        
        // 执行擦除
        for (DrawingShape *shape : shapesToErase) {
            if (shape && m_mode == WholeErase) {
                eraseShape(shape);
            } else if (shape) {
                partialEraseShape(shape, scenePos, m_eraserSize);
            }
        }
    } catch (...) {
        // qDebug() << "Error during mouse press erasing";
        m_isErasing = false;
    }
    
    return true;
}

bool DrawingToolEraser::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene) return false;
    
    // 更新橡皮擦预览
    showEraserCursor(scenePos);
    
    if (m_isErasing) {
        // 查找擦除区域内的图形
        QList<DrawingShape*> shapesToErase = findShapesInArea(scenePos, m_eraserSize);
        
        // 执行擦除
        for (DrawingShape *shape : shapesToErase) {
            if (m_mode == WholeErase) {
                eraseShape(shape);
            } else {
                partialEraseShape(shape, scenePos, m_eraserSize);
            }
        }
    }
    
    return true;
}

bool DrawingToolEraser::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton) {
        m_isErasing = false;
        return true;
    }
    
    return false;
}

bool DrawingToolEraser::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) {
        // 释放Shift键恢复到整体擦除模式
        setEraserMode(WholeErase);
        return true;
    }
    
    return false;
}

bool DrawingToolEraser::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) {
        // 按住Shift键切换到局部擦除模式
        setEraserMode(PartialErase);
        return true;
    }
    
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        // 增加橡皮擦大小
        setEraserSize(m_eraserSize + 5);
        return true;
    }
    
    if (event->key() == Qt::Key_Minus) {
        // 减小橡皮擦大小
        setEraserSize(m_eraserSize - 5);
        return true;
    }
    
    return false;
}

void DrawingToolEraser::setEraserMode(EraserMode mode)
{
    m_mode = mode;
}

void DrawingToolEraser::setEraserSize(qreal size)
{
    m_eraserSize = qBound(5.0, size, 100.0); // 限制在5-100像素之间
}

QList<DrawingShape*> DrawingToolEraser::findShapesInArea(const QPointF &center, qreal radius)
{
    QList<DrawingShape*> shapesInArea;
    
    if (!m_scene) return shapesInArea;
    
    // 创建擦除区域
    QRectF eraseArea(center.x() - radius, center.y() - radius, radius * 2, radius * 2);
    
    // 获取区域内的所有图形项
    QList<QGraphicsItem*> items = m_scene->items(eraseArea);
    
    for (QGraphicsItem *item : items) {
        // 检查项目是否有效
        if (!item) continue;
        
        // 使用类型检查而不是qgraphicsitem_cast
        if (item->type() == QGraphicsItem::UserType + 2) { // DrawingShape类型
            DrawingShape *shape = static_cast<DrawingShape*>(item);
            // 检查图形是否真的在擦除范围内
            if (isInRange(shape->sceneBoundingRect().center(), center, radius) ||
                shape->sceneBoundingRect().intersects(eraseArea)) {
                shapesInArea.append(shape);
            }
        }
    }
    
    return shapesInArea;
}

void DrawingToolEraser::eraseShape(DrawingShape *shape)
{
    if (!shape || !m_scene) return;
    
    try {
        // 记录擦除历史
        m_erasedShapes.append(shape);
        
        // 从场景中移除图形
        m_scene->removeItem(shape);
        
        // 安全删除图形
        shape->setVisible(false); // 先隐藏
        delete shape;
        
        // 标记场景已修改
        m_scene->setModified(true);
    } catch (...) {
        // 如果出错，至少保证不会崩溃
        qDebug() << "Error erasing shape";
    }
}

void DrawingToolEraser::partialEraseShape(DrawingShape *shape, const QPointF &center, qreal radius)
{
    // TODO: 局部擦除功能需要更复杂的实现
    // 暂时使用整体擦除
    eraseShape(shape);
}

void DrawingToolEraser::updateEraserPreview(const QPointF &scenePos)
{
    hideEraserCursor();
    showEraserCursor(scenePos);
}

void DrawingToolEraser::showEraserCursor(const QPointF &scenePos)
{
    if (!m_scene) return;
    
    // 如果预览项已存在，先隐藏
    if (m_previewItem) {
        m_previewItem->setVisible(false);
    }
    
    // 创建橡皮擦预览圆圈
    m_previewItem = m_scene->addEllipse(-m_eraserSize/2, -m_eraserSize/2, m_eraserSize, m_eraserSize);
    m_previewItem->setPos(scenePos);
    
    // 根据模式设置不同的颜色
    if (m_mode == PartialErase) {
        m_previewItem->setPen(QPen(Qt::red, 2, Qt::DashLine));
        m_previewItem->setBrush(QBrush(Qt::red, Qt::Dense4Pattern));
    } else {
        m_previewItem->setPen(QPen(Qt::black, 1, Qt::DashLine));
        m_previewItem->setBrush(QBrush(Qt::white, Qt::Dense4Pattern));
    }
    
    m_previewItem->setZValue(1000); // 确保在最顶层
}

void DrawingToolEraser::hideEraserCursor()
{
    if (m_previewItem && m_scene) {
        m_scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
}

bool DrawingToolEraser::isInRange(const QPointF &point, const QPointF &center, qreal radius)
{
    qreal distance = QLineF(point, center).length();
    return distance <= radius;
}