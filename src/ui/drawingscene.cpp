#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QUndoCommand>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QDebug>
#include <QGraphicsScene>
#include <QTimer>
#include <QPointer>
#include <algorithm>
#include <limits>
#include "../ui/drawingscene.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-group.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"
#include "../core/performance-monitor.h"
#include "../ui/command-manager.h"
#include "../ui/snap-manager.h"

class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(DrawingScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("添加项目", parent), m_scene(scene), m_item(item) 
    {
        // 构造函数中只记录状态，不立即添加到场景
        qDebug() << "AddItemCommand created, item:" << m_item;
    }
    
    void undo() override {
        qDebug() << "AddItemCommand::undo called, item:" << m_item << "scene:" << m_scene << "item->scene:" << (m_item ? m_item->scene() : nullptr);
        
        if (m_item && m_scene) {
            // 只有当item在场景中时才移除
            if (m_item->scene() == m_scene) {
                m_scene->removeItem(m_item);
                m_item->setVisible(false);
                // 强制通知所有工具清理手柄（针对撤销删除操作的特殊处理）
                emit m_scene->allToolsClearHandles();
                qDebug() << "AddItemCommand::undo - removed item from scene and cleared handles";
            } else {
                qDebug() << "AddItemCommand::undo - item not in this scene, current scene:" << m_item->scene() << "target scene:" << m_scene;
                // 如果对象在其他场景中，先从那里移除
                if (m_item->scene()) {
                    m_item->scene()->removeItem(m_item);
                    m_item->setVisible(false);
                    qDebug() << "AddItemCommand::undo - removed item from wrong scene";
                }
            }
        } else {
            qDebug() << "AddItemCommand::undo - item or scene is null";
        }
    }
    
    void redo() override {
        qDebug() << "AddItemCommand::redo called, m_item:" << m_item << "m_scene:" << m_scene;
        
        if (m_item && m_scene) {
            // 检查项目是否已经在目标场景中
            if (m_item->scene() == m_scene) {
                qDebug() << "AddItemCommand::redo - item already in target scene, just making visible";
                m_item->setVisible(true);
                return;
            }
            
            // 先从当前场景移除（如果在某个场景中）
            if (m_item->scene()) {
                qDebug() << "AddItemCommand::redo - removing item from current scene:" << m_item->scene();
                m_item->scene()->removeItem(m_item);
            }
            
            // 添加到目标场景
            qDebug() << "AddItemCommand::redo - adding item to target scene";
            m_scene->addItem(m_item);
            m_item->setVisible(true);
            qDebug() << "AddItemCommand::redo - successfully added item to scene";
        } else {
            qDebug() << "AddItemCommand::redo - m_item or m_scene is null";
        }
    }
    
    ~AddItemCommand() override {
        qDebug() << "AddItemCommand::~AddItemCommand called, item:" << m_item << "in scene:" << (m_item && m_item->scene());
        // 如果对象不在场景中，需要手动删除
        // 如果对象在场景中，scene会管理它的生命周期
        if (m_item && !m_item->scene()) {
            delete m_item;
            qDebug() << "AddItemCommand::~AddItemCommand - deleted item not in scene";
        } else {
            qDebug() << "AddItemCommand::~AddItemCommand - cleanup, item in scene or null";
        }
    }
    
private:
    DrawingScene *m_scene;
    QGraphicsItem *m_item;
};

class RemoveItemCommand : public QUndoCommand
{
public:
    RemoveItemCommand(DrawingScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("删除项目", parent), m_scene(scene), m_item(item), m_wasInScene(false)
    {
        // 记录删除前的状态
        if (m_item && m_item->scene() == m_scene) {
            m_wasInScene = true;
            m_itemVisible = m_item->isVisible();
        }
    }
    
    void undo() override {
        qDebug() << "RemoveItemCommand::undo called, item:" << m_item << "scene:" << m_scene << "item->scene:" << (m_item ? m_item->scene() : nullptr);
        
        if (m_item && m_scene) {
            // 确保item不在任何场景中，避免重复添加
            if (m_item->scene()) {
                if (m_item->scene() != m_scene) {
                    qDebug() << "RemoveItemCommand::undo - item is in wrong scene, removing from:" << m_item->scene();
                }
                m_item->scene()->removeItem(m_item);
            }
            // 添加回场景
            m_scene->addItem(m_item);
            m_item->setVisible(m_itemVisible);
            qDebug() << "RemoveItemCommand::undo - added item back to scene";
        }
    }
    
    void redo() override {
        qDebug() << "RemoveItemCommand::redo called, item:" << m_item << "scene:" << m_scene << "item->scene:" << (m_item ? m_item->scene() : nullptr);
        
        if (m_item && m_scene) {
            // 只有当item在场景中时才移除
            if (m_item->scene() == m_scene) {
                m_scene->removeItem(m_item);
                m_item->setVisible(false);
                
                // 发送清理所有手柄信号，通知所有工具清理手柄
                emit m_scene->allToolsClearHandles();
                
                qDebug() << "RemoveItemCommand::redo - removed item from scene";
            } else if (m_item->scene()) {
                // 如果对象在其他场景中，也从那里移除
                qDebug() << "RemoveItemCommand::redo - removing item from wrong scene:" << m_item->scene();
                QGraphicsScene *wrongScene = m_item->scene();
                wrongScene->removeItem(m_item);
                m_item->setVisible(false);
            } else {
                qDebug() << "RemoveItemCommand::redo - item not in any scene";
            }
        }
    }
    
    ~RemoveItemCommand() override {
        // 只有从未加入过scene的对象才需要手动删除
        // 如果对象曾经被加入过scene，scene会管理它的生命周期
        if (m_item && !m_wasInScene) {
            delete m_item;
            qDebug() << "RemoveItemCommand::~RemoveItemCommand - deleted item that was never in scene";
        } else {
            qDebug() << "RemoveItemCommand::~RemoveItemCommand - cleanup";
        }
    }
    
private:
    DrawingScene *m_scene;
    QGraphicsItem *m_item;
    bool m_wasInScene;
    bool m_itemVisible = true;
};

class SceneTransformCommand : public QUndoCommand
{
public:
    enum TransformType {
        Move,
        Scale,
        Rotate,
        Skew,
        Generic
    };
    
