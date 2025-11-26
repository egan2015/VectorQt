#include <QMouseEvent>
#include <QKeyEvent>
#include <QPen>
#include <QBrush>
#include "../core/toolbase.h"
#include "../ui/drawingview.h"
#include "../ui/drawingscene.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"
#include "../ui/command-manager.h"

// ToolBase
ToolBase::ToolBase(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_state(ToolState::Inactive)
{
}

void ToolBase::activate(DrawingScene *scene, DrawingView *view)
{
    if (m_state == ToolState::Active) {
        return; // 已经激活
    }
    
    setState(ToolState::Activating);
    
    m_scene = scene;
    m_view = view;
    
    // 设置工具光标
    setToolCursor();
    
    setState(ToolState::Active);
    onActivated();
    emit activated();
}

void ToolBase::deactivate()
{
    if (m_state == ToolState::Inactive || m_state == ToolState::Deactivating) {
        return; // 已经未激活或正在停用
    }
    
    if (!canDeactivate()) {
        return; // 不允许停用
    }
    
    setState(ToolState::Deactivating);
    
    // 恢复默认光标
    restoreDefaultCursor();
    
    onDeactivated();
    setState(ToolState::Inactive);
    emit deactivated();
    
    m_scene = nullptr;
    m_view = nullptr;
}

void ToolBase::cleanup()
{
    if (m_state != ToolState::Inactive) {
        deactivate();
    }
}

void ToolBase::setState(ToolState newState)
{
    if (m_state != newState) {
        ToolState oldState = m_state;
        m_state = newState;
        onStateChanged(oldState, newState);
        emit stateChanged(oldState, newState);
    }
}

void ToolBase::setToolCursor()
{
    if (m_view) {
        CursorManager::instance().setCursorForView(m_view, getCursorType());
    }
}

void ToolBase::restoreDefaultCursor()
{
    if (m_view) {
        m_view->setCursor(Qt::ArrowCursor);
    }
}

bool ToolBase::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool ToolBase::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool ToolBase::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool ToolBase::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool ToolBase::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    return false;
}



// LegacyRectangleTool
LegacyRectangleTool::LegacyRectangleTool(QObject *parent)
    : ToolBase(parent)
    , m_drawing(false)
    , m_previewItem(nullptr)
    , m_currentItem(nullptr)
{
}

void LegacyRectangleTool::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    // 初始化状态
    m_drawing = false;
    m_currentItem = nullptr;
    m_previewItem = nullptr;
}

void LegacyRectangleTool::deactivate()
{
    // 完成当前正在绘制的矩形
    if (m_drawing && m_currentItem) {
        m_drawing = false;
        // 检查是否太小，如果是则删除
        QRectF currentRect = m_currentItem->rectangle();
        if (currentRect.width() <= 5 && currentRect.height() <= 5) {
            // 太小了，删除
            if (m_scene) {
                m_scene->removeItem(m_currentItem);
            }
            delete m_currentItem;
            m_currentItem = nullptr;  // 重要：立即设置为nullptr
        } else {
            // 图形大小合适，添加到图层
            if (m_scene) {
                // 先从场景中移除（因为已经添加了）
                m_scene->removeItem(m_currentItem);
                
                // 添加到活动图层
                LayerManager *layerManager = LayerManager::instance();
                DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
                
                if (activeLayer) {
                    activeLayer->addShape(m_currentItem);
                    qDebug() << "Added rectangle to active layer on deactivate:" << activeLayer->name();
                } else {
                    // 如果没有活动图层，直接添加到场景（向后兼容）
                    m_scene->addItem(m_currentItem);
                    qDebug() << "No active layer, added rectangle directly to scene on deactivate";
                }
                
                m_scene->setModified(true);
            }
            // 不删除，所有权转移给场景/图层
            m_currentItem = nullptr;  // 重要：立即设置为nullptr
        }
    }
    
    // 清理预览项
    if (m_previewItem) {
        if (m_scene) {
            m_scene->removeItem(m_previewItem);
        }
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    
    ToolBase::deactivate();
}

LegacyRectangleTool::~LegacyRectangleTool()
{
    // 清理内存，防止内存泄漏
    if (m_previewItem) {
        if (m_scene) {
            m_scene->removeItem(m_previewItem);
        }
        // previewItem可能还没有被添加到场景中，需要手动删除
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    // m_currentItem应该在deactivate中已经被处理
    // 但为了安全起见，再次检查
    if (m_currentItem) {
        // 检查对象是否仍然有效，通过检查它是否还在场景中
        if (m_scene && m_scene->items().contains(m_currentItem)) {
            m_scene->removeItem(m_currentItem);
        }
        // 只有当对象不在场景中时才删除它
        // 如果对象在场景中，场景会负责删除它
        if (!m_currentItem->scene()) {
            delete m_currentItem;
        }
        m_currentItem = nullptr;
    }
}

bool LegacyRectangleTool::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_startPos = scenePos;
        
        // 清除之前的选择
        if (m_scene) {
            m_scene->clearSelection();
        }
        
        // 创建新的DrawingRectangle，但先不添加到图层
        m_currentItem = new DrawingRectangle(QRectF(0, 0, 1, 1));
        m_currentItem->setFillBrush(QBrush(Qt::yellow));
        m_currentItem->setStrokePen(QPen(Qt::black, 2));
        m_currentItem->setPos(scenePos);  // 关键：使用setPos设置位置
        
        // 只添加到场景进行预览，不添加到图层
        if (m_scene) {
            m_scene->addItem(m_currentItem);
            qDebug() << "Created rectangle preview, not yet added to layer";
        }
        
        return true;
    }
    return false;
}

