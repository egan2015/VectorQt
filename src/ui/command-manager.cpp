#include <QGraphicsItem>
#include <QDataStream>
#include "command-manager.h"
#include "drawingscene.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-group.h"
#include "../core/layer-manager.h"
#include "../core/drawing-layer.h"

// 静态成员初始化
CommandManager* CommandManager::s_instance = nullptr;

CommandManager::CommandManager(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
    , m_undoStack(new QUndoStack(this))
{
    // qDebug() << "CommandManager created";
    
    connect(m_undoStack, &QUndoStack::cleanChanged, this, &CommandManager::undoStackChanged);
    connect(m_undoStack, &QUndoStack::canUndoChanged, this, [this](bool canUndo) { 
        // qDebug() << "CommandManager: canUndoChanged:" << canUndo; 
        emit canUndoChanged(canUndo); 
    });
    connect(m_undoStack, &QUndoStack::canRedoChanged, this, [this](bool canRedo) { 
        // qDebug() << "CommandManager: canRedoChanged:" << canRedo; 
        emit canRedoChanged(canRedo); 
    });
    connect(m_undoStack, &QUndoStack::undoTextChanged, this, &CommandManager::undoStackChanged);
}

CommandManager::~CommandManager()
{
    // qDebug() << "CommandManager destroyed";
    clearInstance();
}

void CommandManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
    
    // 监听 scene 销毁，避免悬空引用
    if (scene) {
        connect(scene, &QObject::destroyed, this, [this]() {
            qDebug() << "Scene destroyed, clearing reference";
            m_scene = nullptr;
        });
    }
}

DrawingScene *CommandManager::scene() const
{
    return m_scene;
}

void CommandManager::undo()
{
    if (canUndo()) {
        m_undoStack->undo();
        emit commandExecuted("撤销: " + m_undoStack->undoText());
        if (m_scene) m_scene->setModified(true);
    }
}

void CommandManager::redo()
{
    if (canRedo()) {
        m_undoStack->redo();
        emit commandExecuted("重做: " + m_undoStack->redoText());
        if (m_scene) m_scene->setModified(true);
    }
}

bool CommandManager::canUndo() const
{
    return m_undoStack->canUndo();
}

bool CommandManager::canRedo() const
{
    return m_undoStack->canRedo();
}

void CommandManager::clear()
{
    m_undoStack->clear();
}

void CommandManager::pushCommand(QUndoCommand *command)
{
    if (!command) {
        qWarning() << "CommandManager::pushCommand called with null command";
        return;
    }
    
    if (!m_undoStack) {
        qWarning() << "CommandManager::pushCommand - no undo stack available";
        delete command;
        return;
    }
    
    // qDebug() << "CommandManager::pushCommand called with:" << command->text();
    // qDebug() << "CommandManager::pushCommand - undoStack count before:" << m_undoStack->count();
    m_undoStack->push(command);
    // qDebug() << "CommandManager::pushCommand - undoStack count after:" << m_undoStack->count();
    emit commandExecuted(command->text());
    if (m_scene) m_scene->setModified(true);
    // qDebug() << "CommandManager::pushCommand completed";
}

void CommandManager::beginMacro(const QString& text)
{
    if (m_undoStack) {
        // qDebug() << "CommandManager::beginMacro called with:" << text;
        m_undoStack->beginMacro(text);
    } else {
        qWarning() << "CommandManager::beginMacro - no undo stack available";
    }
}

void CommandManager::endMacro()
{
    if (m_undoStack) {
        // qDebug() << "CommandManager::endMacro called";
        m_undoStack->endMacro();
    } else {
        qWarning() << "CommandManager::endMacro - no undo stack available";
    }
}

// BaseCommand实现
BaseCommand::BaseCommand(CommandManager *manager, const QString &text, QUndoCommand *parent)
    : QUndoCommand(text, parent)
    , m_commandManager(manager)
    , m_scene(manager ? manager->scene() : nullptr)
{
}

// SelectionCommand实现
SelectionCommand::SelectionCommand(CommandManager *manager, const QList<DrawingShape*>& shapes, 
                                const QString &text, QUndoCommand *parent)
    : BaseCommand(manager, text, parent)
    , m_shapes(shapes)
{
}

