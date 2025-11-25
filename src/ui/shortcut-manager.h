#ifndef SHORTCUT_MANAGER_H
#define SHORTCUT_MANAGER_H

#include <QObject>
#include <QHash>
#include <QKeySequence>
#include <QAction>
#include <QTimer>

// 前向声明
class DrawingScene;
class DrawingView;
class ToolManager;

// 枚举声明
enum class ToolType : int;

/**
 * 上下文枚举 - 用于定义快捷键生效的上下文条件
 */
enum class ShortcutContext {
    Global,           // 全局有效
    HasSelection,     // 有选中对象时有效
    NoSelection,      // 无选中对象时有效
    NodeEditMode,     // 节点编辑模式有效
    PathEditMode,     // 路径编辑模式有效
    TextEditMode      // 文本编辑模式有效
};

/**
 * 快捷键管理器 - 处理快捷键冲突和临时工具切换
 */
class ShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutManager(QObject *parent = nullptr);
    
    // 设置工具管理器和场景
    void setToolManager(ToolManager* toolManager);
    void setScene(DrawingScene* scene);
    
    // 注册快捷键
    void registerShortcut(ToolType toolType, const QKeySequence& shortcut, QAction* action = nullptr, 
                         ShortcutContext context = ShortcutContext::Global);
    void registerTemporaryShortcut(ToolType toolType, const QKeySequence& shortcut, 
                                  ShortcutContext context = ShortcutContext::Global);
    
    // 快捷键处理
    bool handleKeyPress(const QKeySequence& key);
    bool handleKeyRelease(const QKeySequence& key);
    
    // 临时工具切换
    void enableTemporaryMode(bool enabled) { m_temporaryModeEnabled = enabled; }
    bool isTemporaryModeEnabled() const { return m_temporaryModeEnabled; }
    
    // 获取工具的当前快捷键
    QKeySequence getShortcut(ToolType toolType) const;
    
    // 设置快捷键
    bool setShortcut(ToolType toolType, const QKeySequence& shortcut);

signals:
    void temporaryToolActivated(ToolType toolType);
    void temporaryToolDeactivated(ToolType toolType);
    void shortcutConflict(ToolType tool1, ToolType tool2, const QKeySequence& shortcut);

private slots:
    void onTemporaryTimeout();

private:
    struct ShortcutInfo {
        ToolType toolType;
        QKeySequence shortcut;
        QAction* action;
        bool isTemporary;
        ShortcutContext context;
    };
    
    void checkConflicts(ToolType toolType, const QKeySequence& shortcut, bool isTemporary);
    bool isContextValid(ShortcutContext context) const;
    void activateTemporaryTool(ToolType toolType);
    void restorePreviousTool();
    
    QHash<QKeySequence, ShortcutInfo> m_shortcuts;
    QHash<ToolType, QKeySequence> m_toolToShortcut;
    
    ToolManager* m_toolManager;
    DrawingScene* m_scene;
    ToolType m_previousTool;
    ToolType m_currentTemporaryTool;
    QTimer* m_temporaryTimer;
    bool m_temporaryModeEnabled;
    bool m_temporaryActive;
    
    static const int TEMPORARY_TOOL_TIMEOUT = 1000; // 1秒后恢复原工具
};

#endif // SHORTCUT_MANAGER_H