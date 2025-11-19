#include "drawing-tool-pen.h"
#include "drawingscene.h"
#include "drawingview.h"
#include "drawing-shape.h"
#include "mainwindow.h"
#include "colorpalette.h"
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QPen>
#include <QBrush>

DrawingToolPen::DrawingToolPen(QObject *parent)
    : ToolBase(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_mode(AnchorMode)
    , m_isDrawing(false)
    , m_isDragging(false)
    , m_previewPathItem(nullptr)
    , m_currentStrokeColor(Qt::black)
    , m_currentFillColor(Qt::transparent)
    , m_strokeWidth(2.0)
    , m_autoClose(false)
    , m_showControlPoints(true)
{
}

void DrawingToolPen::activate(DrawingScene *scene, DrawingView *view)
{
    m_scene = scene;
    m_view = view;
    
    // 获取当前颜色
    m_currentStrokeColor = getCurrentStrokeColor();
    m_currentFillColor = getCurrentFillColor();
    
    clearCurrentPath();
}

void DrawingToolPen::deactivate()
{
    if (m_isDrawing) {
        finishPath();
    }
    
    clearCurrentPath();
    m_scene = nullptr;
    m_view = nullptr;
}

bool DrawingToolPen::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene) return false;
    
    if (event->button() == Qt::LeftButton) {
        if (!m_isDrawing) {
            // 开始新的路径
            m_isDrawing = true;
            m_isDragging = true;
            m_dragStart = scenePos;
            addAnchorPoint(scenePos);
            return true;
        } else {
            // 检查是否接近第一个锚点（闭合路径）
            if (m_anchorPoints.size() > 2 && isNearFirstAnchor(scenePos)) {
                finishPath();
                return true;
            }
            
            // 检查是否接近现有锚点（编辑模式）
            QPointF nearestAnchor = findNearestAnchor(scenePos);
            if (!nearestAnchor.isNull() && nearestAnchor != m_anchorPoints.last()) {
                // 连接到现有锚点
                addAnchorPoint(nearestAnchor);
                return true;
            }
            
            // 添加新的锚点并继续拖动
            addAnchorPoint(scenePos);
            m_isDragging = true;
            m_dragStart = scenePos;
            return true;
        }
    }
    
    return false;
}

bool DrawingToolPen::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || !m_isDrawing) return false;
    
    if (m_isDragging) {
        // 自由绘制模式：实时添加点到路径
        qreal distance = QLineF(m_dragStart, scenePos).length();
        if (distance > 2.0) { // 只有移动一定距离才添加新点
            addAnchorPoint(scenePos);
            m_dragStart = scenePos;
        }
        return true;
    }
    
    // 更新鼠标悬停状态
    updateAnchorDisplay();
    return true;
}

bool DrawingToolPen::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || !m_isDrawing) return false;
    
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        // 不结束路径，继续绘制
        return true;
    }
    
    return false;
}

bool DrawingToolPen::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || !m_isDrawing) return false;
    
    if (event->button() == Qt::LeftButton) {
        finishPath();
        return true;
    }
    
    return false;
}

bool DrawingToolPen::keyPressEvent(QKeyEvent *event)
{
    if (!m_scene || !m_isDrawing) return false;
    
    if (event->key() == Qt::Key_Escape) {
        clearCurrentPath();
        m_isDrawing = false;
        return true;
    }
    
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finishPath();
        return true;
    }
    
    return false;
}

void DrawingToolPen::addAnchorPoint(const QPointF &scenePos)
{
    m_anchorPoints.append(scenePos);
    
    // 更新路径 - 使用平滑连接
    if (m_anchorPoints.size() == 1) {
        m_currentPath.moveTo(scenePos);
    } else if (m_anchorPoints.size() == 2) {
        m_currentPath.lineTo(scenePos);
    } else {
        // 使用二次贝塞尔曲线平滑连接
        QPointF prev = m_anchorPoints[m_anchorPoints.size() - 2];
        QPointF current = scenePos;
        QPointF control = QPointF(
            (prev.x() + current.x()) / 2,
            (prev.y() + current.y()) / 2
        );
        m_currentPath.quadTo(control, current);
    }
    
    // 绘制锚点（只在关键点显示）
    if (m_anchorPoints.size() % 5 == 0 || m_anchorPoints.size() <= 2) {
        drawAnchorPoint(scenePos, true);
    }
    
    // 更新预览
    updatePreviewPath();
}