// TransformCommand实现
TransformCommand::TransformCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                                  const QString &text, QUndoCommand *parent)
    : SelectionCommand(manager, shapes, text, parent)
{
}

void TransformCommand::saveOriginalStates()
{
    m_originalStates.clear();
    for (DrawingShape *shape : m_shapes) {
        if (shape) {
            m_originalStates[shape] = shape->serialize();
        }
    }
}

void TransformCommand::saveNewStates()
{
    m_newStates.clear();
    for (DrawingShape *shape : m_shapes) {
        if (shape) {
            m_newStates[shape] = shape->serialize();
        }
    }
}

void TransformCommand::restoreStates(const QMap<DrawingShape*, QByteArray>& states)
{
    for (auto it = states.begin(); it != states.end(); ++it) {
        DrawingShape *shape = it.key();
        if (shape) {
            shape->deserialize(it.value());
        }
    }
}

// PropertyCommand实现
PropertyCommand::PropertyCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                                const QString &propertyName, const QVariant &oldValue, const QVariant &newValue,
                                const QString &text, QUndoCommand *parent)
    : SelectionCommand(manager, shapes, text, parent)
    , m_propertyName(propertyName)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
}

void PropertyCommand::undo()
{
    // 恢复属性值
    for (DrawingShape *shape : m_shapes) {
        if (!shape) continue;
        
        if (m_propertyName == "fillBrush") {
            shape->setFillBrush(m_oldValue.value<QBrush>());
        } else if (m_propertyName == "strokePen") {
            shape->setStrokePen(m_oldValue.value<QPen>());
        } else if (m_propertyName == "opacity") {
            shape->setOpacity(m_oldValue.toReal());
        } else if (m_propertyName == "rotation") {
            shape->setRotation(m_oldValue.toReal());
        } else if (m_propertyName == "scale") {
            shape->setScale(m_oldValue.toReal());
        } else if (m_propertyName == "pos") {
            shape->setPos(m_oldValue.value<QPointF>());
        } else if (m_propertyName == "zValue") {
            shape->setZValue(m_oldValue.toReal());
        } else if (m_propertyName == "visible") {
            shape->setVisible(m_oldValue.toBool());
        } else if (m_propertyName == "enabled") {
            shape->setEnabled(m_oldValue.toBool());
        }
        // 可以添加更多属性支持
    }
    
    if (m_scene) m_scene->setModified(true);
    emit m_commandManager->statusMessageChanged("已撤销属性修改");
}

void PropertyCommand::redo()
{
    // 设置新属性值
    for (DrawingShape *shape : m_shapes) {
        if (!shape) continue;
        
        if (m_propertyName == "fillBrush") {
            shape->setFillBrush(m_newValue.value<QBrush>());
        } else if (m_propertyName == "strokePen") {
            shape->setStrokePen(m_newValue.value<QPen>());
        } else if (m_propertyName == "opacity") {
            shape->setOpacity(m_newValue.toReal());
        } else if (m_propertyName == "rotation") {
            shape->setRotation(m_newValue.toReal());
        } else if (m_propertyName == "scale") {
            shape->setScale(m_newValue.toReal());
        } else if (m_propertyName == "pos") {
            shape->setPos(m_newValue.value<QPointF>());
        } else if (m_propertyName == "zValue") {
            shape->setZValue(m_newValue.toReal());
        } else if (m_propertyName == "visible") {
            shape->setVisible(m_newValue.toBool());
        } else if (m_propertyName == "enabled") {
            shape->setEnabled(m_newValue.toBool());
        }
        // 可以添加更多属性支持
    }
    
    if (m_scene) m_scene->setModified(true);
    emit m_commandManager->statusMessageChanged("已应用属性修改");
}

// DeleteCommand实现 - 简化版本，专注对象生命周期管理
DeleteCommand::DeleteCommand(CommandManager *manager, const QList<DrawingShape*>& shapes, 
                            QUndoCommand *parent)
    : SelectionCommand(manager, shapes, "删除对象", parent)
{
    // 只保存需要的信息：父级关系和场景位置
    for (DrawingShape *shape : shapes) {
        if (shape) {
            m_parents[shape] = shape->parentItem();
            m_positions[shape] = shape->scenePos();
        }
    }
}

