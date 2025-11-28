#include <QGraphicsScene>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QPen>
#include <QBrush>
#include <QElapsedTimer>
#include <QtMath>
#include <QDebug>
#include "drawing-tool-pen.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"

#include "../ui/mainwindow.h"
#include "../ui/colorpalette.h"
#include "../ui/command-manager.h"

DrawingToolPen::DrawingToolPen(QObject *parent)
    : ToolBase(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_mode(FreeDrawMode)  // 默认使用自由绘制模式
    , m_isDrawing(false)
    , m_isDragging(false)
    , m_brushEngine(new BrushEngine(this))
    , m_currentPath(nullptr)
    , m_previewPathItem(nullptr)
    , m_currentStrokeColor(Qt::black)
    , m_currentFillColor(Qt::transparent)
    , m_strokeWidth(3.0)  // 钢笔工具使用更宽的默认线宽
    , m_autoClose(false)
    , m_showControlPoints(true)
    , m_pressureSupport(true)
    , m_pressureSensitivity(0.8)
{
    // 加载钢笔预设
    m_brushEngine->loadDefaultProfile("Fountain Pen");
    
    // 钢笔工具不需要对象池，直接创建对象更简单高效
    
    // 连接信号
    connect(m_brushEngine, &BrushEngine::strokeUpdated, this, [this]() {
        if (m_currentPath) {
            m_currentPath->setPath(m_brushEngine->getStrokePath());
            // 应用笔锋效果
            QPainterPath path = m_brushEngine->getStrokePath();
            applyPenTipEffect(path);
            m_currentPath->setPath(path);
        }
    });
}

void DrawingToolPen::activate(DrawingScene *scene, DrawingView *view)
{
    m_scene = scene;
    m_view = view;
    
    // 获取当前颜色
    m_currentStrokeColor = getCurrentStrokeColor();
    m_currentFillColor = getCurrentFillColor();
    
    // 配置画笔引擎
    BrushProfile profile = m_brushEngine->currentProfile();
    profile.baseWidth = m_strokeWidth;
    profile.pressureSensitivity = m_pressureSensitivity;
    m_brushEngine->loadProfile(profile);
    
    clearCurrentPath();
}

void DrawingToolPen::deactivate()
{
    if (m_isDrawing) {
        if (m_mode == FreeDrawMode && m_currentPath) {
            // 对于自由绘制模式，需要手动完成绘制
            endFreeDraw();
        } else {
            // 对于锚点模式，使用 finishPath
            finishPath();
        }
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
            
            // 根据模式处理
            if (m_mode == FreeDrawMode) {
                beginFreeDraw(scenePos);
            } else {
                addAnchorPoint(scenePos);
            }
            return true;
        } else {
            // 根据模式处理
            if (m_mode == FreeDrawMode) {
                // 自由绘制模式下，继续绘制
                return true;
            } else {
                // 锚点模式
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
    }
    
    return false;
}

bool DrawingToolPen::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || !m_isDrawing) return false;
    
    if (m_isDragging) {
        if (m_mode == FreeDrawMode) {
            // 自由绘制模式：使用画笔引擎
            updateFreeDraw(scenePos);
        } else {
            // 锚点模式：实时添加点到路径
            qreal distance = QLineF(m_dragStart, scenePos).length();
            if (distance > 2.0) { // 只有移动一定距离才添加新点
                addAnchorPoint(scenePos);
                m_dragStart = scenePos;
            }
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
        
        if (m_mode == FreeDrawMode) {
            endFreeDraw();
        }
        // 锚点模式下不结束路径，继续绘制
        
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
        m_penPath.moveTo(scenePos);
    } else if (m_anchorPoints.size() == 2) {
        m_penPath.lineTo(scenePos);
    } else {
        // 使用二次贝塞尔曲线平滑连接
        QPointF prev = m_anchorPoints[m_anchorPoints.size() - 2];
        QPointF current = scenePos;
        QPointF control = QPointF(
            (prev.x() + current.x()) / 2,
            (prev.y() + current.y()) / 2
        );
        m_penPath.quadTo(control, current);
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
    if (m_penPath.elementCount() > 1) {
        QPainterPath newPath;
        newPath.moveTo(m_anchorPoints.first());
        for (int i = 1; i < m_anchorPoints.size() - 1; ++i) {
            newPath.lineTo(m_anchorPoints[i]);
        }
        m_penPath = newPath;
    }
}

void DrawingToolPen::updateCurveAnchor(const QPointF &scenePos)
{
    if (!m_isDragging) return;
    
    m_currentControl = scenePos;
    
    // 重建路径
    m_penPath = QPainterPath();
    m_penPath.moveTo(m_anchorPoints.first());
    
    for (int i = 1; i < m_anchorPoints.size(); ++i) {
        if (i == m_anchorPoints.size() - 1) {
            // 最后一个锚点，使用曲线连接
            QPointF prevAnchor = m_anchorPoints[i - 1];
            QPointF currentAnchor = m_anchorPoints[i];
            QPointF control1 = prevAnchor + (m_currentControl - prevAnchor) * 0.5;
            QPointF control2 = currentAnchor + (m_currentControl - currentAnchor) * 0.5;
            
            m_penPath.cubicTo(control1, control2, currentAnchor);
        } else {
            m_penPath.lineTo(m_anchorPoints[i]);
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
    if (!m_scene || m_penPath.isEmpty()) return;
    
    // 创建DrawingPath对象
    DrawingPath *pathShape = new DrawingPath();
    pathShape->setPath(m_penPath);
    pathShape->setStrokePen(QPen(m_currentStrokeColor, m_strokeWidth));
    pathShape->setFillBrush(QBrush(m_currentFillColor));
    pathShape->setZValue(1);
    
    // 添加到活动图层
    LayerManager *layerManager = LayerManager::instance();
    DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
    
    if (activeLayer) {
        activeLayer->addShape(pathShape);
        qDebug() << "Added pathShape to active layer:" << activeLayer->name();
    } else {
        // 如果没有活动图层，直接添加到场景（向后兼容）
        m_scene->addItem(pathShape);
        qDebug() << "No active layer, added pathShape directly to scene";
    }
    
    // 设置为选中状态，这样用户可以看到选择框
    pathShape->setShowSelectionIndicator(false);
    
    // 创建撤销命令
    class PenAddCommand : public QUndoCommand
    {
    public:
        PenAddCommand(DrawingScene *scene, DrawingPath *path, DrawingLayer *layer, QUndoCommand *parent = nullptr)
            : QUndoCommand("添加钢笔路径", parent), m_scene(scene), m_path(path), m_layer(layer), m_pathOwnedByCommand(false) {}
        
        ~PenAddCommand() {
            // 如果命令拥有路径对象且路径不在场景中，则删除对象
            if (m_pathOwnedByCommand && m_path && !m_path->scene()) {
                delete m_path;
                m_path = nullptr;
            }
        }
        
        void undo() override {
            if (m_path && m_layer) {
                m_layer->removeShape(static_cast<DrawingShape*>(m_path));
                m_path->setVisible(false);
                m_path->setSelected(false); // 取消选中状态
                
                // 命令现在拥有路径对象的责任
                m_pathOwnedByCommand = true;
                
                // 发送清理所有手柄信号，通知所有工具清理手柄
                if (m_scene) {
                    emit m_scene->allToolsClearHandles();
                }
                
                // 通知图层内容变化
                LayerManager *layerManager = LayerManager::instance();
                if (layerManager) {
                    emit layerManager->layerContentChanged(m_layer);
                }
            }
        }
        
        void redo() override {
            if (m_path && m_layer) {
                m_layer->addShape(static_cast<DrawingShape*>(m_path));
                m_path->setVisible(true);
                
                // 自动选中新创建的图形
                m_path->setSelected(true);
                
                // 清除其他选中项
                if (m_scene) {
                    for (QGraphicsItem *item : m_scene->selectedItems()) {
                        if (item != m_path) {
                            item->setSelected(false);
                        }
                    }
                }
                
                // 路径现在由图层管理，命令不再拥有
                m_pathOwnedByCommand = false;
                
                // 通知图层内容变化
                LayerManager *layerManager = LayerManager::instance();
                if (layerManager) {
                    emit layerManager->layerContentChanged(m_layer);
                }
            }
        }
        
    private:
        DrawingScene *m_scene;
        DrawingPath *m_path;
        DrawingLayer *m_layer;
        bool m_pathOwnedByCommand; // 标记命令是否拥有路径对象
    };
    
    // 创建并推送撤销命令
    PenAddCommand *command = new PenAddCommand(m_scene, pathShape, activeLayer);
    if (CommandManager::hasInstance()) {
        CommandManager::instance()->pushCommand(command);
    } else {
        command->redo();
        delete command;
    }
    
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
    
    if (!m_scene || m_penPath.isEmpty()) return;
    
    // 创建新的预览路径
    m_previewPathItem = m_scene->addPath(m_penPath);
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
    m_penPath = QPainterPath();
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
        m_penPath.closeSubpath();
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

void DrawingToolPen::beginFreeDraw(const QPointF &scenePos)
{
    // 清理之前的数据
    m_freeDrawPoints.clear();
    m_pressures.clear();
    m_timer.restart();
    
    // 直接创建路径对象
    m_currentPath = new DrawingPath();
    QPainterPath path;
    path.moveTo(scenePos);
    m_currentPath->setPath(path);
    
    // 设置基本画笔样式（与画笔工具相同）
    QPen pen(m_currentStrokeColor, m_strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    m_currentPath->setStrokePen(pen);
    m_currentPath->setFillBrush(Qt::NoBrush);
    
    // 只添加到场景进行预览，不添加到图层
    m_scene->addItem(m_currentPath);
    qDebug() << "Created pen preview, not yet added to layer";
    
    m_currentPath->setVisible(true);
    m_currentPath->setFlag(QGraphicsItem::ItemIsSelectable, true);
    
    // 添加第一个点
    m_freeDrawPoints.append(scenePos);
    m_pressures.append(1.0);
    m_lastPoint = scenePos;
    
    qDebug() << "Pen tool: Created initial path at" << scenePos;
}

void DrawingToolPen::updateFreeDraw(const QPointF &scenePos)
{
    if (!m_currentPath) return;
    
    // 计算与上一个点的距离（与画笔工具完全相同）
    qreal distance = QLineF(m_lastPoint, scenePos).length();
    
    // 只有当移动距离足够大时才添加点（避免过多的点）
    if (distance > 2.0) {
        m_freeDrawPoints.append(scenePos);
        m_lastPoint = scenePos;
        
        // 直接更新路径（与画笔工具完全相同）
        QPainterPath path;
        path.moveTo(m_freeDrawPoints.first());
        for (int i = 1; i < m_freeDrawPoints.size(); ++i) {
            path.lineTo(m_freeDrawPoints[i]);
        }
        
        m_currentPath->setPath(path);
        
        qDebug() << "Pen tool: Updated path with" << m_freeDrawPoints.size() << "points";
    }
}

void DrawingToolPen::endFreeDraw()
{
    if (!m_currentPath) return;
    
    // 设置控制点
    m_currentPath->setControlPoints(m_freeDrawPoints);
    
    // 检查路径是否太小，如果是则删除
    QRectF boundingRect = m_currentPath->boundingRect();
    if (boundingRect.width() <= 5 && boundingRect.height() <= 5) {
        // 太小了，删除
        m_scene->removeItem(m_currentPath);
        // 删除路径对象
        delete m_currentPath;
        m_currentPath = nullptr;
        m_freeDrawPoints.clear();
        m_pressures.clear();
        m_isDrawing = false;
        qDebug() << "Pen stroke too small, deleted";
        return;
    }
    
    // 路径大小合适，现在才添加到图层
    // 先从场景中移除（因为已经添加了）
    m_scene->removeItem(m_currentPath);
    
    // 添加到活动图层
    LayerManager *layerManager = LayerManager::instance();
    DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
    
    if (activeLayer) {
        activeLayer->addShape(m_currentPath);
        qDebug() << "Added pen stroke to active layer:" << activeLayer->name();
    } else {
        // 如果没有活动图层，直接添加到场景（向后兼容）
        m_scene->addItem(m_currentPath);
        qDebug() << "No active layer, added pen stroke directly to scene";
    }
    
    // 不自动选中，避免显示选择框
    m_currentPath->setSelected(false);
    m_currentPath->setVisible(true);
    m_currentPath->setFlag(QGraphicsItem::ItemIsSelectable, true);
    
    // 创建撤销命令
    class PenFreeDrawCommand : public QUndoCommand
    {
    public:
        PenFreeDrawCommand(DrawingScene *scene, DrawingPath *path, DrawingLayer *layer, QUndoCommand *parent = nullptr)
            : QUndoCommand("添加钢笔自由绘制", parent), m_scene(scene), m_path(path), m_layer(layer) {}
        
        void undo() override {
            if (m_path && m_layer) {
                m_layer->removeShape(static_cast<DrawingShape*>(m_path));
                m_path->setVisible(false);
                
                // 通知图层内容变化
                LayerManager *layerManager = LayerManager::instance();
                if (layerManager) {
                    emit layerManager->layerContentChanged(m_layer);
                }
            }
        }
        
        void redo() override {
            if (m_path && m_layer) {
                m_layer->addShape(static_cast<DrawingShape*>(m_path));
                m_path->setVisible(true);
                
                // 通知图层内容变化
                LayerManager *layerManager = LayerManager::instance();
                if (layerManager) {
                    emit layerManager->layerContentChanged(m_layer);
                }
            }
        }
        
    private:
        DrawingScene *m_scene;
        DrawingPath *m_path;
        DrawingLayer *m_layer;
    };
    
    // 创建并推送撤销命令
    PenFreeDrawCommand *command = new PenFreeDrawCommand(m_scene, m_currentPath, activeLayer);
    if (CommandManager::hasInstance()) {
        CommandManager::instance()->pushCommand(command);
    } else {
        command->redo();
        delete command;
    }
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    qDebug() << "Pen tool: Finished drawing with" << m_freeDrawPoints.size() << "points";
    
    // 清理当前路径，但保持工具激活状态以便继续绘制
    m_currentPath = nullptr;
    m_freeDrawPoints.clear();
    m_pressures.clear();
    m_isDrawing = false;  // 重置绘制状态，允许开始新的绘制
}

void DrawingToolPen::applyPenTipEffect(QPainterPath& path)
{
    if (m_freeDrawPoints.size() < 3) return;
    
    // 创建渐变的笔锋效果
    QVector<qreal> widths;
    createTaperedPath(m_freeDrawPoints, widths);
    
    // 重建路径
    QPainterPath newPath;
    
    if (m_freeDrawPoints.size() >= 2) {
        // 起始点（笔锋）
        QPointF startPoint = m_freeDrawPoints[0];
        QPointF nextPoint = m_freeDrawPoints[1];
        QPointF direction = nextPoint - startPoint;
        qreal angle = qAtan2(direction.y(), direction.x()) + M_PI/2;
        
        qreal startWidth = widths[0] * 0.3; // 起笔较细
        QPointF offset1 = QPointF(qCos(angle) * startWidth/2, qSin(angle) * startWidth/2);
        QPointF offset2 = QPointF(qCos(angle + M_PI) * startWidth/2, qSin(angle + M_PI) * startWidth/2);
        
        newPath.moveTo(startPoint + offset1);
        
        // 绘制路径主体
        for (int i = 1; i < m_freeDrawPoints.size() - 1; ++i) {
            QPointF current = m_freeDrawPoints[i];
            qreal width = widths[i];
            
            // 计算垂直方向
            QPointF next = m_freeDrawPoints[i + 1];
            QPointF prev = m_freeDrawPoints[i - 1];
            QPointF dir = next - prev;
            angle = qAtan2(dir.y(), dir.x()) + M_PI/2;
            
            offset1 = QPointF(qCos(angle) * width/2, qSin(angle) * width/2);
            newPath.lineTo(current + offset1);
        }
        
        // 结束点（笔锋）
        QPointF endPoint = m_freeDrawPoints.last();
        QPointF prevPoint = m_freeDrawPoints[m_freeDrawPoints.size() - 2];
        direction = endPoint - prevPoint;
        angle = qAtan2(direction.y(), direction.x()) + M_PI/2;
        
        qreal endWidth = widths.last() * 0.3; // 收笔较细
        offset1 = QPointF(qCos(angle) * endWidth/2, qSin(angle) * endWidth/2);
        offset2 = QPointF(qCos(angle + M_PI) * endWidth/2, qSin(angle + M_PI) * endWidth/2);
        
        newPath.lineTo(endPoint + offset1);
        
        // 返回路径
        for (int i = m_freeDrawPoints.size() - 2; i > 0; --i) {
            QPointF current = m_freeDrawPoints[i];
            qreal width = widths[i];
            
            QPointF next = m_freeDrawPoints[i + 1];
            QPointF prev = m_freeDrawPoints[i - 1];
            QPointF dir = next - prev;
            angle = qAtan2(dir.y(), dir.x()) + M_PI/2;
            
            offset2 = QPointF(qCos(angle + M_PI) * width/2, qSin(angle + M_PI) * width/2);
            newPath.lineTo(current + offset2);
        }
        
        newPath.lineTo(startPoint + offset2);
        newPath.closeSubpath();
    }
    
    path = newPath;
}

void DrawingToolPen::createTaperedPath(const QVector<QPointF>& points, QVector<qreal>& widths)
{
    widths.clear();
    
    if (points.size() == 0) return;
    
    // 基础宽度
    qreal baseWidth = m_strokeWidth;
    
    for (int i = 0; i < points.size(); ++i) {
        qreal width = baseWidth;
        
        // 应用压力
        if (i < m_pressures.size()) {
            width *= m_pressures[i];
        }
        
        // 起笔和收笔的笔锋效果
        if (points.size() > 10) {
            qreal taperLength = points.size() * 0.1; // 10%的长度用于笔锋
            
            if (i < taperLength) {
                // 起笔渐细
                qreal factor = i / taperLength;
                width *= (0.3 + 0.7 * factor);
            } else if (i > points.size() - taperLength - 1) {
                // 收笔渐细
                qreal factor = (points.size() - 1 - i) / taperLength;
                width *= (0.3 + 0.7 * factor);
            }
        }
        
        widths.append(width);
    }
}

void DrawingToolPen::setBrushProfile(const QString& profileName)
{
    m_brushEngine->loadDefaultProfile(profileName);
}

void DrawingToolPen::setBrushWidth(qreal width)
{
    m_strokeWidth = width;
    BrushProfile profile = m_brushEngine->currentProfile();
    profile.baseWidth = width;
    m_brushEngine->loadProfile(profile);
}

void DrawingToolPen::setPressureSensitivity(qreal sensitivity)
{
    m_pressureSensitivity = qBound(0.0, sensitivity, 1.0);
    BrushProfile profile = m_brushEngine->currentProfile();
    profile.pressureSensitivity = m_pressureSensitivity;
    m_brushEngine->loadProfile(profile);
}

void DrawingToolPen::togglePressureSupport(bool enabled)
{
    m_pressureSupport = enabled;
}