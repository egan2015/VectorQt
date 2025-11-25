#ifndef TOOL_STATE_MANAGER_H
#define TOOL_STATE_MANAGER_H

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QVariant>
#include <QDateTime>
#include "../core/toolbase.h"

/**
 * 工具状态管理器 - 负责保存和恢复工具状态
 * 用于优化工具切换性能，避免重复初始化
 */
class ToolStateManager : public QObject
{
    Q_OBJECT

public:
    explicit ToolStateManager(QObject *parent = nullptr);
    ~ToolStateManager() = default;

    // 保存工具状态
    void saveToolState(ToolBase* tool, const QString& key, const QVariant& value);
    
    // 恢复工具状态
    QVariant restoreToolState(ToolBase* tool, const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    // 检查工具是否有保存的状态
    bool hasToolState(ToolBase* tool, const QString& key) const;
    
    // 清除指定工具的状态
    void clearToolState(ToolBase* tool);
    
    // 清除所有工具状态
    void clearAllStates();
    
    // 获取工具的所有状态键
    QStringList getToolStateKeys(ToolBase* tool) const;

private:
    // 生成工具的唯一标识
    QString getToolId(ToolBase* tool) const;
    
    // 工具状态存储结构
    struct ToolState {
        QHash<QString, QVariant> properties;
        QDateTime lastUsed;
    };
    
    QHash<QString, ToolState> m_toolStates;
};

#endif // TOOL_STATE_MANAGER_H