bool LegacyRectangleTool::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentItem && m_scene) {
        // 计算对齐后的位置
        // 在创建新对象时，不排除任何对象，以便可以吸附到已有对象
        QPointF alignedPos = smartSnap(scenePos, nullptr);
        
        // 计算新的矩形大小，基于起始点和当前位置
        QPointF delta = alignedPos - m_startPos;
        QRectF newRect;
        
        if (delta.x() >= 0 && delta.y() >= 0) {
            // 右下拖动
            newRect = QRectF(0, 0, qMax(delta.x(), 1.0), qMax(delta.y(), 1.0));
            m_currentItem->setPos(m_startPos);
        } else if (delta.x() < 0 && delta.y() >= 0) {
            // 左下拖动
            newRect = QRectF(0, 0, qAbs(delta.x()), qMax(delta.y(), 1.0));
            m_currentItem->setPos(QPointF(alignedPos.x(), m_startPos.y()));
        } else if (delta.x() >= 0 && delta.y() < 0) {
            // 右上拖动
            newRect = QRectF(0, 0, qMax(delta.x(), 1.0), qAbs(delta.y()));
            m_currentItem->setPos(QPointF(m_startPos.x(), alignedPos.y()));
        } else {
            // 左上拖动
            newRect = QRectF(0, 0, qAbs(delta.x()), qAbs(delta.y()));
            m_currentItem->setPos(alignedPos);
        }
        
        // 只有当矩形真正发生变化时才设置
        if (m_currentItem->rectangle() != newRect) {
            m_currentItem->setRectangle(newRect);
        }
        return true;
    }
    return false;
}

bool LegacyRectangleTool::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        
        if (m_currentItem) {
            // 检查是否太小，如果是则删除
            QRectF currentRect = m_currentItem->rectangle();
            if (currentRect.width() <= 5 && currentRect.height() <= 5) {
                // 太小了，删除
                if (m_scene) {
                    m_scene->removeItem(m_currentItem);
                }
                delete m_currentItem;
                m_currentItem = nullptr;
            } else {
                // 图形大小合适，现在才添加到图层
                if (m_scene) {
                    // 先从场景中移除（因为已经添加了）
                    m_scene->removeItem(m_currentItem);
                    
                    // 添加到活动图层
                    LayerManager *layerManager = LayerManager::instance();
                    DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
                    
                    if (activeLayer) {
                        activeLayer->addShape(m_currentItem);
                        qDebug() << "Added rectangle to active layer:" << activeLayer->name();
                    } else {
                        // 如果没有活动图层，直接添加到场景（向后兼容）
                        m_scene->addItem(m_currentItem);
                        qDebug() << "No active layer, added rectangle directly to scene";
                    }
                    
                    m_scene->setModified(true);
                    
                    // 创建一个简单的撤销命令
                    // 使用统一的CreateCommand
                    CreateCommand *command = new CreateCommand(m_scene->commandManager(), m_currentItem, "添加矩形");
                    if (m_scene->commandManager()) {
                        m_scene->commandManager()->pushCommand(command);
                    } else {
                        // 降级处理：直接使用撤销栈
                        command->redo();
                        delete command;
                    }
                }
                // 重要：将所有权转移给场景/图层，不再由工具管理
                m_currentItem = nullptr;
            }
        }
        
        return true;
    }
    return false;
}

