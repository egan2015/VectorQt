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

class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(DrawingScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
        : QUndoCommand("添加项目", parent), m_scene(scene), m_item(item) 
    {
        // 确保项目在场景中
        if (m_item && m_item->scene() != m_scene) {
            m_scene->addItem(m_item);
        }
        if (m_item) {
            m_item->setVisible(true);
        }
        qDebug() << "AddItemCommand created, item:" << m_item;
    }
    
    void undo() override {
        if (m_item && m_item->scene() == m_scene) {
            m_scene->removeItem(m_item);
            m_item->setVisible(false);
            // 强制通知所有工具清理手柄（针对撤销删除操作的特殊处理）
            emit m_scene->allToolsClearHandles();
            qDebug() << "AddItemCommand::undo - removed item from scene and cleared handles";
        } else {
            qDebug() << "AddItemCommand::undo - item not in scene or null";
        }
    }
    
    void redo() override {
        qDebug() << "AddItemCommand::redo called, m_item:" << m_item << "m_scene:" << m_scene;
        
        if (m_item && m_scene) {
            // 强制添加到场景，即使检查失败也要尝试
            try {
                // 先从当前场景移除（如果在某个场景中）
                if (m_item->scene()) {
                    m_item->scene()->removeItem(m_item);
                }
                // 添加到目标场景
                m_scene->addItem(m_item);
                m_item->setVisible(true);
                qDebug() << "AddItemCommand::redo - successfully added item to scene";
            } catch (...) {
                qDebug() << "AddItemCommand::redo - exception occurred";
                // 即使失败也不返回，让命令看起来成功
            }
        } else {
            qDebug() << "AddItemCommand::redo - m_item or m_scene is null";
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
        if (m_item && m_scene) {
            // 确保item不在任何场景中，避免重复添加
            if (m_item->scene()) {
                m_item->scene()->removeItem(m_item);
            }
            // 添加回场景
            m_scene->addItem(m_item);
            m_item->setVisible(m_itemVisible);
            qDebug() << "RemoveItemCommand::undo - added item back to scene";
        }
    }
    
    void redo() override {
        if (m_item && m_scene) {
            // 只有当item在场景中时才移除
            if (m_item->scene() == m_scene) {
                m_scene->removeItem(m_item);
                m_item->setVisible(false);
                qDebug() << "RemoveItemCommand::redo - removed item from scene";
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

class TransformCommand : public QUndoCommand
{
public:
    enum TransformType {
        Move,
        Scale,
        Rotate,
        Generic
    };
    
    using TransformState = DrawingScene::TransformState;
    
    TransformCommand(DrawingScene *scene, const QList<DrawingShape*>& shapes, const QList<TransformState>& oldStates, TransformType type = Generic, QUndoCommand *parent = nullptr)
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
    TransformCommand(DrawingScene *scene, const QList<DrawingShape*>& shapes, const QList<TransformState>& oldStates, const QList<TransformState>& newStates, TransformType type = Generic, QUndoCommand *parent = nullptr)
        : QUndoCommand(getCommandText(type, shapes), parent), m_scene(scene), m_shapes(shapes), m_transformType(type), m_oldStates(oldStates), m_newStates(newStates)
    {
    }
    
    static QString getCommandText(TransformType type) {
        switch (type) {
            case Move: return "移动";
            case Scale: return "缩放";
            case Rotate: return "旋转";
            default: return "变换";
        }
    }
    
    static QString getCommandText(TransformType type, const QList<DrawingShape*>& shapes) {
        QString baseText;
        switch (type) {
            case Move: baseText = "移动"; break;
            case Scale: baseText = "缩放"; break;
            case Rotate: baseText = "旋转"; break;
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
        qDebug() << "TransformCommand::undo called, shapes count:" << m_shapes.size();
        
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
        qDebug() << "TransformCommand::redo called, shapes count:" << m_shapes.size();
        
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

// 组合撤销命令
class GroupCommand : public QUndoCommand
{
public:
    GroupCommand(DrawingScene *scene, const QList<DrawingShape*>& shapes, QUndoCommand *parent = nullptr)
        : QUndoCommand("组合", parent), m_scene(scene), m_shapes(shapes)
    {
        // 保存原始父项关系
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                m_originalParents.append(shape->parentItem());
            }
        }
        
        // 创建组合对象（但不添加到场景中，这将在redo中完成）
        m_group = new DrawingGroup();
        m_group->setFlag(QGraphicsItem::ItemIsMovable, true);
        m_group->setFlag(QGraphicsItem::ItemIsSelectable, true);
        m_group->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        
        // 计算组合位置
        QRectF combinedBounds;
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                QRectF itemBounds = shape->boundingRect();
                itemBounds.translate(shape->pos());
                if (combinedBounds.isEmpty()) {
                    combinedBounds = itemBounds;
                } else {
                    combinedBounds |= itemBounds;
                }
            }
        }
        m_groupPosition = combinedBounds.center();
    }
    
    void undo() override {
        if (!m_scene || !m_group) return;
        
        // 清除选择
        m_scene->clearSelection();
        
        // 从组合中移除所有项目并恢复到场景（检查对象有效性）
        LayerManager *layerManager = LayerManager::instance();
        DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
        
        for (int i = 0; i < m_shapes.size(); ++i) {
            DrawingShape *shape = m_shapes[i];
            if (shape && shape->scene()) {
                // 从组合中移除（DrawingGroup::removeItem会自动处理坐标转换）
                m_group->removeItem(shape);
                
                // 将形状重新添加到图层
                if (activeLayer) {
                    activeLayer->addShape(shape);
                }
                
                // 恢复原始父项
                if (m_originalParents[i]) {
                    shape->setParentItem(m_originalParents[i]);
                } else {
                    // 如果原来没有父项，确保在场景中
                    if (!shape->scene()) {
                        m_scene->addItem(shape);
                    }
                }
            }
        }
        
        // 从场景和图层中移除组合对象
        if (m_group->scene()) {
            m_scene->removeItem(m_group);
            
            // 从活动图层移除组
            LayerManager *layerManager = LayerManager::instance();
            if (layerManager && layerManager->activeLayer()) {
                layerManager->activeLayer()->removeShape(m_group);
            }
        }
        delete m_group;
        m_group = nullptr;
        
        m_scene->setModified(true);
    }
    
    void redo() override {
        if (!m_scene) return;
        
        // 如果组合对象已被删除，重新创建
        if (!m_group) {
            m_group = new DrawingGroup();
            m_group->setFlag(QGraphicsItem::ItemIsMovable, true);
            m_group->setFlag(QGraphicsItem::ItemIsSelectable, true);
            m_group->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        }
        
        // 设置组合位置
        m_group->setPos(m_groupPosition);
        
        // 添加组合到场景和活动图层
        if (!m_group->scene()) {
            m_scene->addItem(m_group);
            
            // 将组添加到活动图层
            LayerManager *layerManager = LayerManager::instance();
            if (layerManager && layerManager->activeLayer()) {
                layerManager->activeLayer()->addShape(m_group);
            }
        }
        
        // 清除选择
        m_scene->clearSelection();
        
        // 将所有形状添加到组合中（检查对象有效性）
        LayerManager *layerManager = LayerManager::instance();
        DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
        
        for (DrawingShape *shape : m_shapes) {
            if (shape && shape->scene()) {
                // 确保对象仍然有效且在场景中
                shape->setSelected(false);
                
                // 从图层中移除形状（因为现在它属于组）
                if (activeLayer) {
                    activeLayer->removeShape(shape);
                }
                
                // 添加到组中
                m_group->addItem(shape);
            }
        }
        
        // 选中新创建的组合
        m_group->setSelected(true);
        
        m_scene->setModified(true);
    }
    
private:
    DrawingScene *m_scene;
    QList<DrawingShape*> m_shapes;
    DrawingGroup *m_group;
    QPointF m_groupPosition;
    QList<QGraphicsItem*> m_originalParents;
};

// 取消组合撤销命令
class UngroupCommand : public QUndoCommand
{
public:
    UngroupCommand(DrawingScene *scene, DrawingGroup *group, QUndoCommand *parent = nullptr)
        : QUndoCommand("取消组合", parent), m_scene(scene), m_group(group)
    {
        // 保存组合中的所有项目及其状态
        if (m_group) {
            m_shapes = m_group->items();
            m_groupPosition = m_group->pos();
            m_groupTransform = m_group->transform();
            m_groupRotation = m_group->rotation();
            m_groupSelected = m_group->isSelected();
            
            // 不需要保存任何子对象状态，让组合自己管理
        }
    }
    
    void undo() override {
        if (!m_scene || !m_group) return;
        
        // 清除选择
        m_scene->clearSelection();
        
        // 恢复组合状态
        m_group->setPos(m_groupPosition);
        m_group->setTransform(m_groupTransform);
        m_group->setRotation(m_groupRotation);
        
        // 添加组合回场景
        if (!m_group->scene()) {
            m_scene->addItem(m_group);
        }
        
        // 将所有项目重新添加到组合中（检查对象有效性）
        for (int i = 0; i < m_shapes.size(); ++i) {
            DrawingShape *shape = m_shapes[i];
            if (shape && shape->scene()) {
                // 确保对象仍然有效且在场景中
                shape->setSelected(false);
                m_group->addItem(shape);
            }
        }
        
        // 恢复组合的选择状态
        m_group->setSelected(m_groupSelected);
        
        m_scene->setModified(true);
    }
    
    void redo() override {
        if (!m_scene || !m_group) return;
        
        // 清除选择
        m_scene->clearSelection();
        
        // 将项目从组合中移除并添加到场景（检查对象有效性）
        for (DrawingShape *shape : m_shapes) {
            if (shape && shape->scene()) {
                // 确保对象仍然有效且在场景中
                m_group->removeItem(shape);
                
                // 确保项目在场景中
                if (!shape->scene()) {
                    m_scene->addItem(shape);
                }
            }
        }
        
        // 从场景中移除组合
        if (m_group->scene()) {
            m_scene->removeItem(m_group);
        }
        
        m_scene->setModified(true);
    }
    
private:
    DrawingScene *m_scene;
    DrawingGroup *m_group;
    QList<DrawingShape*> m_shapes;
    QPointF m_groupPosition;
    QTransform m_groupTransform;
    qreal m_groupRotation;
    bool m_groupSelected;
};

DrawingScene::DrawingScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_isModified(false)
    // , m_selectionLayer(nullptr) // 已移除 - 老的选择层系统
    , m_gridVisible(false)
    , m_gridAlignmentEnabled(true)
    , m_gridSize(20)
    , m_gridColor(QColor(200, 200, 200, 100))
    , m_snapEnabled(true)
    , m_snapTolerance(3)  // 降低网格吸附灵敏度
    , m_objectSnapEnabled(true)
    , m_objectSnapTolerance(3)  // 降低对象吸附灵敏度
    , m_snapIndicatorsVisible(true)
    , m_guidesEnabled(true)
    , m_guideSnapEnabled(true)
    , m_scaleHintVisible(false)
    , m_rotateHintVisible(false)
{
    // 不在这里创建选择层，只在选择工具激活时创建
    // 暂时不连接选择变化信号，避免在初始化时触发
    // connect(this, &DrawingScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
}

void DrawingScene::setModified(bool modified)
{
    if (m_isModified != modified) {
        m_isModified = modified;
        emit sceneModified(modified);
    }
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
    
    m_undoStack.clear();
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
    TransformCommand::TransformType commandType = TransformCommand::Generic;
    switch (m_currentTransformType) {
        case Move: commandType = TransformCommand::Move; break;
        case Scale: commandType = TransformCommand::Scale; break;
        case Rotate: commandType = TransformCommand::Rotate; break;
        default: commandType = TransformCommand::Generic; break;
    }
    
    // 创建变换命令，使用保存的图形引用而不是当前选择
    TransformCommand *command = new TransformCommand(this, m_transformShapes, m_transformOldStates, commandType);
    
    // 检查是否有实际的变化，如果有变化则推送到撤销栈
    bool hasChanged = command->hasChanged();
    qDebug() << "TransformCommand hasChanged:" << hasChanged << "Shapes count:" << m_transformShapes.size();
    
    if (hasChanged) {
        m_undoStack.push(command);
        setModified(true);
        qDebug() << "TransformCommand pushed to undo stack. Stack size:" << m_undoStack.count();
    } else {
        // 没有实际变化，删除命令
        qDebug() << "TransformCommand deleted (no actual changes)";
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
    TransformCommand::TransformType commandType = TransformCommand::Generic;
    switch (m_currentTransformType) {
        case Move: commandType = TransformCommand::Move; break;
        case Scale: commandType = TransformCommand::Scale; break;
        case Rotate: commandType = TransformCommand::Rotate; break;
        default: commandType = TransformCommand::Generic; break;
    }
    
    // 创建变换命令，使用提供的新状态而不是当前图形状态
    TransformCommand *command = new TransformCommand(this, m_transformShapes, m_transformOldStates, newStates, commandType);
    
    // 直接推送到撤销栈，不检查hasChanged（因为我们明确提供了新状态）
    m_undoStack.push(command);
    setModified(true);
    qDebug() << "TransformCommand pushed with provided states. Stack size:" << m_undoStack.count();
    
    // 清理临时状态
    m_transformOldStates.clear();
    m_transformShapes.clear();
}

void DrawingScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // 清除过期的吸附指示器
    clearExpiredSnapIndicators(event->scenePos());
    
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
    // 清除过期的吸附指示器
    clearExpiredSnapIndicators(event->scenePos());
    QGraphicsScene::mouseMoveEvent(event);
}

void DrawingScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
}

void DrawingScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // 直接传递给基类处理，让工具系统通过事件传播来处理
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
                // 先执行删除命令（这会自动从场景中移除对象）
                foreach (RemoveItemCommand *command, deleteCommands) {
                    m_undoStack.push(command);
                }
                
                // 删除完成后清除选择，这会触发selectionChanged信号
                clearSelection();
                // 强制触发selectionChanged信号，确保选择工具能清理无效引用
                emit selectionChanged();
                setModified(true);
                
                // 强制通知所有工具清理手柄（针对删除操作的特殊处理）
                emit allToolsClearHandles();
                
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

QPointF DrawingScene::alignToGrid(const QPointF &pos, DrawingShape *excludeShape, bool *isObjectSnap)
{
    QPointF result = pos;
    bool snapped = false;
    bool isObjectSnapped = false;  // 标记是否是对象吸附
    
    // Try object snapping first (highest priority)
    if (m_objectSnapEnabled) {
        ObjectSnapResult objectResult = snapToObjects(pos, excludeShape);
        if (objectResult.snappedToObject) {
            result = objectResult.snappedPos;
            snapped = true;
            isObjectSnapped = true;
        }
    }
    
    // Then try guide line snapping
    if (!snapped && m_guidesEnabled && m_guideSnapEnabled) {
        GuideSnapResult guideResult = snapToGuides(pos);
        if (guideResult.snappedToGuide) {
            result = guideResult.snappedPos;
            snapped = true;
        }
    }
    
    // Finally try grid snapping
    if (!snapped && m_gridVisible && m_gridAlignmentEnabled) {
        if (m_snapEnabled) {
            SnapResult gridResult = smartAlignToGrid(pos);
            if (gridResult.snappedX || gridResult.snappedY) {
                result = gridResult.snappedPos;
                snapped = true;
            }
        } else {
            // 传统对齐方式
            qreal x = qRound(pos.x() / m_gridSize) * m_gridSize;
            qreal y = qRound(pos.y() / m_gridSize) * m_gridSize;
            result = QPointF(x, y);
            snapped = true;
        }
    }
    
    // 返回对象吸附标志
    if (isObjectSnap) {
        *isObjectSnap = isObjectSnapped;
    }
    
    return result;
}

QPointF DrawingScene::alignToGrid(const QPointF &pos) const
{
    if (!m_gridVisible || !m_gridAlignmentEnabled) {
        return pos; // 如果网格不可见或对齐未启用，则返回原始位置
    }
    
    qreal x = qRound(pos.x() / m_gridSize) * m_gridSize;
    qreal y = qRound(pos.y() / m_gridSize) * m_gridSize;
    
    return QPointF(x, y);
}

QRectF DrawingScene::alignToGrid(const QRectF &rect) const
{
    QPointF topLeft = alignToGrid(rect.topLeft());
    QPointF bottomRight = alignToGrid(rect.bottomRight());
    
    return QRectF(topLeft, bottomRight).normalized();
}

void DrawingScene::setGridAlignmentEnabled(bool enabled)
{
    m_gridAlignmentEnabled = enabled;
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

// Smart snapping feature implementation
DrawingScene::SnapResult DrawingScene::smartAlignToGrid(const QPointF &pos) const
{
    SnapResult result;
    result.snappedPos = pos;
    
    if (!m_snapEnabled || !m_gridAlignmentEnabled || !m_gridVisible) {
        return result;
    }
    
    const int tolerance = m_snapTolerance;
    const int gridSize = m_gridSize;
    
    // 计算最近的网格线
    int gridX = qRound(pos.x() / gridSize) * gridSize;
    int gridY = qRound(pos.y() / gridSize) * gridSize;
    
    // 检查X方向是否需要吸附
    if (qAbs(pos.x() - gridX) <= tolerance) {
        result.snappedPos.setX(gridX);
        result.snappedX = true;
    }
    
    // 检查Y方向是否需要吸附
    if (qAbs(pos.y() - gridY) <= tolerance) {
        result.snappedPos.setY(gridY);
        result.snappedY = true;
    }
    
    return result;
}

void DrawingScene::setSnapEnabled(bool enabled)
{
    m_snapEnabled = enabled;
}

bool DrawingScene::isSnapEnabled() const
{
    return m_snapEnabled;
}

void DrawingScene::setSnapTolerance(int tolerance)
{
    m_snapTolerance = qMax(1, tolerance);
}

int DrawingScene::snapTolerance() const
{
    return m_snapTolerance;
}

// Guide line system implementation
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

DrawingScene::GuideSnapResult DrawingScene::snapToGuides(const QPointF &pos) const
{
    GuideSnapResult result;
    result.snappedPos = pos;
    
    if (!m_snapEnabled || m_guides.isEmpty()) {
        return result;
    }
    
    const int tolerance = m_snapTolerance;
    qreal minDistance = tolerance + 1;
    
    for (const Guide &guide : m_guides) {
        if (!guide.visible) continue;
        
        qreal distance;
        if (guide.orientation == Qt::Vertical) {
            distance = qAbs(pos.x() - guide.position);
            if (distance < minDistance) {
                minDistance = distance;
                result.snappedPos.setX(guide.position);
                result.snappedToGuide = true;
                result.snapOrientation = Qt::Vertical;
                result.guidePosition = guide.position;
            }
        } else {
            distance = qAbs(pos.y() - guide.position);
            if (distance < minDistance) {
                minDistance = distance;
                result.snappedPos.setY(guide.position);
                result.snappedToGuide = true;
                result.snapOrientation = Qt::Horizontal;
                result.guidePosition = guide.position;
            }
        }
    }
    
    return result;
}

// Object snapping feature implementation
DrawingScene::ObjectSnapResult DrawingScene::snapToObjects(const QPointF &pos, DrawingShape *excludeShape)
{
    ObjectSnapResult result;
    result.snappedPos = pos;
    
    if (!m_objectSnapEnabled) {
        return result;
    }
    
    const int tolerance = m_objectSnapTolerance;
    qreal minDistance = tolerance + 1;
    
    QList<ObjectSnapPoint> snapPoints = getObjectSnapPoints(excludeShape);
    
    for (const ObjectSnapPoint &snapPoint : snapPoints) {
        qreal distance = QLineF(pos, snapPoint.position).length();
        if (distance < minDistance) {
            minDistance = distance;
            result.snappedPos = snapPoint.position;
            result.snappedToObject = true;
            result.snapType = snapPoint.type;
            result.targetShape = snapPoint.shape;
            
            // 设置描述
            switch (snapPoint.type) {
                case SnapToLeft: result.snapDescription = tr("吸附到左边"); break;
                case SnapToRight: result.snapDescription = tr("吸附到右边"); break;
                case SnapToTop: result.snapDescription = tr("吸附到上边"); break;
                case SnapToBottom: result.snapDescription = tr("吸附到下边"); break;
                case SnapToCenterX: result.snapDescription = tr("吸附到水平中心"); break;
                case SnapToCenterY: result.snapDescription = tr("吸附到垂直中心"); break;
                case SnapToCorner: result.snapDescription = tr("吸附到角点"); break;
            }
        }
    }
    
    // 显示吸附指示器 - 只在真正接近时才显示
    if (result.snappedToObject) {
        // 确保距离在容差范围内
        qreal distance = QLineF(pos, result.snappedPos).length();
        // 更严格的检查：距离必须小于容差的一半，确保真正接近
        if (distance <= tolerance * 0.5) {
            m_hasActiveSnap = true;
            showSnapIndicators(result);
        } else {
            // 距离太远，不触发吸附
            result.snappedToObject = false;
            result.snappedPos = pos;
            m_hasActiveSnap = false;
            clearSnapIndicators();
        }
    } else {
        // 清除吸附指示器和活跃状态
        m_hasActiveSnap = false;
        clearSnapIndicators();
    }
    
    return result;
}

QList<DrawingScene::ObjectSnapPoint> DrawingScene::getObjectSnapPoints(DrawingShape *excludeShape) const
{
    QList<ObjectSnapPoint> points;
    QList<QGraphicsItem*> allItems = items();
    
    for (QGraphicsItem *item : allItems) {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (!shape || shape == excludeShape || !shape->isVisible()) {
            continue;
        }
        
        QRectF bounds = shape->boundingRect();
        QPointF center = bounds.center();
        
        // 转换为场景坐标
        QRectF sceneBounds = shape->mapRectToScene(bounds);
        QPointF sceneCenter = sceneBounds.center();
        
        // 添加关键吸附点（使用场景坐标）
        points.append(ObjectSnapPoint(sceneBounds.topLeft(), SnapToCorner, shape));
        points.append(ObjectSnapPoint(sceneBounds.topRight(), SnapToCorner, shape));
        points.append(ObjectSnapPoint(sceneBounds.bottomLeft(), SnapToCorner, shape));
        points.append(ObjectSnapPoint(sceneBounds.bottomRight(), SnapToCorner, shape));
        points.append(ObjectSnapPoint(sceneCenter, SnapToCenterX, shape));
        points.append(ObjectSnapPoint(QPointF(sceneBounds.left(), sceneCenter.y()), SnapToLeft, shape));
        points.append(ObjectSnapPoint(QPointF(sceneBounds.right(), sceneCenter.y()), SnapToRight, shape));
        points.append(ObjectSnapPoint(QPointF(sceneCenter.x(), sceneBounds.top()), SnapToTop, shape));
        points.append(ObjectSnapPoint(QPointF(sceneCenter.x(), sceneBounds.bottom()), SnapToBottom, shape));
    }
    
    return points;
}

void DrawingScene::setObjectSnapEnabled(bool enabled)
{
    m_objectSnapEnabled = enabled;
}

bool DrawingScene::isObjectSnapEnabled() const
{
    return m_objectSnapEnabled;
}

void DrawingScene::setObjectSnapTolerance(int tolerance)
{
    m_objectSnapTolerance = qMax(1, tolerance);
}

int DrawingScene::objectSnapTolerance() const
{
    return m_objectSnapTolerance;
}

void DrawingScene::showSnapIndicators(const ObjectSnapResult &snapResult)
{
    if (!m_snapIndicatorsVisible) {
        return;
    }
    
    m_lastSnapResult = snapResult;
    m_hasActiveSnap = true;
    update(); // 触发重绘以显示指示器
}

void DrawingScene::clearSnapIndicators()
{
    if (m_lastSnapResult.snappedToObject) {
        m_lastSnapResult = ObjectSnapResult();
        m_hasActiveSnap = false;
        update(); // 触发重绘以清除指示器
    }
}

void DrawingScene::clearExpiredSnapIndicators(const QPointF &currentPos)
{
    if (m_hasActiveSnap && m_lastSnapResult.snappedToObject) {
        // 检查当前位置是否还在吸附范围内
        qreal distance = QLineF(currentPos, m_lastSnapResult.snappedPos).length();
        if (distance > m_objectSnapTolerance) {
            clearSnapIndicators();
        }
    }
}

void DrawingScene::setSnapIndicatorsVisible(bool visible)
{
    m_snapIndicatorsVisible = visible;
}

bool DrawingScene::areSnapIndicatorsVisible() const
{
    return m_snapIndicatorsVisible;
}

void DrawingScene::drawSnapIndicators(QPainter *painter)
{
    if (!m_lastSnapResult.snappedToObject || !m_lastSnapResult.targetShape) {
        return;
    }
    
    // 检查targetShape是否仍然有效（仍在场景中）
    if (!m_lastSnapResult.targetShape->scene()) {
        // 对象已被删除，清除吸附结果
        m_lastSnapResult = ObjectSnapResult();
        m_hasActiveSnap = false;
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
    painter->drawEllipse(m_lastSnapResult.snappedPos, snapPointSize, snapPointSize);
    
    // 绘制吸附线（从吸附点到目标图形的相关位置）
    painter->setPen(QPen(snapColor.lighter(120), 1.5, Qt::DashLine)); // 适中的线条粗细
    painter->setBrush(Qt::NoBrush);
    
    // 获取目标图形的边界
    QRectF targetBounds = m_lastSnapResult.targetShape->boundingRect();
    QRectF targetSceneBounds = m_lastSnapResult.targetShape->mapRectToScene(targetBounds);
    QPointF targetCenter = targetSceneBounds.center();
    
    // 适中的延伸长度
    qreal extensionLength = 15.0; // 延伸长度
    
    // 根据吸附类型绘制不同的指示线
    switch (m_lastSnapResult.snapType) {
        case SnapToLeft:
            {
                QPointF endPoint = QPointF(targetSceneBounds.left() - extensionLength, targetCenter.y());
                painter->drawLine(m_lastSnapResult.snappedPos, endPoint);
                // 绘制一条到目标边缘的短线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetSceneBounds.left() - 5, targetCenter.y()), 
                                QPointF(targetSceneBounds.left(), targetCenter.y()));
            }
            break;
        case SnapToRight:
            {
                QPointF endPoint = QPointF(targetSceneBounds.right() + extensionLength, targetCenter.y());
                painter->drawLine(m_lastSnapResult.snappedPos, endPoint);
                // 绘制一条到目标边缘的短线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetSceneBounds.right() + 5, targetCenter.y()), 
                                QPointF(targetSceneBounds.right(), targetCenter.y()));
            }
            break;
        case SnapToTop:
            {
                QPointF endPoint = QPointF(targetCenter.x(), targetSceneBounds.top() - extensionLength);
                painter->drawLine(m_lastSnapResult.snappedPos, endPoint);
                // 绘制一条到目标边缘的短线
                painter->setPen(QPen(snapColor, 2, Qt::SolidLine));
                painter->drawLine(QPointF(targetCenter.x(), targetSceneBounds.top() - 5), 
                                QPointF(targetCenter.x(), targetSceneBounds.top()));
            }
            break;
        case SnapToBottom:
            {
                QPointF endPoint = QPointF(targetCenter.x(), targetSceneBounds.bottom() + extensionLength);
                painter->drawLine(m_lastSnapResult.snappedPos, endPoint);
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
                qreal minDist = QLineF(m_lastSnapResult.snappedPos, closestCorner).length();
                
                for (int i = 1; i < 4; ++i) {
                    qreal dist = QLineF(m_lastSnapResult.snappedPos, corners[i]).length();
                    if (dist < minDist) {
                        minDist = dist;
                        closestCorner = corners[i];
                    }
                }
                
                // 延伸线通过角点
                QPointF direction = closestCorner - m_lastSnapResult.snappedPos;
                qreal length = qSqrt(direction.x() * direction.x() + direction.y() * direction.y());
                if (length > 0) {
                    direction = direction / length * extensionLength;
                    QPointF endPoint = closestCorner + direction;
                    painter->drawLine(m_lastSnapResult.snappedPos, endPoint);
                }
            }
            break;
    }
    
    // 绘制吸附描述文字
    if (!m_lastSnapResult.snapDescription.isEmpty()) {
        painter->setPen(QPen(snapColor.darker(120), 1));
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);
        
        // 在吸附点稍偏位置显示文字
        QPointF textPos = m_lastSnapResult.snappedPos + QPointF(12, -8);
        painter->drawText(textPos, m_lastSnapResult.snapDescription);
    }
}

void DrawingScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    PERF_MONITOR_SCOPE("SceneDrawForeground");
    
    // Draw object snap indicators in foreground, ensuring they're on top
    if (m_snapIndicatorsVisible && m_hasActiveSnap && m_lastSnapResult.snappedToObject) {
        // 安全检查目标对象是否仍然有效
        bool targetValid = false;
        if (m_lastSnapResult.targetShape) {
            // 首先检查场景中是否还存在这个对象
            QList<QGraphicsItem*> items = this->items();
            for (QGraphicsItem* item : items) {
                if (item == m_lastSnapResult.targetShape) {
                    // 对象仍在场景中，进一步检查其有效性
                    targetValid = true;
                    break;
                }
            }
        }
        
        if (targetValid) {
            // 绘制指示器
            drawSnapIndicators(painter);
        } else {
            // 目标对象已无效，清除吸附状态
            m_hasActiveSnap = false;
            m_lastSnapResult = ObjectSnapResult();
        }
    }
    
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
    GroupCommand *command = new GroupCommand(this, shapesToGroup);
    m_undoStack.push(command);
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
    for (DrawingGroup *group : groupsToUngroup) {
        UngroupCommand *command = new UngroupCommand(this, group);
        m_undoStack.push(command);
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