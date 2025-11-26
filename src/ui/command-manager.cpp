#include "command-manager.h"
#include "drawingscene.h"
#include "../core/drawing-shape.h"
#include <QGraphicsItem>
#include <QDataStream>

CommandManager::CommandManager(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
    , m_undoStack(new QUndoStack(this))
{
    connect(m_undoStack, &QUndoStack::cleanChanged, this, &CommandManager::undoStackChanged);
    connect(m_undoStack, &QUndoStack::canUndoChanged, this, [this](bool canUndo) { 
        qDebug() << "CommandManager: canUndoChanged:" << canUndo; 
        emit canUndoChanged(canUndo); 
    });
    connect(m_undoStack, &QUndoStack::canRedoChanged, this, [this](bool canRedo) { 
        qDebug() << "CommandManager: canRedoChanged:" << canRedo; 
        emit canRedoChanged(canRedo); 
    });
    connect(m_undoStack, &QUndoStack::undoTextChanged, this, &CommandManager::undoStackChanged);
}

CommandManager::~CommandManager()
{
}

void CommandManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
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
    if (command) {
        qDebug() << "CommandManager::pushCommand called with:" << command->text();
        qDebug() << "CommandManager::pushCommand - undoStack count before:" << m_undoStack->count();
        m_undoStack->push(command);
        qDebug() << "CommandManager::pushCommand - undoStack count after:" << m_undoStack->count();
        emit commandExecuted(command->text());
        if (m_scene) m_scene->setModified(true);
        qDebug() << "CommandManager::pushCommand completed";
    } else {
        qDebug() << "CommandManager::pushCommand called with null command";
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
    // TODO: 实现属性恢复逻辑
    emit m_commandManager->statusMessageChanged("已撤销属性修改");
}

void PropertyCommand::redo()
{
    // TODO: 实现属性设置逻辑
    emit m_commandManager->statusMessageChanged("已应用属性修改");
}

// DeleteCommand实现
DeleteCommand::DeleteCommand(CommandManager *manager, const QList<DrawingShape*>& shapes, 
                            QUndoCommand *parent)
    : SelectionCommand(manager, shapes, "删除对象", parent)
{
    // 保存父级关系和序列化数据
    for (DrawingShape *shape : shapes) {
        if (shape) {
            m_parents[shape] = shape->parentItem();
            m_serializedData[shape] = shape->serialize();
        }
    }
}

void DeleteCommand::undo()
{
    if (!m_scene) return;
    
    // 恢复删除的对象
    for (auto it = m_serializedData.begin(); it != m_serializedData.end(); ++it) {
        DrawingShape *shape = it.key();
        if (shape) {
            // 重新创建对象（需要工厂方法支持）
            // TODO: 实现对象恢复逻辑
            if (m_parents.contains(shape)) {
                shape->setParentItem(m_parents[shape]);
            }
            m_scene->addItem(shape);
        }
    }
    
    emit m_commandManager->statusMessageChanged(QString("已恢复 %1 个对象").arg(m_shapes.count()));
}

void DeleteCommand::redo()
{
    if (!m_scene) return;
    
    // 删除对象
    for (DrawingShape *shape : m_shapes) {
        if (shape) {
            m_scene->removeItem(shape);
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

void DuplicateCommand::undo()
{
    if (!m_scene) return;
    
    // 删除复制的对象
    for (DrawingShape *shape : m_duplicatedShapes) {
        if (shape) {
            m_scene->removeItem(shape);
            delete shape;
        }
    }
    m_duplicatedShapes.clear();
    
    emit m_commandManager->statusMessageChanged("已撤销复制");
}

void DuplicateCommand::redo()
{
    qDebug() << "DuplicateCommand::redo called";
    if (!m_scene) {
        qDebug() << "DuplicateCommand::redo: scene is null";
        return;
    }
    
    m_duplicatedShapes.clear();
    
    // 复制对象
    for (auto it = m_serializedData.begin(); it != m_serializedData.end(); ++it) {
        DrawingShape *original = it.key();
        if (original) {
            // 使用序列化数据创建新对象，而不是clone()
            QByteArray shapeData = it.value();
            
            // 读取形状类型
            QDataStream typeStream(shapeData);
            int typeValue;
            typeStream >> typeValue;
            DrawingShape::ShapeType shapeType = static_cast<DrawingShape::ShapeType>(typeValue);
            
            // 创建对应的形状对象
            DrawingShape *duplicate = nullptr;
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
                    qDebug() << "Unsupported shape type for duplication:" << typeValue;
                    continue;
            }
            
            if (duplicate) {
                // 反序列化数据
                duplicate->deserialize(shapeData);
                
                // 应用偏移
                QPointF currentPos = duplicate->pos();
                duplicate->setPos(currentPos + m_offset);
                
                m_scene->addItem(duplicate);
                duplicate->setSelected(true);
                m_duplicatedShapes.append(duplicate);
            }
        }
    }
    
    qDebug() << "DuplicateCommand::redo created" << m_duplicatedShapes.count() << "duplicated shapes";
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
    // TODO: 实现具体的对齐逻辑
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
            shape->deserialize(shapeData);
            // 应用偏移
            shape->setPos(shape->pos() + m_offset);
            
            m_scene->addItem(shape);
            shape->setSelected(true);
            m_pastedShapes.append(shape);
        }
    }
    
    qDebug() << "PasteCommand::redo created" << m_pastedShapes.count() << "pasted shapes";
    emit m_commandManager->statusMessageChanged(QString("已粘贴 %1 个对象").arg(m_pastedShapes.count()));
}