// LegacyEllipseTool
LegacyEllipseTool::LegacyEllipseTool(QObject *parent)
    : ToolBase(parent)
    , m_drawing(false)
    , m_previewItem(nullptr)
    , m_currentItem(nullptr)
{
}

void LegacyEllipseTool::activate(DrawingScene *scene, DrawingView *view)
{
    qDebug() << "LegacyEllipseTool::activate called, scene:" << scene << "view:" << view;
    
    ToolBase::activate(scene, view);
    // 初始化状态
    m_drawing = false;
    m_currentItem = nullptr;
    m_previewItem = nullptr;
    
    qDebug() << "LegacyEllipseTool::activate completed";
}

void LegacyEllipseTool::deactivate()
{
    // 完成当前正在绘制的椭圆
    if (m_drawing && m_currentItem) {
        m_drawing = false;
        // 检查是否太小，如果是则删除
        QRectF currentRect = m_currentItem->ellipse();
        if (currentRect.width() <= 5 && currentRect.height() <= 5) {
            // 太小了，删除
            if (m_scene) {
                m_scene->removeItem(m_currentItem);
            }
            delete m_currentItem;
            m_currentItem = nullptr;  // 重要：立即设置为nullptr
        } else {
            // 图形大小合适，添加到图层
            if (m_scene) {
                // 先从场景中移除（因为已经添加了）
                m_scene->removeItem(m_currentItem);
                
                // 添加到活动图层
                LayerManager *layerManager = LayerManager::instance();
                DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
                
                if (activeLayer) {
                    activeLayer->addShape(m_currentItem);
                    qDebug() << "Added ellipse to active layer on deactivate:" << activeLayer->name();
                } else {
                    // 如果没有活动图层，直接添加到场景（向后兼容）
                    m_scene->addItem(m_currentItem);
                    qDebug() << "No active layer, added ellipse directly to scene on deactivate";
                }
                
                m_scene->setModified(true);
            }
            // 不删除，所有权转移给场景/图层
            m_currentItem = nullptr;  // 重要：立即设置为nullptr
        }
    }
    
    // 清理预览项
    if (m_previewItem) {
        if (m_scene) {
            m_scene->removeItem(m_previewItem);
        }
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    
    ToolBase::deactivate();
}

LegacyEllipseTool::~LegacyEllipseTool()
{
    // 清理内存，防止内存泄漏
    if (m_previewItem) {
        if (m_scene) {
            m_scene->removeItem(m_previewItem);
        }
        // previewItem可能还没有被添加到场景中，需要手动删除
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    if (m_currentItem) {
        // 检查对象是否仍然有效，通过检查它是否还在场景中
        if (m_scene && m_scene->items().contains(m_currentItem)) {
            m_scene->removeItem(m_currentItem);
        }
        // 只有当对象不在场景中时才删除它
        // 如果对象在场景中，场景会负责删除它
        if (!m_currentItem->scene()) {
            delete m_currentItem;
        }
        m_currentItem = nullptr;
    }
}

bool LegacyEllipseTool::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton) {
        m_drawing = true;
        m_startPos = scenePos;
        
        // 清除之前的选择
        if (m_scene) {
            m_scene->clearSelection();
        }
        
        // 创建新的DrawingEllipse，但先不添加到图层
        m_currentItem = new DrawingEllipse(QRectF(0, 0, 1, 1));
        m_currentItem->setFillBrush(QBrush(Qt::cyan));
        m_currentItem->setStrokePen(QPen(Qt::black, 2));
        m_currentItem->setPos(scenePos);  // 关键：使用setPos设置位置
        
        // 只添加到场景进行预览，不添加到图层
        if (m_scene) {
            m_scene->addItem(m_currentItem);
            qDebug() << "Created ellipse preview, not yet added to layer";
        }
        
        return true;
    }
    return false;
}