void DeleteCommand::undo()
{
    if (!m_scene) return;
    
    // 从场景中恢复被删除的对象
    for (auto it = m_parents.begin(); it != m_parents.end(); ++it) {
        DrawingShape *shape = it.key();
        QGraphicsItem *parent = it.value();
        QPointF pos = m_positions[shape];
        
        if (shape) {
            // 重新设置父级关系
            shape->setParentItem(parent);
            // 恢复场景位置
            shape->setPos(pos);
            // 添加到场景
            m_scene->addItem(shape);
            shape->setVisible(true);
            
            // 通知工具对象状态已变化
            emit m_scene->objectStateChanged(shape);
        }
    }
    
    emit m_commandManager->statusMessageChanged(QString("已恢复 %1 个对象").arg(m_shapes.count()));
}

void DeleteCommand::redo()
{
    if (!m_scene) return;
    
    // 删除对象（从场景中移除，但不删除对象）
    for (DrawingShape *shape : m_shapes) {
        if (shape) {
            m_scene->removeItem(shape);
            shape->setVisible(false);
        }
    }
    
    emit m_commandManager->statusMessageChanged(QString("已删除 %1 个对象").arg(m_shapes.count()));
}

// DuplicateCommand实现
DuplicateCommand::DuplicateCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                                  const QPointF &offset, QUndoCommand *parent)
    : SelectionCommand(manager, shapes, "复制对象", parent)
    , m_offset(offset)
{
    // 保存原始对象的序列化数据
    for (DrawingShape *shape : shapes) {
        if (shape) {
            m_serializedData[shape] = shape->serialize();
        }
    }
}

DuplicateCommand::~DuplicateCommand()
{
    // 简单的清理：只删除重复对象，不做复杂的检查
    // 如果对象仍在场景中，说明撤销操作已经发生，不应该删除
    // 如果对象不在场景中，说明命令正常结束，可以删除
    for (DrawingShape *shape : m_duplicatedShapes) {
        if (shape && shape->scene() != m_scene) {
            delete shape;
        }
    }
    m_duplicatedShapes.clear();
}

void DuplicateCommand::undo()
{
    if (!m_scene) return;
    
    // 只从场景中移除复制的对象，不删除
    for (DrawingShape *shape : m_duplicatedShapes) {
        if (shape) {
            m_scene->removeItem(shape);
        }
    }
    
    emit m_commandManager->statusMessageChanged("已撤销复制");
}