    using TransformState = DrawingScene::TransformState;
    
    SceneTransformCommand(DrawingScene *scene, const QList<DrawingShape*>& shapes, const QList<TransformState>& oldStates, TransformType type = Generic, QUndoCommand *parent = nullptr)
        : QUndoCommand(getCommandText(type, shapes), parent), m_scene(scene), m_shapes(shapes), m_transformType(type), m_oldStates(oldStates)
    {
        // 立即捕获变换后的状态（因为这是在变换结束时调用的）
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                TransformState state;
                state.position = shape->pos();
                state.transform = shape->transform();
                state.rotation = shape->rotation();
                m_newStates.append(state);
            }
        }
    }
    
    // 新的构造函数，接受新状态作为参数
    SceneTransformCommand(DrawingScene *scene, const QList<DrawingShape*>& shapes, const QList<TransformState>& oldStates, const QList<TransformState>& newStates, TransformType type = Generic, QUndoCommand *parent = nullptr)
        : QUndoCommand(getCommandText(type, shapes), parent), m_scene(scene), m_shapes(shapes), m_transformType(type), m_oldStates(oldStates), m_newStates(newStates)
    {
    }
    
    static QString getCommandText(TransformType type) {
        switch (type) {
            case Move: return "移动";
            case Scale: return "缩放";
            case Rotate: return "旋转";
            case Skew: return "斜切";
            default: return "变换";
        }
    }
    
    static QString getCommandText(TransformType type, const QList<DrawingShape*>& shapes) {
        QString baseText;
        switch (type) {
            case Move: baseText = "移动"; break;
            case Scale: baseText = "缩放"; break;
            case Rotate: baseText = "旋转"; break;
            case Skew: baseText = "斜切"; break;
            default: baseText = "变换"; break;
        }
        
        if (shapes.size() == 1) {
            DrawingShape *shape = shapes.first();
            if (shape) {
                switch (shape->shapeType()) {
                    case DrawingShape::Rectangle: return baseText + "矩形";
                    case DrawingShape::Ellipse: return baseText + "椭圆";
                    case DrawingShape::Path: return baseText + "路径";
                    case DrawingShape::Line: return baseText + "直线";
                    case DrawingShape::Polyline: return baseText + "折线";
                    case DrawingShape::Polygon: return baseText + "多边形";
                    case DrawingShape::Text: return baseText + "文本";
                    case DrawingShape::Group: return baseText + "组合";
                    default: return baseText; break;
                }
            }
        } else if (shapes.size() > 1) {
            return baseText + QString::number(shapes.size()) + "个对象";
        }
        
        return baseText;
    }
    
    int id() const override {
        // 为每个变换类型返回不同的ID，防止合并
        return static_cast<int>(m_transformType);
    }
    
    
    
    bool hasChanged() const {
        if (m_oldStates.size() != m_newStates.size()) {
            return true;
        }
        
        for (int i = 0; i < m_oldStates.size(); ++i) {
            const auto &oldState = m_oldStates[i];
            const auto &newState = m_newStates[i];
            
            // 使用更合适的浮点数比较，避免精度问题
            if (qAbs(oldState.position.x() - newState.position.x()) > 0.001 ||
                qAbs(oldState.position.y() - newState.position.y()) > 0.001 ||
                qAbs(oldState.rotation - newState.rotation) > 0.001 ||
                oldState.transform != newState.transform) {
                return true;
            }
        }
        
        return false;
    }
    
    void undo() override {
        qDebug() << "SceneTransformCommand::undo called, shapes count:" << m_shapes.size();
        
        // 恢复到变换前的状态
        for (int i = 0; i < m_shapes.size() && i < m_oldStates.size(); ++i) {
            DrawingShape *shape = m_shapes[i];
            // 检查图形是否仍然有效且在正确的场景中
            if (shape && shape->scene() == m_scene) {
                const TransformState &state = m_oldStates[i];
                qDebug() << "  Restoring shape" << i << "to pos:" << state.position << "transform:" << state.transform;
                
                shape->setPos(state.position);
                shape->applyTransform(state.transform);
                shape->setRotation(state.rotation);
                
                // 老的手柄系统已移除，不再需要更新手柄位置
            } else {
                qDebug() << "  Shape" << i << "is invalid or not in scene (possibly deleted)";
                // 图形可能已被删除，跳过此操作但不报错
            }
        }
        if (m_scene) {
            m_scene->update();
            // 通知所有工具对象状态已变化
            for (DrawingShape *shape : m_shapes) {
                if (shape) {
                    emit m_scene->objectStateChanged(shape);
                }
            }
        }
    }
    
    void redo() override {
        qDebug() << "SceneTransformCommand::redo called, shapes count:" << m_shapes.size();
        
        // 应用到变换后的状态
        for (int i = 0; i < m_shapes.size() && i < m_newStates.size(); ++i) {
            DrawingShape *shape = m_shapes[i];
            // 检查图形是否仍然有效且在正确的场景中
            if (shape && shape->scene() == m_scene) {
                const TransformState &state = m_newStates[i];
                qDebug() << "  Applying shape" << i << "to pos:" << state.position;
                
                shape->setPos(state.position);
                shape->applyTransform(state.transform);
                shape->setRotation(state.rotation);
                
                // 老的手柄系统已移除，不再需要更新手柄位置
            } else {
                qDebug() << "  Shape" << i << "is invalid or not in scene (possibly deleted)";
                // 图形可能已被删除，跳过此操作但不报错
            }
        }
        if (m_scene) {
            m_scene->update();
            // 通知所有工具对象状态已变化
            for (DrawingShape *shape : m_shapes) {
                if (shape) {
                    emit m_scene->objectStateChanged(shape);
                }
            }
        }
    }
    