bool LegacyEllipseTool::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentItem && m_scene) {
        // 计算对齐后的位置
        // 在创建新对象时，不排除任何对象，以便可以吸附到已有对象
        QPointF alignedPos = smartSnap(scenePos, nullptr);
        
        // 计算新的椭圆大小，基于起始点和当前位置
        QPointF delta = alignedPos - m_startPos;
        QRectF newRect;
        
        if (delta.x() >= 0 && delta.y() >= 0) {
            // 右下拖动
            newRect = QRectF(0, 0, qMax(delta.x(), 1.0), qMax(delta.y(), 1.0));
            m_currentItem->setPos(m_startPos);
        } else if (delta.x() < 0 && delta.y() >= 0) {
            // 左下拖动
            newRect = QRectF(0, 0, qAbs(delta.x()), qMax(delta.y(), 1.0));
            m_currentItem->setPos(QPointF(alignedPos.x(), m_startPos.y()));
        } else if (delta.x() >= 0 && delta.y() < 0) {
            // 右上拖动
            newRect = QRectF(0, 0, qMax(delta.x(), 1.0), qAbs(delta.y()));
            m_currentItem->setPos(QPointF(m_startPos.x(), alignedPos.y()));
        } else {
            // 左上拖动
            newRect = QRectF(0, 0, qAbs(delta.x()), qAbs(delta.y()));
            m_currentItem->setPos(alignedPos);
        }
        
        m_currentItem->setEllipse(newRect);
        return true;
    }
    return false;
}

bool LegacyEllipseTool::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        
        if (m_currentItem) {
            // 检查是否太小，如果是则删除
            QRectF currentRect = m_currentItem->ellipse();
            if (currentRect.width() <= 5 && currentRect.height() <= 5) {
                // 太小了，删除
                if (m_scene) {
                    m_scene->removeItem(m_currentItem);
                }
                delete m_currentItem;
                m_currentItem = nullptr;
            } else {
                // 图形大小合适，现在才添加到图层
                if (m_scene) {
                    // 先从场景中移除（因为已经添加了）
                    m_scene->removeItem(m_currentItem);
                    
                    // 添加到活动图层
                    LayerManager *layerManager = LayerManager::instance();
                    DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
                    
                    if (activeLayer) {
                        activeLayer->addShape(m_currentItem);
                        qDebug() << "Added ellipse to active layer:" << activeLayer->name();
                    } else {
                        // 如果没有活动图层，直接添加到场景（向后兼容）
                        m_scene->addItem(m_currentItem);
                        qDebug() << "No active layer, added ellipse directly to scene";
                    }
                    
                    m_scene->setModified(true);
                    
                    // 创建一个简单的撤销命令
                    // 使用统一的CreateCommand
                    CreateCommand *command = new CreateCommand(m_scene->commandManager(), m_currentItem, "添加椭圆");
                    if (m_scene->commandManager()) {
                        m_scene->commandManager()->pushCommand(command);
                    } else {
                        // 降级处理：直接使用撤销栈
                        command->redo();
                        delete command;
                    }
                }
                m_currentItem = nullptr;
            }
        }
    }
    
    return true;
}

QPointF ToolBase::smartSnap(const QPointF &scenePos, DrawingShape *excludeShape)
{
    QPointF alignedPos = scenePos;
    
    if (qobject_cast<DrawingScene*>(m_scene)) {
        DrawingScene *drawingScene = qobject_cast<DrawingScene*>(m_scene);
        
        // 首先尝试对象吸附（优先级更高）
        DrawingScene::ObjectSnapResult objectSnap = drawingScene->snapToObjects(scenePos, excludeShape);
        if (objectSnap.snappedToObject) {
            // 对象吸附成功
            alignedPos = objectSnap.snappedPos;
            // qDebug() << "Object snap triggered:" << objectSnap.snapDescription;
        } else {
            // 对象吸附失败，尝试网格吸附
            if (drawingScene->isGridAlignmentEnabled()) {
                DrawingScene::SnapResult gridSnap = drawingScene->smartAlignToGrid(scenePos);
                if (gridSnap.snappedX || gridSnap.snappedY) {
                    alignedPos = gridSnap.snappedPos;
                    // qDebug() << "Grid snap: X snapped:" << gridSnap.snappedX << "Y snapped:" << gridSnap.snappedY;
                }
            }
        }
        
        // 如果没有发生任何吸附，清除指示器
        if (alignedPos == scenePos) {
            drawingScene->clearSnapIndicators();
        }
    }
    
    return alignedPos;
}