void DuplicateCommand::redo()
{
    if (!m_scene) return;
    
    // 如果复制的对象已存在，只需重新添加到场景
    if (!m_duplicatedShapes.isEmpty()) {
        for (DrawingShape *shape : m_duplicatedShapes) {
            if (shape) {
                m_scene->addItem(shape);
                shape->setSelected(true);
            }
        }
        emit m_commandManager->statusMessageChanged(QString("已复制 %1 个对象").arg(m_duplicatedShapes.count()));
        return;
    }
    
    // 第一次执行，创建复制的对象
    // 先清除原始对象的选择状态
    for (DrawingShape *original : m_shapes) {
        if (original) {
            original->setSelected(false);
        }
    }
    
    for (DrawingShape *original : m_shapes) {
        if (!original) continue;
        
        // 尝试使用clone方法，如果失败则使用序列化
        qDebug() << "DuplicateCommand: attempting clone for shape type:" << original->shapeType();
        DrawingShape *duplicate = original->clone();
        qDebug() << "DuplicateCommand: clone() result:" << (duplicate != nullptr);
        
        if (!duplicate) {
            // 回退到序列化方式
            qDebug() << "DuplicateCommand: clone failed, using serialization";
            QByteArray shapeData = original->serialize();
            QDataStream typeStream(shapeData);
            int typeValue;
            typeStream >> typeValue;
            DrawingShape::ShapeType shapeType = static_cast<DrawingShape::ShapeType>(typeValue);
            
            switch (shapeType) {
                case DrawingShape::Rectangle:
                    duplicate = new DrawingRectangle();
                    break;
                case DrawingShape::Ellipse:
                    duplicate = new DrawingEllipse();
                    break;
                case DrawingShape::Line:
                    duplicate = new DrawingLine();
                    break;
                case DrawingShape::Path:
                    duplicate = new DrawingPath();
                    break;
                case DrawingShape::Polyline:
                    duplicate = new DrawingPolyline();
                    break;
                case DrawingShape::Polygon:
                    duplicate = new DrawingPolygon();
                    break;
                case DrawingShape::Text:
                    duplicate = new DrawingText();
                    break;
                case DrawingShape::Group:
                    duplicate = new DrawingGroup();
                    break;
                default:
                    continue;
            }
            
            if (duplicate) {
                qDebug() << "DuplicateCommand: deserializing shape type" << shapeType << "data size:" << shapeData.size();
                duplicate->deserialize(shapeData);
                qDebug() << "DuplicateCommand: deserialized pos:" << duplicate->pos();
                qDebug() << "DuplicateCommand: rect bounds:" << duplicate->boundingRect();
                qDebug() << "DuplicateCommand: stroke pen:" << duplicate->strokePen().color();
                qDebug() << "DuplicateCommand: fill brush:" << duplicate->fillBrush().color();
            }
        }
        
        // 无论使用哪种方式，都添加到场景
        if (duplicate) {
            // 应用偏移
            QPointF currentPos = duplicate->pos();
            QPointF newPos = currentPos + m_offset;
            qDebug() << "DuplicateCommand: currentPos:" << currentPos << "newPos:" << newPos;
            duplicate->setPos(newPos);
            
            m_scene->addItem(duplicate);
            duplicate->setSelected(true);
            qDebug() << "DuplicateCommand: shape added to scene, visible:" << duplicate->isVisible();
            
            // 添加到活动图层
            LayerManager *layerManager = LayerManager::instance();
            if (layerManager) {
                DrawingLayer *activeLayer = layerManager->activeLayer();
                if (activeLayer) {
                    qDebug() << "DuplicateCommand: adding shape to layer, already in scene:" << (duplicate->scene() != nullptr);
                    activeLayer->addShape(duplicate);
                }
            }
            
            m_duplicatedShapes.append(duplicate);
        }
    }
    
    emit m_commandManager->statusMessageChanged(QString("已复制 %1 个对象").arg(m_duplicatedShapes.count()));
}

// AlignCommand实现
AlignCommand::AlignCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                          AlignmentType alignment, QUndoCommand *parent)
    : TransformCommand(manager, shapes, "对齐对象", parent)
    , m_alignment(alignment)
{
    saveOriginalStates();
}

void AlignCommand::undo()
{
    restoreStates(m_originalStates);
    emit m_commandManager->statusMessageChanged("已撤销对齐");
}