private:
    DrawingScene *m_scene;
    QList<DrawingShape*> m_shapes;
    QList<TransformState> m_oldStates;
    QList<TransformState> m_newStates;
    TransformType m_transformType;
};





DrawingScene::DrawingScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_isModified(false)
    // , m_selectionLayer(nullptr) // 已移除 - 老的选择层系统
    , m_gridVisible(false)
    , m_gridAlignmentEnabled(true)
    , m_gridSize(20)
    , m_gridColor(QColor(200, 200, 200, 100))
    , m_guidesEnabled(true)
    , m_scaleHintVisible(false)
    , m_rotateHintVisible(false)
    
    , m_currentTool(0) // 默认为选择工具
{
    // 不在这里创建选择层，只在选择工具激活时创建
    // 暂时不连接选择变化信号，避免在初始化时触发
    // connect(this, &DrawingScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
}

void DrawingScene::setCurrentTool(int toolType)
{
    m_currentTool = toolType;
}

void DrawingScene::setModified(bool modified)
{
    if (m_isModified != modified) {
        m_isModified = modified;
        emit sceneModified(modified);
    }
}



void DrawingScene::setSnapManager(SnapManager *snapManager)
{
    m_snapManager = snapManager;
}

void DrawingScene::clearScene()
{
    // 先清除所有选择
    clearSelection();
    
    // QGraphicsScene会自动管理item的生命周期，只需要移除它们
    QList<QGraphicsItem*> items = this->items();
    foreach (QGraphicsItem *item, items) {
        if (item) {
            removeItem(item);
            // 不需要手动删除，scene会自动处理
        }
    }
    
    // 清理撤销栈（通过CommandManager）
    if (CommandManager::hasInstance()) {
        CommandManager::instance()->clear();
    }
    setModified(false);
}



// 变换撤销支持
void DrawingScene::beginTransform(TransformType type)
{
    // 如果已经有正在进行的变换，先结束它
    if (!m_transformOldStates.isEmpty()) {
        endTransform();
    }
    
    // 保存变换前的状态
    m_transformOldStates.clear();
    m_transformShapes.clear();
    m_currentTransformType = type;
    
    QList<DrawingShape*> selectedShapes;
    QList<QGraphicsItem*> selected = selectedItems();
    
    // 从选中的项目中提取DrawingShape对象
    for (QGraphicsItem *item : selected) {
        DrawingShape *shape = static_cast<DrawingShape*>(item);
        if (shape) {
            selectedShapes.append(shape);
        }
    }
    
    // 保存每个选中图形的初始状态和引用
    for (DrawingShape *shape : selectedShapes) {
        if (shape) {
            TransformState state;
            state.position = shape->pos();
            state.transform = shape->transform();
            state.rotation = shape->rotation();
            m_transformOldStates.append(state);
            m_transformShapes.append(shape);
        }
    }
    
    qDebug() << "beginTransform called. Type:" << type << "Shapes count:" << m_transformShapes.size();
}

void DrawingScene::endTransform()
{
    // 如果没有保存的初始状态，直接返回
    if (m_transformOldStates.isEmpty() || m_transformShapes.isEmpty()) {
        return;
    }
    
    // 确定变换类型
    SceneTransformCommand::TransformType commandType = SceneTransformCommand::Generic;
    switch (m_currentTransformType) {
        case Move: commandType = SceneTransformCommand::Move; break;
        case Scale: commandType = SceneTransformCommand::Scale; break;
        case Rotate: commandType = SceneTransformCommand::Rotate; break;
        case Skew: commandType = SceneTransformCommand::Skew; break;
        default: commandType = SceneTransformCommand::Generic; break;
    }
    
    // 创建变换命令，使用保存的图形引用而不是当前选择
    SceneTransformCommand *command = new SceneTransformCommand(this, m_transformShapes, m_transformOldStates, commandType);
    
    // 检查是否有实际的变化，如果有变化则推送到撤销栈
    bool hasChanged = command->hasChanged();
    qDebug() << "SceneTransformCommand hasChanged:" << hasChanged << "Shapes count:" << m_transformShapes.size();
    
    if (hasChanged) {
        if (CommandManager::hasInstance()) {
            CommandManager::instance()->pushCommand(command);
            qDebug() << "SceneTransformCommand pushed to command manager. Stack size:" << CommandManager::instance()->undoStack()->count();
        } else {
            qDebug() << "No CommandManager instance, deleting command";
            delete command;
        }
    } else {
        // 没有实际变化，删除命令
        qDebug() << "SceneTransformCommand deleted (no actual changes)";
        delete command;
    }
    
    // 清理临时状态
    m_transformOldStates.clear();
    m_transformShapes.clear();
}

void DrawingScene::endTransformWithStates(const QList<TransformState>& newStates)
{
    // 如果没有保存的初始状态，直接返回
    if (m_transformOldStates.isEmpty() || m_transformShapes.isEmpty()) {
        return;
    }
    
    // 确定变换类型
    SceneTransformCommand::TransformType commandType = SceneTransformCommand::Generic;
    switch (m_currentTransformType) {
        case Move: commandType = SceneTransformCommand::Move; break;
        case Scale: commandType = SceneTransformCommand::Scale; break;
        case Rotate: commandType = SceneTransformCommand::Rotate; break;
        case Skew: commandType = SceneTransformCommand::Skew; break;
        default: commandType = SceneTransformCommand::Generic; break;
    }
    
    // 创建变换命令，使用提供的新状态而不是当前图形状态
    SceneTransformCommand *command = new SceneTransformCommand(this, m_transformShapes, m_transformOldStates, newStates, commandType);
    
    // 直接推送到撤销栈，不检查hasChanged（因为我们明确提供了新状态）
    if (CommandManager::hasInstance()) {
        CommandManager::instance()->pushCommand(command);
        qDebug() << "SceneTransformCommand pushed with provided states. Stack size:" << CommandManager::instance()->undoStack()->count();
    } else {
        qDebug() << "No CommandManager instance, deleting command";
        delete command;
    }
    
    // 清理临时状态
    m_transformOldStates.clear();
    m_transformShapes.clear();
}

void DrawingScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // 检查是否点击了空白区域
    QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
    bool clickedOnEmpty = (item == nullptr);
    
    // 如果点击空白区域且有选择，清除所有选择
    if (clickedOnEmpty) {
        // qDebug() << "Clicked on empty area, clearing selection";
        clearSelection();
    }
    
    QGraphicsScene::mousePressEvent(event);
}

void DrawingScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
}

void DrawingScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
}

void DrawingScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // 获取双击位置的项目
    QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
    
    if (item) {
        // 双击图形，在节点编辑和选择工具之间切换
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            // 发出工具切换请求信号
            if (m_currentTool == static_cast<int>(ToolType::NodeEdit)) {
                emit toolSwitchRequested(static_cast<int>(ToolType::Select));
            } else {
                emit toolSwitchRequested(static_cast<int>(ToolType::NodeEdit));
            }
            return;
        }
    }
    
    // 其他情况传递给基类处理
    QGraphicsScene::mouseDoubleClickEvent(event);
}

void DrawingScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // 发出右键菜单请求信号，让MainWindow处理
    emit contextMenuRequested(event->scenePos());
}

void DrawingScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        QList<QGraphicsItem*> selected = selectedItems();
        if (!selected.isEmpty()) {
            // 创建删除命令列表，先不执行删除
            QList<RemoveItemCommand*> deleteCommands;
            
            foreach (QGraphicsItem *item, selected) {
                if (item && item->scene() == this) {
                    // 只处理仍在当前场景中的项目
                    deleteCommands.append(new RemoveItemCommand(this, item));
                }
            }
            
            // 只有在有有效项目时才清除选择并执行删除
            if (!deleteCommands.isEmpty()) {
                // 先清除选择（在删除命令执行前）
                clearSelection();
                // 强制触发selectionChanged信号，确保选择工具能清理无效引用
                emit selectionChanged();
                
                // 执行删除命令（CommandManager会自动调用redo）
                foreach (RemoveItemCommand *command, deleteCommands) {
                    if (CommandManager::hasInstance()) {
                        CommandManager::instance()->pushCommand(command);
                    } else {
                        delete command;
                    }
                }
                
                setModified(true);
                
                qDebug() << "Deleted" << deleteCommands.size() << "items from scene";
            }
        }
        event->accept();
    } else {
        QGraphicsScene::keyPressEvent(event);
    }
}



