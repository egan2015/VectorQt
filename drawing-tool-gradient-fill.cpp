#include "drawing-tool-gradient-fill.h"
#include "drawingscene.h"
#include "drawingview.h"
#include "drawing-shape.h"
#include "mainwindow.h"
#include "colorpalette.h"
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QBrush>
#include <QPen>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <algorithm>

DrawingToolGradientFill::DrawingToolGradientFill(QObject *parent)
    : ToolBase(parent)
    , m_gradientMode(LinearGradient)
    , m_fillMode(ShapeFill)
    , m_isDragging(false)
    , m_previewItem(nullptr)
    , m_targetShape(nullptr)
    , m_currentFillColor(Qt::blue)
{
    // 初始化默认渐变色标
    m_gradientStops.append(QGradientStop(0.0, Qt::blue));
    m_gradientStops.append(QGradientStop(1.0, Qt::white));
}

void DrawingToolGradientFill::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    
    // 获取当前填充颜色
    m_currentFillColor = getCurrentFillColor();
    
    // 更新第一个色标的颜色
    if (!m_gradientStops.isEmpty()) {
        m_gradientStops[0] = QGradientStop(0.0, m_currentFillColor);
    }
}

void DrawingToolGradientFill::deactivate()
{
    // 恢复目标形状的可移动性
    if (m_targetShape) {
        m_targetShape->setFlags(m_originalFlags);
        m_targetShape = nullptr;
    }
    
    // 清理所有预览项
    if (m_scene) {
        QList<QGraphicsItem*> items = m_scene->items();
        for (QGraphicsItem *item : items) {
            // 移除所有预览相关的临时项
            if (item->type() == QGraphicsLineItem::Type || 
                (item->type() == QGraphicsEllipseItem::Type && 
                 (item->zValue() > 1000))) { // 高z值的临时标记
                m_scene->removeItem(item);
                delete item;
            }
        }
    }
    
    ToolBase::deactivate();
}

bool DrawingToolGradientFill::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        // 每次点击时重新获取当前颜色
        m_currentFillColor = getCurrentFillColor();
        
        // 更新第一个色标的颜色
        if (!m_gradientStops.isEmpty()) {
            m_gradientStops[0] = QGradientStop(0.0, m_currentFillColor);
        }
        
        // 查找点击位置的封闭图形
        DrawingShape *shape = findEnclosedShape(scenePos);
        if (shape) {
            // 临时禁用图形的可移动性，防止拖动时触发变换
            m_targetShape = shape;
            m_originalFlags = shape->flags();
            shape->setFlags(shape->flags() & ~QGraphicsItem::ItemIsMovable);
            
            // 保存原始填充用于可能的恢复
            m_originalFillBrush = shape->fillBrush();
            
            m_startPoint = scenePos;
            m_endPoint = scenePos;
            m_isDragging = true;
            
            // 创建预览
            updatePreview(scenePos);
            
            // 阻止事件传播到图形
            return true;
        }
    }
    
    return false;
}

bool DrawingToolGradientFill::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_isDragging && m_scene) {
        m_endPoint = scenePos;
        
        // 更新预览线段
        updatePreview(scenePos);
        
        // 实时预览渐变效果
        if (m_targetShape) {
            // 获取形状的边界框
            QRectF shapeBounds = m_targetShape->boundingRect();
            QPointF shapeCenter = m_targetShape->pos() + shapeBounds.center();
            
            // 计算拖动向量（用于确定渐变方向）
            QPointF dragVector = m_endPoint - m_startPoint;
            qreal dragLength = QLineF(m_startPoint, m_endPoint).length();
            
            // 根据渐变模式创建渐变
            QGradient *gradient = nullptr;
            
            switch (m_gradientMode) {
            case LinearGradient: {
                // 创建基于形状边界的线性渐变
                QLineF dragLine(m_startPoint, m_endPoint);
                QPointF normalizedVector = QPointF(
                    dragLine.dx() / dragLength,
                    dragLine.dy() / dragLength
                );
                QPointF gradientStart = shapeCenter - normalizedVector * shapeBounds.width() * 0.5;
                QPointF gradientEnd = shapeCenter + normalizedVector * shapeBounds.width() * 0.5;
                gradient = createLinearGradient(gradientStart, gradientEnd);
                break;
            }
            case RadialGradient: {
                // 径向渐变的半径基于形状大小，拖动距离只影响强度
                qreal baseRadius = qMax(shapeBounds.width(), shapeBounds.height()) * 0.5;
                qreal radius = baseRadius; // 固定半径，不依赖拖动距离
                gradient = createRadialGradient(shapeCenter, radius);
                break;
            }
            case ConicalGradient: {
                // 锥形渐变的角度基于拖动方向
                qreal angle = QLineF(m_startPoint, m_endPoint).angle();
                gradient = createConicalGradient(shapeCenter, angle);
                break;
            }
            }
            
            if (gradient) {
                // 临时应用渐变用于预览
                QBrush brush(*gradient);
                m_targetShape->setFillBrush(brush);
                delete gradient;
            }
        }
        
        return true;
    }
    
    return false;
}