void AlignCommand::redo()
{
    saveNewStates();
    
    // 执行对齐操作
    if (m_shapes.isEmpty()) return;
    
    // 计算对齐边界
    QRectF combinedBounds = m_shapes.first()->boundingRect();
    combinedBounds.translate(m_shapes.first()->pos());
    
    for (DrawingShape *shape : m_shapes) {
        QRectF shapeBounds = shape->boundingRect();
        shapeBounds.translate(shape->pos());
        combinedBounds |= shapeBounds;
    }
    
    // 根据对齐类型执行对齐
    switch (m_alignment) {
        case Left: {
            qreal leftEdge = combinedBounds.left();
            for (DrawingShape *shape : m_shapes) {
                QPointF pos = shape->pos();
                pos.setX(leftEdge);
                shape->setPos(pos);
            }
            break;
        }
        case Center: {
            qreal center = combinedBounds.left() + combinedBounds.width() / 2;
            for (DrawingShape *shape : m_shapes) {
                QRectF bounds = shape->boundingRect();
                QPointF pos = shape->pos();
                pos.setX(center - bounds.width() / 2);
                shape->setPos(pos);
            }
            break;
        }
        case Right: {
            qreal rightEdge = combinedBounds.right();
            for (DrawingShape *shape : m_shapes) {
                QRectF bounds = shape->boundingRect();
                QPointF pos = shape->pos();
                pos.setX(rightEdge - bounds.width());
                shape->setPos(pos);
            }
            break;
        }
        case Top: {
            qreal topEdge = combinedBounds.top();
            for (DrawingShape *shape : m_shapes) {
                QPointF pos = shape->pos();
                pos.setY(topEdge);
                shape->setPos(pos);
            }
            break;
        }
        case Middle: {
            qreal middle = combinedBounds.top() + combinedBounds.height() / 2;
            for (DrawingShape *shape : m_shapes) {
                QRectF bounds = shape->boundingRect();
                QPointF pos = shape->pos();
                pos.setY(middle - bounds.height() / 2);
                shape->setPos(pos);
            }
            break;
        }
        case Bottom: {
            qreal bottomEdge = combinedBounds.bottom();
            for (DrawingShape *shape : m_shapes) {
                QRectF bounds = shape->boundingRect();
                QPointF pos = shape->pos();
                pos.setY(bottomEdge - bounds.height());
                shape->setPos(pos);
            }
            break;
        }
    }
    
    QString alignmentName;
    switch (m_alignment) {
        case Left: alignmentName = "左对齐"; break;
        case Center: alignmentName = "水平居中"; break;
        case Right: alignmentName = "右对齐"; break;
        case Top: alignmentName = "顶对齐"; break;
        case Middle: alignmentName = "垂直居中"; break;
        case Bottom: alignmentName = "底对齐"; break;
    }
    
    emit m_commandManager->statusMessageChanged(QString("已%1").arg(alignmentName));
}

// EffectCommand实现
EffectCommand::EffectCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                            EffectType effectType, const QVariant &effectParams,
                            const QString &text, QUndoCommand *parent)
    : TransformCommand(manager, shapes, text, parent)
    , m_effectType(effectType)
    , m_effectParams(effectParams)
{
    saveOriginalStates();
}

void EffectCommand::undo()
{
    restoreStates(m_originalStates);
    emit m_commandManager->statusMessageChanged("已撤销效果");
}

void EffectCommand::redo()
{
    qDebug() << "EffectCommand::redo called, effectType:" << m_effectType << "shapes count:" << m_shapes.count();
    saveNewStates();
    
    // 应用效果
    for (DrawingShape *shape : m_shapes) {
        if (!shape) continue;
        qDebug() << "Applying effect to shape:" << shape;
        
        switch (m_effectType) {
            case Blur: {
                qreal radius = m_effectParams.toDouble();
                // 应用模糊效果
                QGraphicsBlurEffect *blurEffect = new QGraphicsBlurEffect();
                blurEffect->setBlurRadius(radius);
                shape->setGraphicsEffect(blurEffect);
                
                // 同时修改填充颜色的透明度来模拟模糊效果
                QBrush fillBrush = shape->fillBrush();
                QColor fillColor = fillBrush.color();
                fillColor.setAlpha(200); // 设置一些透明度
                shape->setFillBrush(fillBrush);
                break;
            }
            case DropShadow: {
                // 解析阴影参数
                QVariantMap params = m_effectParams.toMap();
                QColor color = params.value("color", QColor(Qt::black)).value<QColor>();
                qreal blurRadius = params.value("blurRadius", 3.0).toReal();
                QPointF offset = params.value("offset", QPointF(3.0, 3.0)).toPointF();
                
                // 应用阴影效果
                QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect();
                shadowEffect->setColor(color);
                shadowEffect->setBlurRadius(blurRadius);
                shadowEffect->setOffset(offset);
                shape->setGraphicsEffect(shadowEffect);
                
                // 同时修改描边颜色来模拟阴影
                QPen strokePen = shape->strokePen();
                strokePen.setColor(color);
                strokePen.setWidth(2);
                shape->setStrokePen(strokePen);
                break;
            }
            case ClearEffect: {
                // 清除所有效果
                shape->setGraphicsEffect(nullptr);
                
                // 恢复原始颜色（这里使用默认值，实际应该保存原始状态）
                QBrush fillBrush = shape->fillBrush();
                QColor fillColor = fillBrush.color();
                fillColor.setAlpha(255); // 恢复完全不透明
                shape->setFillBrush(fillBrush);
                
                QPen strokePen = shape->strokePen();
                strokePen.setColor(Qt::black);
                strokePen.setWidth(2);
                shape->setStrokePen(strokePen);
                break;
            }
        }
    }
    
    emit m_commandManager->statusMessageChanged("已应用效果");
}