void DrawingScene::activateSelectionTool()
{
    qDebug() << "activateSelectionTool called";
    // 只在选择工具激活时连接选择变化信号
    if (!signalsBlocked()) {
        // 断开已存在的连接（如果有的话）
        disconnect(this, &DrawingScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
        
        // 连接QGraphicsScene的selectionChanged信号到我们的onSelectionChanged方法
        connect(this, &QGraphicsScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
        
        // 老的选择层系统已移除，不再需要更新
        // if (this->selectionLayer()) {
        //     this->selectionLayer()->updateSelectionBounds();
        // }
    }
}

void DrawingScene::deactivateSelectionTool()
{
    // 断开选择变化信号
    disconnect(this, &DrawingScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
    disconnect(this, &QGraphicsScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
}

void DrawingScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    PERF_MONITOR_SCOPE("SceneDrawBackground");
    
    // 记录绘制调用统计 - 每帧只记录一次
    PerformanceMonitor::instance().recordRenderStats(1, 0, 0);
    
    // 设置亮色背景
    painter->fillRect(rect, QColor(255, 255, 255)); // 白色背景
    
    // 绘制网格，但限制在场景矩形范围内
    if (m_gridVisible) {
        QRectF sceneRect = QGraphicsScene::sceneRect();
        QRectF limitedRect = rect.intersected(sceneRect);
        if (!limitedRect.isEmpty()) {
            drawGrid(painter, limitedRect);
        }
    }
    
    // Draw guide lines
    if (m_guidesEnabled && !m_guides.isEmpty()) {
        painter->setRenderHint(QPainter::Antialiasing, false);
        
        for (const Guide &guide : m_guides) {
            if (!guide.visible) continue;
            
            painter->setPen(QPen(guide.color, 1, Qt::SolidLine));
            
            if (guide.orientation == Qt::Vertical) {
                // 垂直参考线
                qreal lineX = guide.position;
                if (lineX >= rect.left() && lineX <= rect.right()) {
                    painter->drawLine(QPointF(lineX, rect.top()), QPointF(lineX, rect.bottom()));
                }
            } else {
                // 水平参考线
                qreal lineY = guide.position;
                if (lineY >= rect.top() && lineY <= rect.bottom()) {
                    painter->drawLine(QPointF(rect.left(), lineY), QPointF(rect.right(), lineY));
                }
            }
        }
        
        painter->setRenderHint(QPainter::Antialiasing, true);
    }
    
    // 吸附指示器移到drawForeground中绘制，确保在最上层
}

void DrawingScene::drawGrid(QPainter *painter, const QRectF &rect)
{
    painter->setPen(QPen(m_gridColor, 1, Qt::DotLine));
    
    // 网格以场景坐标(0,0)为原点，与标尺对齐
    // 计算网格起始位置（从0开始，对齐到网格大小）
    int startX = qFloor(rect.left() / m_gridSize) * m_gridSize;
    int startY = qFloor(rect.top() / m_gridSize) * m_gridSize;
    int endX = qCeil(rect.right() / m_gridSize) * m_gridSize;
    int endY = qCeil(rect.bottom() / m_gridSize) * m_gridSize;
    
    // 确保包含0,0点
    startX = qMin(startX, 0);
    startY = qMin(startY, 0);
    
    // 绘制垂直线
    for (int x = startX; x <= endX; x += m_gridSize) {
        // 加粗原点线
        if (x == 0) {
            painter->setPen(QPen(m_gridColor.darker(150), 1, Qt::SolidLine));
        } else {
            painter->setPen(QPen(m_gridColor, 1, Qt::DotLine));
        }
        painter->drawLine(QPointF(x, startY), QPointF(x, endY));
    }
    
    // 绘制水平线
    for (int y = startY; y <= endY; y += m_gridSize) {
        // 加粗原点线
        if (y == 0) {
            painter->setPen(QPen(m_gridColor.darker(150), 1, Qt::SolidLine));
        } else {
            painter->setPen(QPen(m_gridColor, 1, Qt::DotLine));
        }
        painter->drawLine(QPointF(startX, y), QPointF(endX, y));
    }
}

// 网格功能实现
void DrawingScene::setGridVisible(bool visible)
{
    if (m_gridVisible != visible) {
        m_gridVisible = visible;
        update(); // 触发重绘
        emit gridVisibilityChanged(visible);
    }
}

bool DrawingScene::isGridVisible() const
{
    return m_gridVisible;
}

void DrawingScene::setGridSize(int size)
{
    if (m_gridSize != size && size > 0) {
        m_gridSize = size;
        update(); // 触发重绘
    }
}

int DrawingScene::gridSize() const
{
    return m_gridSize;
}

void DrawingScene::setGridColor(const QColor &color)
{
    if (m_gridColor != color) {
        m_gridColor = color;
        update(); // 触发重绘
    }
}

QColor DrawingScene::gridColor() const
{
    return m_gridColor;
}

// 参考线方法实现（保留在Scene中）
void DrawingScene::addGuide(Qt::Orientation orientation, qreal position)
{
    m_guides.append(Guide(orientation, position));
    update();
}

void DrawingScene::removeGuide(Qt::Orientation orientation, qreal position)
{
    for (int i = 0; i < m_guides.size(); ++i) {
        if (m_guides[i].orientation == orientation && qAbs(m_guides[i].position - position) < 1.0) {
            m_guides.removeAt(i);
            update();
            break;
        }
    }
}

void DrawingScene::clearGuides()
{
    m_guides.clear();
    update();
}

void DrawingScene::setGuideVisible(Qt::Orientation orientation, qreal position, bool visible)
{
    for (Guide &guide : m_guides) {
        if (guide.orientation == orientation && qAbs(guide.position - position) < 1.0) {
            guide.visible = visible;
            update();
            break;
        }
    }
}

// 网格对齐方法实现（保留在Scene中）
void DrawingScene::setGridAlignmentEnabled(bool enabled)
{
    if (m_gridAlignmentEnabled != enabled) {
        m_gridAlignmentEnabled = enabled;
        // 注意：不再发射gridAlignmentChanged信号，该信号由SnapManager处理
    }
}

bool DrawingScene::isGridAlignmentEnabled() const
{
    return m_gridAlignmentEnabled;
}

void DrawingScene::onSelectionChanged()
{
    qDebug() << "DrawingScene::onSelectionChanged called, selected items count:" << selectedItems().count();
    // 发射selectionChanged信号
    emit selectionChanged();
}

void DrawingScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    PERF_MONITOR_SCOPE("SceneDrawForeground");
    
    // 吸附指示器已禁用
    // if (m_snapManager && m_snapManager->areSnapIndicatorsVisible()) {
    //     // 让SnapManager处理吸附指示器的绘制
    //     if (m_snapManager->hasActiveSnap()) {
    //         drawSnapIndicators(painter);
    //     }
    // }
    
    // Draw scale hint
    if (m_scaleHintVisible && m_lastScaleHint.showHint) {
        QColor hintColor = QColor(0, 150, 255, 200); // Semi-transparent blue
        painter->setPen(QPen(hintColor.darker(120), 1));
        painter->setBrush(QBrush(hintColor));
        
        // 绘制提示背景
        QFont font = painter->font();
        font.setPointSize(10);
        painter->setFont(font);
        
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(m_lastScaleHint.hintDescription);
        textRect.adjust(-4, -2, 4, 2); // 添加内边距
        textRect.moveTopLeft(m_lastScaleHint.hintPosition.toPoint());
        
        // 绘制背景框
        painter->drawRoundedRect(textRect, 3, 3);
        
        // 绘制文本
        painter->setPen(QPen(Qt::white, 1));
        painter->drawText(textRect, Qt::AlignCenter, m_lastScaleHint.hintDescription);
    }
    
    // Draw rotate hint
    if (m_rotateHintVisible && m_lastRotateHint.showHint) {
        QColor hintColor = QColor(255, 150, 0, 200); // Semi-transparent orange
        painter->setPen(QPen(hintColor.darker(120), 1));
        painter->setBrush(QBrush(hintColor));
        
        // 绘制提示背景
        QFont font = painter->font();
        font.setPointSize(10);
        painter->setFont(font);
        
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(m_lastRotateHint.hintDescription);
        textRect.adjust(-4, -2, 4, 2); // 添加内边距
        textRect.moveTopLeft(m_lastRotateHint.hintPosition.toPoint());
        
        // 绘制背景框
        painter->drawRoundedRect(textRect, 3, 3);
        
        // 绘制文本
        painter->setPen(QPen(Qt::white, 1));
        painter->drawText(textRect, Qt::AlignCenter, m_lastRotateHint.hintDescription);
    }
}

// 吸附指示器绘制方法（供SnapManager调用）
void DrawingScene::drawSnapIndicators(QPainter *painter)
{
    if (!m_snapManager || !m_snapManager->hasActiveSnap()) {
        return;
    }
    
    // 从SnapManager获取最后一个吸附结果
    ObjectSnapResult snapResult = m_snapManager->getLastSnapResult();
    
    if (!snapResult.snappedToObject || !snapResult.targetShape) {
        return;
    }
    
    // 检查targetShape是否仍然有效（仍在场景中）
    if (!snapResult.targetShape->scene()) {
        // 对象已被删除，清除吸附结果
        m_snapManager->clearSnapIndicators();
        return;
    }
    
    // 设置吸附指示器的样式
    painter->setRenderHint(QPainter::Antialiasing, true);
    
    // 绘制吸附点 - 使用更醒目的颜色
    QColor snapColor = QColor(255, 0, 100); // 亮红色
    painter->setPen(QPen(snapColor, 1, Qt::SolidLine));
    painter->setBrush(QBrush(snapColor));
    
    // 绘制吸附点（适中的大小）
    qreal snapPointSize = 5.0;
    painter->drawEllipse(snapResult.snappedPos, snapPointSize, snapPointSize);
    
    // 绘制吸附线（从吸附点到目标图形的相关位置）
    painter->setPen(QPen(snapColor.lighter(120), 1.5, Qt::DashLine)); // 适中的线条粗细
    painter->setBrush(Qt::NoBrush);
    
    // 获取目标图形的边界
    QRectF targetBounds = snapResult.targetShape->boundingRect();
    QRectF targetSceneBounds = snapResult.targetShape->mapRectToScene(targetBounds);
    QPointF targetCenter = targetSceneBounds.center();
    
    // 适中的延伸长度
    qreal extensionLength = 15.0; // 延伸长度
    
    // 根据吸附类型绘制不同的指示线
    switch (snapResult.snapType) {
        case SnapToLeft:
            {
                QPointF endPoint = QPointF(targetSceneBounds.left() - extensionLength, targetCenter.y());
                painter->drawLine(snapResult.snappedPos, endPoint);
                // 绘制一条到目标边缘的短线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetSceneBounds.left() - 5, targetCenter.y()), 
                                QPointF(targetSceneBounds.left(), targetCenter.y()));
            }
            break;
        case SnapToRight:
            {
                QPointF endPoint = QPointF(targetSceneBounds.right() + extensionLength, targetCenter.y());
                painter->drawLine(snapResult.snappedPos, endPoint);
                // 绘制一条到目标边缘的短线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetSceneBounds.right() + 5, targetCenter.y()), 
                                QPointF(targetSceneBounds.right(), targetCenter.y()));
            }
            break;
        case SnapToTop:
            {
                QPointF endPoint = QPointF(targetCenter.x(), targetSceneBounds.top() - extensionLength);
                painter->drawLine(snapResult.snappedPos, endPoint);
                // 绘制一条到目标边缘的短线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetCenter.x(), targetSceneBounds.top() - 5), 
                                QPointF(targetCenter.x(), targetSceneBounds.top()));
            }
            break;
        case SnapToBottom:
            {
                QPointF endPoint = QPointF(targetCenter.x(), targetSceneBounds.bottom() + extensionLength);
                painter->drawLine(snapResult.snappedPos, endPoint);
                // 绘制一条到目标边缘的短线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetCenter.x(), targetSceneBounds.bottom() + 5), 
                                QPointF(targetCenter.x(), targetSceneBounds.bottom()));
            }
            break;
        case SnapToCenterX:
            {
                // 水平中心线，两端延伸
                QPointF leftPoint = QPointF(targetSceneBounds.left() - extensionLength, targetCenter.y());
                QPointF rightPoint = QPointF(targetSceneBounds.right() + extensionLength, targetCenter.y());
                painter->drawLine(leftPoint, rightPoint);
                // 绘制中心点标记
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetCenter.x() - 5, targetCenter.y()), 
                                QPointF(targetCenter.x() + 5, targetCenter.y()));
            }
            break;
        case SnapToCenterY:
            {
                // 垂直中心线，两端延伸
                QPointF topPoint = QPointF(targetCenter.x(), targetSceneBounds.top() - extensionLength);
                QPointF bottomPoint = QPointF(targetCenter.x(), targetSceneBounds.bottom() + extensionLength);
                painter->drawLine(topPoint, bottomPoint);
                // 绘制中心点标记
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetCenter.x(), targetCenter.y() - 5), 
                                QPointF(targetCenter.x(), targetCenter.y() + 5));
            }
            break;
        case SnapToCenter:
            {
                // 中心点标记，水平和垂直线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetCenter.x() - 7, targetCenter.y()), 
                                QPointF(targetCenter.x() + 7, targetCenter.y()));
                painter->drawLine(QPointF(targetCenter.x(), targetCenter.y() - 7), 
                                QPointF(targetCenter.x(), targetCenter.y() + 7));
                // 绘制连接线
                painter->setPen(QPen(snapColor.lighter(120), 1.5, Qt::DashLine));
                painter->drawLine(snapResult.snappedPos, targetCenter);
            }
            break;
        case SnapToCorner:
            {
                // 找到最近的角点
                QPointF corners[] = {
                    targetSceneBounds.topLeft(),
                    targetSceneBounds.topRight(),
                    targetSceneBounds.bottomLeft(),
                    targetSceneBounds.bottomRight()
                };
                
                QPointF closestCorner = corners[0];
                qreal minDist = QLineF(snapResult.snappedPos, closestCorner).length();
                
                for (int i = 1; i < 4; ++i) {
                    qreal dist = QLineF(snapResult.snappedPos, corners[i]).length();
                    if (dist < minDist) {
                        minDist = dist;
                        closestCorner = corners[i];
                    }
                }
                
                // 延伸线通过角点
                QPointF direction = closestCorner - snapResult.snappedPos;
                qreal length = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
                if (length > 0) {
                    direction = direction / length * extensionLength;
                    QPointF endPoint = closestCorner + direction;
                    painter->drawLine(snapResult.snappedPos, endPoint);
                }
            }
            break;
    }
    
    // 绘制吸附描述文字
    if (!snapResult.snapDescription.isEmpty()) {
        painter->setPen(QPen(snapColor.darker(120), 1));
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);
        
        // 在吸附点稍偏位置显示文字
        QPointF textPos = snapResult.snappedPos + QPointF(12, -8);
        painter->drawText(textPos, snapResult.snapDescription);
    }
}

