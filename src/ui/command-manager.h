#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <QObject>
#include <QUndoStack>
#include <QUndoCommand>
#include <QList>
#include <QMap>
#include <QByteArray>
#include <QVariant>
#include <QVariantMap>
#include <QPointF>
#include <QGraphicsItem>

class DrawingScene;
class DrawingShape;
class DrawingGroup;
class DrawingText;
class DrawingPath;

class CommandManager : public QObject
{
    Q_OBJECT

public:
    // 禁用拷贝和移动
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;
    
    // 获取当前实例（可能为 nullptr）
    static CommandManager* instance() { return s_instance; }
    
    // 检查实例是否有效
    static bool hasInstance() { return s_instance != nullptr; }
    
    // 设置实例（只能调用一次）
    static void setInstance(CommandManager* instance) {
        Q_ASSERT(s_instance == nullptr && "CommandManager instance already set");
        s_instance = instance;
    }
    
    // 清理实例
    static void clearInstance() {
        s_instance = nullptr;
    }

    explicit CommandManager(QObject *parent = nullptr);
    ~CommandManager();

    void setScene(DrawingScene *scene);
    DrawingScene *scene() const;
    
    // 安全检查方法
    bool hasValidScene() const { return m_scene != nullptr; }
    
    // 撤销/重做操作
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    void clear();
    
    // 命令执行
    void pushCommand(QUndoCommand *command);
    
    // 便捷的命令创建和推送
    template<typename T, typename... Args>
    void createAndPush(Args&&... args) {
        pushCommand(new T(std::forward<Args>(args)...));
    }
    
    // 宏命令支持
    void beginMacro(const QString& text);
    void endMacro();
    
    // 便捷的宏命令方法
    template<typename Func>
    void executeMacro(const QString& text, Func&& func) {
        beginMacro(text);
        func();
        endMacro();
    }
    
    // 获取撤销栈
    QUndoStack* undoStack() const { return m_undoStack; }

signals:
    void commandExecuted(const QString &commandText);
    void undoStackChanged();
    void statusMessageChanged(const QString &message);
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);

private:
    static CommandManager* s_instance;
    DrawingScene *m_scene;
    QUndoStack *m_undoStack;
};

// 基础命令类
class BaseCommand : public QUndoCommand
{
public:
    BaseCommand(CommandManager *manager, const QString &text, QUndoCommand *parent = nullptr);
    
protected:
    CommandManager *m_commandManager;
    DrawingScene *m_scene;
};

// 选择相关命令
class SelectionCommand : public BaseCommand
{
public:
    SelectionCommand(CommandManager *manager, const QList<DrawingShape*>& shapes, 
                    const QString &text, QUndoCommand *parent = nullptr);
    
protected:
    QList<DrawingShape*> m_shapes;
};

// 变换命令
class TransformCommand : public SelectionCommand
{
public:
    TransformCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                    const QString &text, QUndoCommand *parent = nullptr);
    
protected:
    QMap<DrawingShape*, QByteArray> m_originalStates;
    QMap<DrawingShape*, QByteArray> m_newStates;
    
    void saveOriginalStates();
    void saveNewStates();
    void restoreStates(const QMap<DrawingShape*, QByteArray>& states);
};

// 属性修改命令
class PropertyCommand : public SelectionCommand
{
public:
    PropertyCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                   const QString &propertyName, const QVariant &oldValue, const QVariant &newValue,
                   const QString &text, QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    QString m_propertyName;
    QVariant m_oldValue;
    QVariant m_newValue;
};

// 删除命令
class DeleteCommand : public SelectionCommand
{
public:
    DeleteCommand(CommandManager *manager, const QList<DrawingShape*>& shapes, 
                 QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    QMap<DrawingShape*, QGraphicsItem*> m_parents;
    QMap<DrawingShape*, QPointF> m_positions; // 保存场景位置
};

// 复制命令
class DuplicateCommand : public SelectionCommand
{
public:
    DuplicateCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                    const QPointF &offset, QUndoCommand *parent = nullptr);
    
    ~DuplicateCommand();
    void undo() override;
    void redo() override;
    
private:
    QPointF m_offset;
    QList<DrawingShape*> m_duplicatedShapes;
    QMap<DrawingShape*, QByteArray> m_serializedData;
};

// 对齐命令
class AlignCommand : public TransformCommand
{
public:
    enum AlignmentType {
        Left, Center, Right,
        Top, Middle, Bottom
    };
    
    AlignCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                AlignmentType alignment, QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    AlignmentType m_alignment;
};

// 效果命令
class EffectCommand : public TransformCommand
{
public:
    enum EffectType {
        Blur,
        DropShadow,
        ClearEffect
    };
    
    EffectCommand(CommandManager *manager, const QList<DrawingShape*>& shapes,
                 EffectType effectType, const QVariant &effectParams,
                 const QString &text, QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    EffectType m_effectType;
    QVariant m_effectParams;
};

// 创建命令
class CreateCommand : public BaseCommand
{
public:
    CreateCommand(CommandManager *manager, DrawingShape *shape, 
                 const QString &text = "创建对象", QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    DrawingShape *m_shape;
    bool m_addedToScene;
};

// 粘贴命令
class PasteCommand : public BaseCommand
{
public:
    PasteCommand(CommandManager *manager, const QList<QByteArray> &shapeDataList, 
                 const QPointF &offset, QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    QList<QByteArray> m_shapeDataList;
    QPointF m_offset;
    QList<DrawingShape*> m_pastedShapes;
};

// 分组命令
class GroupCommand : public SelectionCommand
{
public:
    GroupCommand(CommandManager *manager, const QList<DrawingShape*>& shapes, 
                 QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    DrawingGroup *m_group;
    QMap<DrawingShape*, QGraphicsItem*> m_parents;
    QMap<DrawingShape*, QPointF> m_positions;
};

// 取消分组命令
class UngroupCommand : public SelectionCommand
{
public:
    UngroupCommand(CommandManager *manager, const QList<DrawingGroup*>& groups, 
                   QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    QList<DrawingGroup*> m_groups;
    QList<DrawingShape*> m_ungroupedShapes;
    QMap<DrawingShape*, DrawingGroup*> m_parentGroups;
    QMap<DrawingShape*, QPointF> m_positions;
};

// 文本转路径命令
class TextToPathCommand : public BaseCommand
{
public:
    TextToPathCommand(CommandManager *manager, const QList<DrawingText*>& textShapes, 
                      QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    QList<DrawingText*> m_textShapes;
    QList<DrawingPath*> m_pathShapes;
    QList<QPointF> m_positions;
    QMap<DrawingText*, QBrush> m_fillBrushes;
    QMap<DrawingText*, QPen> m_strokePens;
};

#endif // COMMAND_MANAGER_H