bool DrawingToolGradientFill::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_isDragging && m_scene) {
        m_isDragging = false;
        
        // 清理所有预览项
        if (m_scene) {
            QList<QGraphicsItem*> items = m_scene->items();
            for (QGraphicsItem *item : items) {
                // 移除所有预览相关的临时项
                if (item->type() == QGraphicsLineItem::Type || 
                    (item->type() == QGraphicsEllipseItem::Type && 
                     (item->zValue() > 1000))) { // 高z值的临时标记
                    m_scene->removeItem(item);
                    delete item;
                }
            }
        }
        
        // 恢复目标形状的可移动性
        if (m_targetShape) {
            m_targetShape->setFlags(m_originalFlags);
            m_targetShape = nullptr;
        }
        
        // 查找目标形状
        DrawingShape *shape = findEnclosedShape(m_startPoint);
        if (shape) {
            // 获取形状的边界框
            QRectF shapeBounds = shape->boundingRect();
            QPointF shapeCenter = shape->pos() + shapeBounds.center();
            
            // 计算拖动向量（用于确定渐变方向）
            QPointF dragVector = m_endPoint - m_startPoint;
            qreal dragLength = QLineF(m_startPoint, m_endPoint).length();
            
            // 根据渐变模式创建渐变
            QGradient *gradient = nullptr;
            
            switch (m_gradientMode) {
            case LinearGradient: {
                // 创建基于形状边界的线性渐变
                QLineF dragLine(m_startPoint, m_endPoint);
                QPointF normalizedVector = QPointF(
                    dragLine.dx() / dragLength,
                    dragLine.dy() / dragLength
                );
                QPointF gradientStart = shapeCenter - normalizedVector * shapeBounds.width() * 0.5;
                QPointF gradientEnd = shapeCenter + normalizedVector * shapeBounds.width() * 0.5;
                gradient = createLinearGradient(gradientStart, gradientEnd);
                break;
            }
            case RadialGradient: {
                // 径向渐变的半径基于形状大小，拖动距离只影响强度
                qreal baseRadius = qMax(shapeBounds.width(), shapeBounds.height()) * 0.5;
                qreal radius = baseRadius; // 固定半径，不依赖拖动距离
                gradient = createRadialGradient(shapeCenter, radius);
                break;
            }
            case ConicalGradient: {
                // 锥形渐变的角度基于拖动方向
                qreal angle = QLineF(m_startPoint, m_endPoint).angle();
                gradient = createConicalGradient(shapeCenter, angle);
                break;
            }
            }
            
            if (gradient) {
                applyGradientToShape(shape, gradient);
                m_scene->setModified(true);
                delete gradient;
            }
        }
        
        return true;
    }
    
    return false;
}

bool DrawingToolGradientFill::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_isDragging && m_targetShape) {
        // 取消当前操作，恢复原始填充
        m_targetShape->setFillBrush(m_originalFillBrush);
        
        // 清理状态
        m_isDragging = false;
        
        // 清理预览项
        if (m_scene) {
            QList<QGraphicsItem*> items = m_scene->items();
            for (QGraphicsItem *item : items) {
                if (item->type() == QGraphicsLineItem::Type || 
                    (item->type() == QGraphicsEllipseItem::Type && 
                     (item->zValue() > 1000))) {
                    m_scene->removeItem(item);
                    delete item;
                }
            }
        }
        
        // 恢复图形可移动性
        if (m_targetShape) {
            m_targetShape->setFlags(m_originalFlags);
            m_targetShape = nullptr;
        }
        
        return true;
    }
    
    return false;
}

void DrawingToolGradientFill::setGradientMode(GradientMode mode)
{
    m_gradientMode = mode;
}

void DrawingToolGradientFill::setFillMode(FillMode mode)
{
    m_fillMode = mode;
}

void DrawingToolGradientFill::addGradientStop(qreal position, const QColor &color)
{
    m_gradientStops.append(QGradientStop(position, color));
    
    // 按位置排序
    std::sort(m_gradientStops.begin(), m_gradientStops.end(),
              [](const QGradientStop &a, const QGradientStop &b) {
                  return a.first < b.first;
              });
}

void DrawingToolGradientFill::clearGradientStops()
{
    m_gradientStops.clear();
}