// Scale hint implementation
void DrawingScene::showScaleHint(const DrawingScene::ScaleHintResult &hintResult)
{
    m_lastScaleHint = hintResult;
    m_scaleHintVisible = hintResult.showHint;
    update(); // 触发重绘
}

void DrawingScene::clearScaleHint()
{
    m_scaleHintVisible = false;
    m_lastScaleHint = DrawingScene::ScaleHintResult();
    update();
}

DrawingScene::ScaleHintResult DrawingScene::calculateScaleHint(qreal sx, qreal sy, const QPointF &pos)
{
    ScaleHintResult result;
    
    // 检查是否有显著的缩放
    if (qAbs(sx - 1.0) > 0.01 || qAbs(sy - 1.0) > 0.01) {
        result.showHint = true;
        result.scaleX = sx;
        result.scaleY = sy;
        result.hintPosition = pos + QPointF(20, -20); // 提示位置偏移
        
        // 生成描述文本
        if (qAbs(sx - sy) < 0.01) {
            // 等比例缩放
            result.hintDescription = tr("缩放: %1%").arg(qRound(sx * 100));
        } else {
            // 非等比例缩放
            result.hintDescription = tr("缩放: X%1% Y%2%")
                .arg(qRound(sx * 100))
                .arg(qRound(sy * 100));
        }
        
        // 检查特殊比例
        if (qAbs(sx - 2.0) < 0.05 && qAbs(sy - 2.0) < 0.05) {
            result.hintDescription += tr(" (2x)");
        } else if (qAbs(sx - 0.5) < 0.05 && qAbs(sy - 0.5) < 0.05) {
            result.hintDescription += tr(" (1/2x)");
        } else if (qAbs(sx - 1.0) < 0.05 && qAbs(sy + 1.0) < 0.05) {
            result.hintDescription += tr(" (水平翻转)");
        } else if (qAbs(sx + 1.0) < 0.05 && qAbs(sy - 1.0) < 0.05) {
            result.hintDescription += tr(" (垂直翻转)");
        }
    }
    
    return result;
}