// CreateCommand 实现
CreateCommand::CreateCommand(CommandManager *manager, DrawingShape *shape, 
                            const QString &text, QUndoCommand *parent)
    : BaseCommand(manager, text, parent)
    , m_shape(shape)
    , m_addedToScene(false)
{
}

void CreateCommand::undo()
{
    if (!m_scene || !m_shape) return;
    
    if (m_addedToScene) {
        m_scene->removeItem(m_shape);
        m_shape->setVisible(false);
        m_addedToScene = false;
        
        // 通知所有工具对象已被删除，传递被删除的图形对象
        // 工具可以通过检查shape->scene()来判断对象是否已被删除
        emit m_scene->objectStateChanged(m_shape);
    }
    
    emit m_commandManager->statusMessageChanged(QString("已撤销创建: %1").arg(text()));
}

void CreateCommand::redo()
{
    if (!m_scene || !m_shape) return;
    
    if (!m_addedToScene) {
        m_scene->addItem(m_shape);
        m_shape->setVisible(true);
        m_shape->setSelected(true);
        m_addedToScene = true;
        
        // 清除其他选中项
        for (QGraphicsItem *item : m_scene->selectedItems()) {
            if (item != m_shape) {
                item->setSelected(false);
            }
        }
    }
    
    emit m_commandManager->statusMessageChanged(QString("已创建: %1").arg(text()));
}

// PasteCommand 实现
PasteCommand::PasteCommand(CommandManager *manager, const QList<QByteArray> &shapeDataList, 
                           const QPointF &offset, QUndoCommand *parent)
    : BaseCommand(manager, "粘贴对象", parent)
    , m_shapeDataList(shapeDataList)
    , m_offset(offset)
{
}

void PasteCommand::undo()
{
    if (!m_scene) return;
    
    // 删除粘贴的对象
    for (DrawingShape *shape : m_pastedShapes) {
        if (shape) {
            m_scene->removeItem(shape);
            delete shape;
        }
    }
    m_pastedShapes.clear();
    
    emit m_commandManager->statusMessageChanged("已撤销粘贴");
}

void PasteCommand::redo()
{
    qDebug() << "PasteCommand::redo called";
    if (!m_scene) {
        qDebug() << "PasteCommand::redo: scene is null";
        return;
    }
    
    m_pastedShapes.clear();
    
    // 创建并添加对象到场景
    for (const QByteArray &shapeData : m_shapeDataList) {
        // 读取形状类型
        QDataStream typeStream(shapeData);
        int typeValue;
        typeStream >> typeValue;
        DrawingShape::ShapeType shapeType = static_cast<DrawingShape::ShapeType>(typeValue);
        
        // 创建对应的形状对象
        DrawingShape *shape = nullptr;
        switch (shapeType) {
            case DrawingShape::Rectangle:
                shape = new DrawingRectangle();
                break;
            case DrawingShape::Ellipse:
                shape = new DrawingEllipse();
                break;
            case DrawingShape::Line:
                shape = new DrawingLine();
                break;
            case DrawingShape::Path:
                shape = new DrawingPath();
                break;
            case DrawingShape::Polyline:
                shape = new DrawingPolyline();
                break;
            case DrawingShape::Polygon:
                shape = new DrawingPolygon();
                break;
            case DrawingShape::Text:
                shape = new DrawingText();
                break;
            case DrawingShape::Group:
                shape = new DrawingGroup();
                break;
            default:
                qDebug() << "Unsupported shape type for pasting:" << typeValue;
                continue;
        }
        
        if (shape) {
            qDebug() << "PasteCommand: deserializing shape type" << shapeType << "data size:" << shapeData.size();
            shape->deserialize(shapeData);
            qDebug() << "PasteCommand: deserialized shape pos:" << shape->pos();
            // 应用偏移
            shape->setPos(shape->pos() + m_offset);
            qDebug() << "PasteCommand: final pos after offset:" << shape->pos();
            
            m_scene->addItem(shape);
            shape->setSelected(true);
            m_pastedShapes.append(shape);
            qDebug() << "PasteCommand: shape added to scene, visible:" << shape->isVisible();
        }
    }
    
    qDebug() << "PasteCommand::redo created" << m_pastedShapes.count() << "pasted shapes";
    emit m_commandManager->statusMessageChanged(QString("已粘贴 %1 个对象").arg(m_pastedShapes.count()));
}