void DrawingToolPen::beginCurveAnchor(const QPointF &scenePos)
{
    m_isDragging = true;
    m_dragStart = scenePos;
    m_currentAnchor = m_anchorPoints.last();
    m_currentControl = scenePos;
    
    // 移除最后的直线段 - 重建路径而不是修改元素
    if (m_currentPath.elementCount() > 1) {
        QPainterPath newPath;
        newPath.moveTo(m_anchorPoints.first());
        for (int i = 1; i < m_anchorPoints.size() - 1; ++i) {
            newPath.lineTo(m_anchorPoints[i]);
        }
        m_currentPath = newPath;
    }
}

void DrawingToolPen::updateCurveAnchor(const QPointF &scenePos)
{
    if (!m_isDragging) return;
    
    m_currentControl = scenePos;
    
    // 重建路径
    m_currentPath = QPainterPath();
    m_currentPath.moveTo(m_anchorPoints.first());
    
    for (int i = 1; i < m_anchorPoints.size(); ++i) {
        if (i == m_anchorPoints.size() - 1) {
            // 最后一个锚点，使用曲线连接
            QPointF prevAnchor = m_anchorPoints[i - 1];
            QPointF currentAnchor = m_anchorPoints[i];
            QPointF control1 = prevAnchor + (m_currentControl - prevAnchor) * 0.5;
            QPointF control2 = currentAnchor + (m_currentControl - currentAnchor) * 0.5;
            
            m_currentPath.cubicTo(control1, control2, currentAnchor);
        } else {
            m_currentPath.lineTo(m_anchorPoints[i]);
        }
    }
    
    // 更新预览
    updatePreviewPath();
    
    // 绘制控制点和控制线
    updateAnchorDisplay();
}

void DrawingToolPen::endCurveAnchor(const QPointF &scenePos)
{
    m_isDragging = false;
    m_mode = AnchorMode;
    
    // 保存控制点
    int lastIndex = m_controlPoints.size() - 2;
    if (lastIndex >= 0) {
        m_controlPoints[lastIndex] = m_currentControl;
        m_controlPoints[lastIndex + 1] = m_currentControl;
    }
}

void DrawingToolPen::createPathShape()
{
    if (!m_scene || m_currentPath.isEmpty()) return;
    
    // 创建DrawingPath对象
    DrawingPath *pathShape = new DrawingPath();
    pathShape->setPath(m_currentPath);
    pathShape->setStrokePen(QPen(m_currentStrokeColor, m_strokeWidth));
    pathShape->setFillBrush(QBrush(m_currentFillColor));
    pathShape->setZValue(1);
    
    // 添加到场景
    m_scene->addItem(pathShape);
    
    // 设置为选中状态，这样用户可以看到选择框
    pathShape->setSelected(true);
    
    m_scene->setModified(true);
}

void DrawingToolPen::updatePreviewPath()
{
    // 清理旧的预览路径
    if (m_previewPathItem) {
        m_scene->removeItem(m_previewPathItem);
        delete m_previewPathItem;
        m_previewPathItem = nullptr;
    }
    
    if (!m_scene || m_currentPath.isEmpty()) return;
    
    // 创建新的预览路径
    m_previewPathItem = m_scene->addPath(m_currentPath);
    m_previewPathItem->setPen(QPen(m_currentStrokeColor, m_strokeWidth, Qt::DashLine));
    m_previewPathItem->setBrush(Qt::NoBrush);
    m_previewPathItem->setZValue(1000);
}