// Rotate hint implementation
void DrawingScene::showRotateHint(const DrawingScene::RotateHintResult &hintResult)
{
    m_lastRotateHint = hintResult;
    m_rotateHintVisible = hintResult.showHint;
    update(); // 触发重绘
}

void DrawingScene::clearRotateHint()
{
    m_rotateHintVisible = false;
    m_lastRotateHint = DrawingScene::RotateHintResult();
    update();
}

DrawingScene::RotateHintResult DrawingScene::calculateRotateHint(qreal angle, const QPointF &pos)
{
    RotateHintResult result;
    
    // 检查是否有显著的旋转
    if (qAbs(angle) > 0.5) {
        result.showHint = true;
        result.angle = angle;
        result.hintPosition = pos + QPointF(20, -20); // 提示位置偏移
        
        // 生成描述文本
        result.hintDescription = tr("旋转: %1°").arg(qRound(angle));
        
        // 检查特殊角度
        if (qAbs(angle - 45.0) < 2.0) {
            result.hintDescription += " (45°)";
        } else if (qAbs(angle - 90.0) < 2.0) {
            result.hintDescription += " (90°)";
        } else if (qAbs(angle - 180.0) < 2.0) {
            result.hintDescription += " (180°)";
        } else if (qAbs(angle + 45.0) < 2.0) {
            result.hintDescription += " (-45°)";
        } else if (qAbs(angle + 90.0) < 2.0) {
            result.hintDescription += " (-90°)";
        } else if (qAbs(angle + 180.0) < 2.0) {
            result.hintDescription += " (-180°)";
        }
    }
    
    return result;
}

// 组合和取消组合操作的实现
void DrawingScene::groupSelectedItems()
{
    QList<QGraphicsItem *> selected = selectedItems();
    
    if (selected.size() < 2) {
        return; // 需要至少选择2个项目才能组合
    }
    
    // 收集要组合的形状
    QList<DrawingShape*> shapesToGroup;
    for (QGraphicsItem *item : selected) {
        if (item && item->parentItem() == nullptr) {  // 确保项目没有父项
            DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
            if (shape) {
                shapesToGroup.append(shape);
            }
        }
    }
    
    if (shapesToGroup.size() < 2) {
        return; // 没有足够的形状可以组合
    }
    
    // 创建并执行组合命令
    if (CommandManager::hasInstance()) {
        GroupCommand *command = new GroupCommand(CommandManager::instance(), shapesToGroup);
        CommandManager::instance()->pushCommand(command);
    }
}

void DrawingScene::ungroupSelectedItems()
{
    QList<QGraphicsItem *> selected = selectedItems();
    
    if (selected.isEmpty()) {
        return; // 没有选中的项目
    }
    
    // 查找选中的组合对象
    QList<DrawingGroup*> groupsToUngroup;
    for (QGraphicsItem *item : selected) {
        if (item && item->type() == QGraphicsItem::UserType + 1) {
            DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
            if (shape && shape->shapeType() == DrawingShape::Group) {
                DrawingGroup *group = dynamic_cast<DrawingGroup*>(item);
                groupsToUngroup.append(group);
            }
        }
    }
    
    // 为每个组合对象创建取消组合命令
    if (CommandManager::hasInstance()) {
        for (DrawingGroup *group : groupsToUngroup) {
            UngroupCommand *command = new UngroupCommand(CommandManager::instance(), {group});
            CommandManager::instance()->pushCommand(command);
        }
    }
}