// GroupCommand 实现
GroupCommand::GroupCommand(CommandManager *manager, const QList<DrawingShape*>& shapes, 
                           QUndoCommand *parent)
    : SelectionCommand(manager, shapes, "组合对象", parent)
    , m_group(nullptr)
{
    // 保存每个对象的父级和位置信息
    for (DrawingShape *shape : shapes) {
        if (shape) {
            m_parents[shape] = shape->parentItem();
            m_positions[shape] = shape->pos();
        }
    }
}

void GroupCommand::undo()
{
    if (!m_scene || !m_group) return;
    
    // 从场景中移除组对象
    m_scene->removeItem(m_group);
    
    // 恢复所有子对象的父级关系和位置
    for (auto it = m_parents.begin(); it != m_parents.end(); ++it) {
        DrawingShape *shape = it.key();
        QGraphicsItem *parent = it.value();
        QPointF pos = m_positions[shape];
        
        if (shape) {
            // 恢复父级关系
            shape->setParentItem(parent);
            // 恢复位置
            shape->setPos(pos);
            // 恢复选择能力
            shape->setFlag(QGraphicsItem::ItemIsSelectable, true);
            shape->setFlag(QGraphicsItem::ItemIsMovable, true);
        }
    }
    
    // 删除组对象
    delete m_group;
    m_group = nullptr;
    
    emit m_commandManager->statusMessageChanged("已取消组合");
}

void GroupCommand::redo()
{
    if (!m_scene) return;
    
    // 创建新的组对象
    m_group = new DrawingGroup();
    
    // 将所有形状添加到组中
    for (DrawingShape *shape : m_shapes) {
        if (shape) {
            m_group->addItem(shape);
        }
    }
    
    // 将组添加到场景中
    m_scene->addItem(m_group);
    
    // 选中组对象
    m_group->setSelected(true);
    
    emit m_commandManager->statusMessageChanged(QString("已组合 %1 个对象").arg(m_shapes.count()));
}

// UngroupCommand 实现
UngroupCommand::UngroupCommand(CommandManager *manager, const QList<DrawingGroup*>& groups, 
                               QUndoCommand *parent)
    : SelectionCommand(manager, QList<DrawingShape*>(), "取消组合", parent)
{
    // 保存组信息
    m_groups = groups;
    
    // 收集所有子对象并保存它们的信息
    for (DrawingGroup *group : groups) {
        if (group) {
            QList<DrawingShape*> items = group->items();
            for (DrawingShape *item : items) {
                if (item) {
                    m_parentGroups[item] = group;
                    m_positions[item] = item->scenePos();
                    m_ungroupedShapes.append(item);
                }
            }
        }
    }
}

void UngroupCommand::undo()
{
    if (!m_scene) return;
    
    // 重新创建组并添加子对象
    for (DrawingGroup *group : m_groups) {
        if (!group) continue;
        
        // 创建新的组对象
        DrawingGroup *newGroup = new DrawingGroup();
        
        // 找到属于这个组的所有子对象
        for (DrawingShape *item : m_ungroupedShapes) {
            if (m_parentGroups[item] == group && item) {
                newGroup->addItem(item);
            }
        }
        
        // 将组添加到场景中
        m_scene->addItem(newGroup);
    }
    
    emit m_commandManager->statusMessageChanged("已重新组合");
}