void DrawingToolPen::clearCurrentPath()
{
    // 清理预览路径
    if (m_previewPathItem) {
        m_scene->removeItem(m_previewPathItem);
        delete m_previewPathItem;
        m_previewPathItem = nullptr;
    }
    
    // 清理锚点显示
    for (QGraphicsEllipseItem *item : m_anchorItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_anchorItems.clear();
    
    // 清理控制点显示
    for (QGraphicsEllipseItem *item : m_controlItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_controlItems.clear();
    
    // 清理控制线显示
    for (QGraphicsLineItem *item : m_controlLineItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_controlLineItems.clear();
    
    // 重置数据
    m_anchorPoints.clear();
    m_controlPoints.clear();
    m_currentPath = QPainterPath();
    m_isDrawing = false;
    m_isDragging = false;
}

void DrawingToolPen::finishPath()
{
    if (!m_isDrawing || m_anchorPoints.size() < 2) {
        clearCurrentPath();
        return;
    }
    
    // 自动闭合路径
    if (m_autoClose && m_anchorPoints.size() > 2) {
        m_currentPath.closeSubpath();
    }
    
    // 创建最终的路径图形
    createPathShape();
    
    // 清理当前路径
    clearCurrentPath();
}

void DrawingToolPen::onStrokeColorChanged(const QColor &color)
{
    m_currentStrokeColor = color;
    updatePreviewPath();
}

void DrawingToolPen::onFillColorChanged(const QColor &color)
{
    m_currentFillColor = color;
}

QColor DrawingToolPen::getCurrentStrokeColor() const
{
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    if (mainWindow) {
        ColorPalette *palette = mainWindow->findChild<ColorPalette*>();
        if (palette) {
            return palette->getCurrentStrokeColor();
        }
    }
    return m_currentStrokeColor;
}

QColor DrawingToolPen::getCurrentFillColor() const
{
    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    if (mainWindow) {
        ColorPalette *palette = mainWindow->findChild<ColorPalette*>();
        if (palette) {
            return palette->getCurrentFillColor();
        }
    }
    return m_currentFillColor;
}

void DrawingToolPen::drawAnchorPoint(const QPointF &pos, bool isSelected)
{
    if (!m_scene) return;
    
    QGraphicsEllipseItem *anchorItem = m_scene->addEllipse(-4, -4, 8, 8);
    anchorItem->setPos(pos);
    anchorItem->setPen(QPen(isSelected ? Qt::blue : Qt::black, 2));
    anchorItem->setBrush(QBrush(isSelected ? Qt::white : Qt::lightGray));
    anchorItem->setZValue(1001);
    
    m_anchorItems.append(anchorItem);
}

void DrawingToolPen::drawControlPoint(const QPointF &pos)
{
    if (!m_scene) return;
    
    QGraphicsEllipseItem *controlItem = m_scene->addEllipse(-3, -3, 6, 6);
    controlItem->setPos(pos);
    controlItem->setPen(QPen(Qt::red, 1));
    controlItem->setBrush(QBrush(Qt::white));
    controlItem->setZValue(1002);
    
    m_controlItems.append(controlItem);
}

void DrawingToolPen::drawControlLine(const QPointF &anchor, const QPointF &control)
{
    if (!m_scene) return;
    
    QGraphicsLineItem *lineItem = m_scene->addLine(QLineF(anchor, control));
    lineItem->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    lineItem->setZValue(999);
    
    m_controlLineItems.append(lineItem);
}

void DrawingToolPen::updateAnchorDisplay()
{
    // 清理旧的显示
    for (QGraphicsEllipseItem *item : m_controlItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_controlItems.clear();
    
    for (QGraphicsLineItem *item : m_controlLineItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_controlLineItems.clear();
    
    if (!m_showControlPoints || !m_isDragging) return;
    
    // 绘制控制点和控制线
    if (!m_currentAnchor.isNull() && !m_currentControl.isNull()) {
        drawControlPoint(m_currentControl);
        drawControlLine(m_currentAnchor, m_currentControl);
    }
}

QPointF DrawingToolPen::findNearestAnchor(const QPointF &scenePos, qreal threshold)
{
    QPointF nearest;
    qreal minDistance = threshold;
    
    for (const QPointF &anchor : m_anchorPoints) {
        qreal distance = QLineF(scenePos, anchor).length();
        if (distance < minDistance) {
            minDistance = distance;
            nearest = anchor;
        }
    }
    
    return nearest;
}

bool DrawingToolPen::isNearFirstAnchor(const QPointF &scenePos, qreal threshold)
{
    if (m_anchorPoints.isEmpty()) return false;
    
    qreal distance = QLineF(scenePos, m_anchorPoints.first()).length();
    return distance < threshold;
}