// Z序控制操作的实现
void DrawingScene::bringToFront()
{
    QList<QGraphicsItem *> selected = selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    
    // 获取场景中所有项（不包括选中的项）
    QList<QGraphicsItem *> allItems = items();
    QList<QGraphicsItem *> otherItems;
    
    for (QGraphicsItem *item : allItems) {
        if (item && !selected.contains(item)) {
            otherItems.append(item);
        }
    }
    
    if (otherItems.isEmpty()) {
        return;
    }
    
    // 找到最大Z值
    qreal maxZ = -999999;
    for (QGraphicsItem *item : otherItems) {
        if (item && item->zValue() > maxZ) {
            maxZ = item->zValue();
        }
    }
    
    // 将选中项设置为比最大Z值更大的值
    for (QGraphicsItem *item : selected) {
        if (item) {
            item->setZValue(maxZ + 1);
        }
    }
    
    setModified(true);
}

void DrawingScene::sendToBack()
{
    QList<QGraphicsItem *> selected = selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    
    // 获取场景中所有项（不包括选中的项）
    QList<QGraphicsItem *> allItems = items();
    QList<QGraphicsItem *> otherItems;
    
    for (QGraphicsItem *item : allItems) {
        if (item && !selected.contains(item)) {
            otherItems.append(item);
        }
    }
    
    if (otherItems.isEmpty()) {
        return;
    }
    
    // 找到最小Z值
    qreal minZ = 999999;
    for (QGraphicsItem *item : otherItems) {
        if (item && item->zValue() < minZ) {
            minZ = item->zValue();
        }
    }
    
    // 将选中项设置为比最小Z值更小的值
    for (QGraphicsItem *item : selected) {
        if (item) {
            item->setZValue(minZ - 1);
        }
    }
    
    setModified(true);
}

void DrawingScene::bringForward()
{
    QList<QGraphicsItem *> selected = selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    
    // 获取场景中所有项，按Z值排序
    QList<QGraphicsItem *> allItems = items();
    if (allItems.size() < 2) {
        return;
    }
    
    // 按Z值排序（从低到高）
    std::sort(allItems.begin(), allItems.end(), [](const QGraphicsItem *a, const QGraphicsItem *b) {
        return a->zValue() < b->zValue();
    });
    
    for (QGraphicsItem *selectedItem : selected) {
        if (!selectedItem) continue;
        
        // 获取选中项的边界框
        QRectF selectedBounds = selectedItem->boundingRect().translated(selectedItem->pos());
        
        // 找到所有与选中项相交的对象
        QList<QGraphicsItem*> intersectingItems;
        for (QGraphicsItem *item : allItems) {
            if (item == selectedItem || selected.contains(item)) continue;
            
            QRectF itemBounds = item->boundingRect().translated(item->pos());
            if (selectedBounds.intersects(itemBounds)) {
                intersectingItems.append(item);
            }
        }
        
        // 如果有相交的对象，只在这些相交对象中移动
        if (!intersectingItems.isEmpty()) {
            // 在相交对象中找到当前选中项的位置
            int currentIndex = -1;
            for (int i = 0; i < intersectingItems.size(); ++i) {
                if (intersectingItems[i]->zValue() > selectedItem->zValue()) {
                    currentIndex = i;
                    break;
                }
            }
            
            // 如果找到更高Z值的相交对象，移动到最接近的那个的上方
            if (currentIndex >= 0) {
                QGraphicsItem *targetItem = intersectingItems[currentIndex];
                selectedItem->setZValue(targetItem->zValue() + 0.01);
            }
        } else {
            // 如果没有相交对象，在所有对象中找下一个更高的Z值
            for (int i = 0; i < allItems.size() - 1; ++i) {
                if (allItems[i] == selectedItem) {
                    QGraphicsItem *nextItem = allItems[i + 1];
                    if (!selected.contains(nextItem)) {
                        selectedItem->setZValue(nextItem->zValue() + 0.01);
                        break;
                    }
                }
            }
        }
    }
    
    setModified(true);
}

void DrawingScene::sendBackward()
{
    QList<QGraphicsItem *> selected = selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    
    // 获取场景中所有项，按Z值排序
    QList<QGraphicsItem *> allItems = items();
    if (allItems.size() < 2) {
        return;
    }
    
    // 按Z值排序（从低到高）
    std::sort(allItems.begin(), allItems.end(), [](const QGraphicsItem *a, const QGraphicsItem *b) {
        return a->zValue() < b->zValue();
    });
    
    for (QGraphicsItem *selectedItem : selected) {
        if (!selectedItem) continue;
        
        // 获取选中项的边界框
        QRectF selectedBounds = selectedItem->boundingRect().translated(selectedItem->pos());
        
        // 找到所有与选中项相交的对象
        QList<QGraphicsItem*> intersectingItems;
        for (QGraphicsItem *item : allItems) {
            if (item == selectedItem || selected.contains(item)) continue;
            
            QRectF itemBounds = item->boundingRect().translated(item->pos());
            if (selectedBounds.intersects(itemBounds)) {
                intersectingItems.append(item);
            }
        }
        
        // 如果有相交的对象，只在这些相交对象中移动
        if (!intersectingItems.isEmpty()) {
            // 在相交对象中找到当前选中项的位置
            int currentIndex = -1;
            for (int i = intersectingItems.size() - 1; i >= 0; --i) {
                if (intersectingItems[i]->zValue() < selectedItem->zValue()) {
                    currentIndex = i;
                    break;
                }
            }
            
            // 如果找到更低Z值的相交对象，移动到最接近的那个的下方
            if (currentIndex >= 0) {
                QGraphicsItem *targetItem = intersectingItems[currentIndex];
                selectedItem->setZValue(targetItem->zValue() - 0.01);
            }
        } else {
            // 如果没有相交对象，在所有对象中找下一个更低的Z值
            for (int i = 1; i < allItems.size(); ++i) {
                if (allItems[i] == selectedItem) {
                    QGraphicsItem *prevItem = allItems[i - 1];
                    if (!selected.contains(prevItem)) {
                        selectedItem->setZValue(prevItem->zValue() - 0.01);
                        break;
                    }
                }
            }
        }
    }
    
    setModified(true);
}