QGradient* DrawingToolGradientFill::getCurrentGradient() const
{
    QGradient *gradient = nullptr;
    
    switch (m_gradientMode) {
    case LinearGradient:
        gradient = new QLinearGradient(m_startPoint, m_endPoint);
        break;
    case RadialGradient: {
        qreal radius = QLineF(m_startPoint, m_endPoint).length();
        gradient = new QRadialGradient(m_startPoint, radius);
        break;
    }
    case ConicalGradient: {
        qreal angle = QLineF(m_startPoint, m_endPoint).angle();
        gradient = new QConicalGradient(m_startPoint, angle);
        break;
    }
    }
    
    if (gradient) {
        // 设置色标
        for (const QGradientStop &stop : m_gradientStops) {
            gradient->setColorAt(stop.first, stop.second);
        }
    }
    
    return gradient;
}

void DrawingToolGradientFill::onFillColorChanged(const QColor &color)
{
    m_currentFillColor = color;
    
    // 更新第一个色标的颜色
    if (!m_gradientStops.isEmpty()) {
        m_gradientStops[0] = QGradientStop(0.0, color);
    }
}

DrawingShape* DrawingToolGradientFill::findEnclosedShape(const QPointF &scenePos)
{
    if (!m_scene) return nullptr;
    
    // 获取点击位置的所有图形项
    QList<QGraphicsItem *> items = m_scene->items(scenePos);
    
    // 查找第一个封闭图形
    for (QGraphicsItem *item : items) {
        // 直接使用QGraphicsItem，不依赖qgraphicsitem_cast
        // 既然m_scene->items()已经返回了这个位置的项，那它就是我们要找的
        if (item->type() == QGraphicsItem::UserType + 2) { // DrawingShape类型
            return static_cast<DrawingShape*>(item);
        }
    }
    
    return nullptr;
}

QLinearGradient* DrawingToolGradientFill::createLinearGradient(const QPointF &start, const QPointF &end) const
{
    QLinearGradient *gradient = new QLinearGradient(start, end);
    
    // 设置色标
    for (const QGradientStop &stop : m_gradientStops) {
        gradient->setColorAt(stop.first, stop.second);
    }
    
    return gradient;
}

QRadialGradient* DrawingToolGradientFill::createRadialGradient(const QPointF &center, qreal radius) const
{
    QRadialGradient *gradient = new QRadialGradient(center, radius);
    
    // 设置色标
    for (const QGradientStop &stop : m_gradientStops) {
        gradient->setColorAt(stop.first, stop.second);
    }
    
    return gradient;
}

QConicalGradient* DrawingToolGradientFill::createConicalGradient(const QPointF &center, qreal angle) const
{
    QConicalGradient *gradient = new QConicalGradient(center, angle);
    
    // 设置色标
    for (const QGradientStop &stop : m_gradientStops) {
        gradient->setColorAt(stop.first, stop.second);
    }
    
    return gradient;
}

void DrawingToolGradientFill::applyGradientToShape(DrawingShape *shape, QGradient *gradient)
{
    if (!shape || !gradient) return;
    
    QBrush brush(*gradient);
    shape->setFillBrush(brush);
}

QColor DrawingToolGradientFill::getCurrentFillColor() const
{
    // 获取主窗口的颜色面板
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    if (mainWindow) {
        // 通过查找方式获取ColorPalette实例
        ColorPalette *palette = mainWindow->findChild<ColorPalette*>();
        if (palette) {
            return palette->getCurrentFillColor();
        }
    }
    
    return m_currentFillColor;
}

void DrawingToolGradientFill::updatePreview(const QPointF &scenePos)
{
    // 清理所有预览项
    if (m_scene) {
        QList<QGraphicsItem*> items = m_scene->items();
        for (QGraphicsItem *item : items) {
            // 移除所有预览相关的临时项
            if (item->type() == QGraphicsLineItem::Type || 
                (item->type() == QGraphicsEllipseItem::Type && 
                 (item->zValue() > 1000))) { // 高z值的临时标记
                m_scene->removeItem(item);
                delete item;
            }
        }
    }
    
    if (!m_scene) return;
    
    // 创建预览线条
    QPen previewPen(Qt::gray, 1, Qt::DashLine);
    QGraphicsLineItem *lineItem = m_scene->addLine(QLineF(m_startPoint, m_endPoint), previewPen);
    lineItem->setZValue(1001); // 设置高z值确保在顶层
    
    // 添加起点和终点标记
    QGraphicsEllipseItem *startMarker = m_scene->addEllipse(-3, -3, 6, 6, QPen(Qt::black), QBrush(Qt::red));
    startMarker->setPos(m_startPoint);
    startMarker->setZValue(1002);
    
    QGraphicsEllipseItem *endMarker = m_scene->addEllipse(-3, -3, 6, 6, QPen(Qt::black), QBrush(Qt::blue));
    endMarker->setPos(m_endPoint);
    endMarker->setZValue(1002);
}