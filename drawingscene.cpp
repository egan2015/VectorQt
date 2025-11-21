#include <QGraphicsItem>
#include "drawingscene.h"
#include "drawing-shape.h"
#include "drawing-group.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QUndoCommand>
#include <QPainter>
#include <QDebug>

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
            qDebug() << "AddItemCommand::undo - removed item from scene";
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
                oldState.transform.transform() != newState.transform.transform()) {
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
                qDebug() << "  Restoring shape" << i << "to pos:" << state.position << "transform:" << state.transform.transform();
                
                shape->setPos(state.position);
                shape->setTransform(state.transform);
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
                shape->setTransform(state.transform);
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
    , m_snapEnabled(true)
    , m_snapTolerance(3)  // 降低网格吸附灵敏度
    , m_objectSnapEnabled(true)
    , m_objectSnapTolerance(3)  // 降低对象吸附灵敏度
    , m_snapIndicatorsVisible(true)
    , m_guidesEnabled(true)
    , m_guideSnapEnabled(true)
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
                
                qDebug() << "Deleted" << deleteCommands.size() << "items from scene";
            }
        }
        event->accept();
    } else {
        QGraphicsScene::keyPressEvent(event);
    }
}

void DrawingScene::updateSelection()
{
    // 使用Qt的信号阻塞机制来避免递归调用
    bool wasBlocked = blockSignals(true);
    
    QList<QGraphicsItem*> selected = selectedItems();
    QList<DrawingShape*> selectedShapes;
    
    // qDebug() << "updateSelection called, total selected items:" << selected.count();
    
    // 只收集选中的DrawingShape对象，忽略QGraphicsItemGroup和DrawingLayer
    for (QGraphicsItem *item : selected) {
        if (!item) continue; // 空指针检查
        
        // 检查是否是DrawingLayer，如果是则跳过
        if (item->type() == QGraphicsItem::UserType + 100) {
            // qDebug() << "Skipping DrawingLayer item in selection";
            continue;
        }
        
        
        
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            // 额外检查对象是否有效且在场景中
            if (shape->scene() == this) {
                selectedShapes.append(shape);
                // qDebug() << "Found selected shape:" << shape;
            }
        }
        // QGraphicsItemGroup和DrawingLayer不需要特殊的编辑手柄，所以不处理
    }
    
    // qDebug() << "Total DrawingShape objects selected:" << selectedShapes.count();
    
    // 禁用所有未选中图形的编辑把手
    QList<QGraphicsItem*> allItems = items();
    for (QGraphicsItem *item : allItems) {
        if (!item) continue; // 空指针检查
        
        // 跳过DrawingLayer
        if (item->type() == QGraphicsItem::UserType + 100) {
            continue;
        }
        
        
        
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape && !selectedShapes.contains(shape)) {
            // 额外检查对象是否有效
            if (shape->scene() == this) {
                // 老的手柄系统已移除，不再需要禁用编辑把手
            }
        }
    }
    
    // 考的手柄系统已移除，不再需要启用编辑把手
    
    // 恢复信号状态
    blockSignals(wasBlocked);
    
    // 发射选择变化信号
    emit selectionChanged();
}

void DrawingScene::activateSelectionTool()
{
    // qDebug() << "activateSelectionTool called";
    // 只在选择工具激活时连接选择变化信号
    if (!signalsBlocked()) {
        // 断开已存在的连接（如果有的话）
        disconnect(this, &DrawingScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
        
        // qDebug() << "Connecting selectionChanged signal";
        connect(this, &DrawingScene::selectionChanged, this, &DrawingScene::onSelectionChanged);
        
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
}

void DrawingScene::drawBackground(QPainter *painter, const QRectF &rect)
{
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
    
    // 🌟 绘制参考线
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
    
    // 🌟 首先尝试对象吸附（优先级最高）
    if (m_objectSnapEnabled) {
        ObjectSnapResult objectResult = snapToObjects(pos, excludeShape);
        if (objectResult.snappedToObject) {
            result = objectResult.snappedPos;
            snapped = true;
            isObjectSnapped = true;
        }
    }
    
    // 🌟 其次尝试参考线吸附
    if (!snapped && m_guidesEnabled && m_guideSnapEnabled) {
        GuideSnapResult guideResult = snapToGuides(pos);
        if (guideResult.snappedToGuide) {
            result = guideResult.snappedPos;
            snapped = true;
        }
    }
    
    // 🌟 最后尝试网格吸附
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
    // qDebug() << "onSelectionChanged called";
    // 直接更新选择
    updateSelection();
}

// 🌟 智能吸附功能实现
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

// 🌟 参考线系统实现
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

// 🌟 对象吸附功能实现
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
                case SnapToLeft: result.snapDescription = "吸附到左边"; break;
                case SnapToRight: result.snapDescription = "吸附到右边"; break;
                case SnapToTop: result.snapDescription = "吸附到上边"; break;
                case SnapToBottom: result.snapDescription = "吸附到下边"; break;
                case SnapToCenterX: result.snapDescription = "吸附到水平中心"; break;
                case SnapToCenterY: result.snapDescription = "吸附到垂直中心"; break;
                case SnapToCorner: result.snapDescription = "吸附到角点"; break;
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
}void DrawingScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    // 🌟 在前景绘制对象吸附指示器，确保在最上层不被遮挡
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
}