#include "tool-state-manager.h"
#include <QDateTime>
#include <QDebug>

ToolStateManager::ToolStateManager(QObject *parent)
    : QObject(parent)
{
}

void ToolStateManager::saveToolState(ToolBase* tool, const QString& key, const QVariant& value)
{
    if (!tool || key.isEmpty()) {
        return;
    }
    
    const QString toolId = getToolId(tool);
    ToolState& state = m_toolStates[toolId];
    state.properties[key] = value;
    state.lastUsed = QDateTime::currentDateTime();
}

QVariant ToolStateManager::restoreToolState(ToolBase* tool, const QString& key, const QVariant& defaultValue) const
{
    if (!tool || key.isEmpty()) {
        return defaultValue;
    }
    
    const QString toolId = getToolId(tool);
    const auto it = m_toolStates.find(toolId);
    if (it != m_toolStates.constEnd()) {
        const auto propIt = it->properties.find(key);
        if (propIt != it->properties.constEnd()) {
            return propIt.value();
        }
    }
    
    return defaultValue;
}

bool ToolStateManager::hasToolState(ToolBase* tool, const QString& key) const
{
    if (!tool || key.isEmpty()) {
        return false;
    }
    
    const QString toolId = getToolId(tool);
    const auto it = m_toolStates.find(toolId);
    if (it != m_toolStates.constEnd()) {
        return it->properties.contains(key);
    }
    
    return false;
}

void ToolStateManager::clearToolState(ToolBase* tool)
{
    if (!tool) {
        return;
    }
    
    const QString toolId = getToolId(tool);
    m_toolStates.remove(toolId);
}

void ToolStateManager::clearAllStates()
{
    m_toolStates.clear();
}

QStringList ToolStateManager::getToolStateKeys(ToolBase* tool) const
{
    if (!tool) {
        return QStringList();
    }
    
    const QString toolId = getToolId(tool);
    const auto it = m_toolStates.find(toolId);
    if (it != m_toolStates.constEnd()) {
        return it->properties.keys();
    }
    
    return QStringList();
}

QString ToolStateManager::getToolId(ToolBase* tool) const
{
    if (!tool) {
        return QString();
    }
    
    // 使用工具的类型名称和内存地址生成唯一ID
    return QString("%1_%2").arg(tool->metaObject()->className()).arg(quintptr(tool));
}