void UngroupCommand::redo()
{
    if (!m_scene) return;
    
    // 取消所有组的组合
    for (DrawingGroup *group : m_groups) {
        if (!group) continue;
        
        // 获取所有子对象
        QList<DrawingShape*> items = group->ungroup();
        
        // 将子对象重新添加到场景中
        for (DrawingShape *item : items) {
            if (item) {
                m_scene->addItem(item);
                item->setSelected(true);
            }
        }
        
        // 从场景中移除组对象
        m_scene->removeItem(group);
        delete group;
    }
    
    emit m_commandManager->statusMessageChanged(QString("已取消组合 %1 个组").arg(m_groups.count()));
}

// TextToPathCommand实现
TextToPathCommand::TextToPathCommand(CommandManager *manager, const QList<DrawingText*>& textShapes, 
                                     QUndoCommand *parent)
    : BaseCommand(manager, "文本转路径", parent)
    , m_textShapes(textShapes)
{
    // 初始化数据结构
    for (DrawingText *textShape : textShapes) {
        m_positions.append(textShape->position());
        m_fillBrushes[textShape] = textShape->fillBrush();
        m_strokePens[textShape] = textShape->strokePen();
    }
}

void TextToPathCommand::redo()
{
    // 转换每个文本对象为路径
    for (int i = 0; i < m_textShapes.count(); ++i) {
        DrawingText *textShape = m_textShapes[i];
        
        // 获取文本属性
        QString text = textShape->text();
        QFont font = textShape->font();
        
        // 创建路径并添加文本
        QPainterPath textPath;
        textPath.addText(QPointF(0, 0), font, text);
        
        // 创建新的路径图形
        DrawingPath *pathShape = new DrawingPath();
        pathShape->setPath(textPath);
        
        // 复制文本的样式
        pathShape->setFillBrush(textShape->fillBrush());
        pathShape->setStrokePen(textShape->strokePen());
        
        // 设置文档
        pathShape->setDocument(textShape->document());
        
        // 设置位置到原文本的位置
        pathShape->setPos(textShape->position());
        
        // 添加到场景
        if (m_scene) {
            m_scene->addItem(pathShape);
            m_pathShapes.append(pathShape);
            
            // 从场景中移除原文本
            m_scene->removeItem(textShape);
        }
    }
    
    emit m_commandManager->statusMessageChanged(QString("已转换 %1 个文本对象为路径").arg(m_textShapes.count()));
}

void TextToPathCommand::undo()
{
    // 恢复每个文本对象
    for (int i = 0; i < m_textShapes.count(); ++i) {
        DrawingText *textShape = m_textShapes[i];
        DrawingPath *pathShape = (i < m_pathShapes.count()) ? m_pathShapes[i] : nullptr;
        
        // 恢复文本对象到场景
        if (m_scene && textShape) {
            m_scene->addItem(textShape);
            
            // 恢复位置和样式
            textShape->setPos(m_positions[i]);
            textShape->setFillBrush(m_fillBrushes[textShape]);
            textShape->setStrokePen(m_strokePens[textShape]);
            
            // 移除路径对象
            if (pathShape) {
                m_scene->removeItem(pathShape);
                delete pathShape;
            }
        }
    }
    
    m_pathShapes.clear();
    emit m_commandManager->statusMessageChanged(QString("已恢复 %1 个文本对象").arg(m_textShapes.count()));
}

// TextEditCommand 实现
TextEditCommand::TextEditCommand(CommandManager *manager, DrawingText *textShape, 
                                const QString &oldText, const QString &newText,
                                QUndoCommand *parent)
    : BaseCommand(manager, "编辑文本", parent)
    , m_textShape(textShape)
    , m_oldText(oldText)
    , m_newText(newText)
{
}

void TextEditCommand::undo()
{
    if (m_textShape) {
        m_textShape->setText(m_oldText);
        emit m_commandManager->statusMessageChanged("已撤销文本编辑");
    }
}

void TextEditCommand::redo()
{
    if (m_textShape) {
        m_textShape->setText(m_newText);
        emit m_commandManager->statusMessageChanged("已编辑文本");
    }
}