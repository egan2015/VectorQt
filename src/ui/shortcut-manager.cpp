#include <QDebug>
#include <QCoreApplication>
#include "shortcut-manager.h"
#include "../tools/tool-manager.h"
ShortcutManager::ShortcutManager(QObject *parent)
    : QObject(parent)
    , m_toolManager(nullptr)
    , m_scene(nullptr)
    , m_previousTool(ToolType::Unknown)
    , m_currentTemporaryTool(ToolType::Unknown)
    , m_temporaryTimer(new QTimer(this))
    , m_temporaryModeEnabled(true)
    , m_temporaryActive(false)
{
    // 设置临时工具定时器
    m_temporaryTimer->setSingleShot(true);
    connect(m_temporaryTimer, &QTimer::timeout, this, &ShortcutManager::onTemporaryTimeout);
}

void ShortcutManager::setToolManager(ToolManager* toolManager)
{
    m_toolManager = toolManager;
}

void ShortcutManager::setScene(DrawingScene* scene)
{
    m_scene = scene;
}

void ShortcutManager::registerShortcut(ToolType toolType, const QKeySequence& shortcut, QAction* action, ShortcutContext context)
{
    if (shortcut.isEmpty()) {
        return;
    }
    
    // 检查冲突
    checkConflicts(toolType, shortcut, false);
    
    // 移除该工具的旧快捷键
    QKeySequence oldShortcut = m_toolToShortcut.value(toolType);
    if (!oldShortcut.isEmpty()) {
        m_shortcuts.remove(oldShortcut);
    }
    
    // 注册快捷键
    ShortcutInfo info;
    info.toolType = toolType;
    info.shortcut = shortcut;
    info.action = action;
    info.isTemporary = false;
    info.context = context;
    
    m_shortcuts[shortcut] = info;
    m_toolToShortcut[toolType] = shortcut;
    
    #ifdef QT_DEBUG
    qDebug() << "ShortcutManager: Registered permanent shortcut" << shortcut.toString() 
             << "for tool" << static_cast<int>(toolType) << "with context" << static_cast<int>(context);
    #endif
}

void ShortcutManager::registerTemporaryShortcut(ToolType toolType, const QKeySequence& shortcut, ShortcutContext context)
{
    if (shortcut.isEmpty()) {
        return;
    }
    
    // 检查冲突
    checkConflicts(toolType, shortcut, true);
    
    // 移除该工具的旧临时快捷键
    auto it = m_shortcuts.begin();
    while (it != m_shortcuts.end()) {
        if (it.value().isTemporary && it.value().toolType == toolType) {
            it = m_shortcuts.erase(it);
        } else {
            ++it;
        }
    }
    
    // 注册临时快捷键
    ShortcutInfo info;
    info.toolType = toolType;
    info.shortcut = shortcut;
    info.action = nullptr;
    info.isTemporary = true;
    info.context = context;
    
    m_shortcuts[shortcut] = info;
    
    #ifdef QT_DEBUG
    qDebug() << "ShortcutManager: Registered temporary shortcut" << shortcut.toString()
             << "for tool" << static_cast<int>(toolType) << "with context" << static_cast<int>(context);
    #endif
}

bool ShortcutManager::handleKeyPress(const QKeySequence& key)
{
    if (!m_toolManager || key.isEmpty()) {
        return false;
    }
    
    auto it = m_shortcuts.find(key);
    if (it == m_shortcuts.end()) {
        return false;
    }
    
    const ShortcutInfo& info = it.value();
    
    // 检查上下文是否有效
    if (!isContextValid(info.context)) {
        return false;
    }
    
    if (info.isTemporary && m_temporaryModeEnabled) {
        // 检查是否有相同按键的永久快捷键
        auto permanentIt = m_shortcuts.find(key);
        bool hasPermanent = false;
        if (permanentIt != m_shortcuts.end() && !permanentIt.value().isTemporary) {
            hasPermanent = true;
        }
        
        // 如果有永久快捷键冲突，优先使用临时快捷键（用户按住键时意图更明确）
        activateTemporaryTool(info.toolType);
        return true;
    } else if (!info.isTemporary) {
        // 永久工具切换 - 只有在没有临时工具激活时才切换
        if (!m_temporaryActive && m_toolManager->switchTool(info.toolType)) {
            return true;
        }
    }
    
    return false;
}

bool ShortcutManager::handleKeyRelease(const QKeySequence& key)
{
    // 对于临时工具，释放按键时恢复原工具
    if (m_temporaryActive && m_shortcuts.contains(key)) {
        const ShortcutInfo& info = m_shortcuts[key];
        if (info.isTemporary && info.toolType == m_currentTemporaryTool) {
            restorePreviousTool();
            return true;
        }
    }
    
    return false;
}

QKeySequence ShortcutManager::getShortcut(ToolType toolType) const
{
    return m_toolToShortcut.value(toolType);
}

bool ShortcutManager::setShortcut(ToolType toolType, const QKeySequence& shortcut)
{
    if (shortcut.isEmpty()) {
        return false;
    }
    
    // 检查冲突
    checkConflicts(toolType, shortcut, false);
    
    // 移除旧的快捷键
    QKeySequence oldShortcut = m_toolToShortcut.value(toolType);
    if (!oldShortcut.isEmpty()) {
        m_shortcuts.remove(oldShortcut);
    }
    
    // 设置新的快捷键
    m_toolToShortcut[toolType] = shortcut;
    
    // 更新对应的Action
    auto it = m_shortcuts.find(shortcut);
    if (it != m_shortcuts.end()) {
        it.value().toolType = toolType;
        if (it.value().action) {
            it.value().action->setShortcut(shortcut);
        }
    } else {
        // 创建新的快捷键信息
        ShortcutInfo info;
        info.toolType = toolType;
        info.shortcut = shortcut;
        info.action = nullptr;
        info.isTemporary = false;
        m_shortcuts[shortcut] = info;
    }
    
    return true;
}

void ShortcutManager::checkConflicts(ToolType toolType, const QKeySequence& shortcut, bool isTemporary)
{
    auto it = m_shortcuts.find(shortcut);
    if (it != m_shortcuts.end() && it.value().toolType != toolType) {
        const ShortcutInfo& existingInfo = it.value();
        
        // 永久快捷键之间不能冲突
        if (!isTemporary && !existingInfo.isTemporary) {
            emit shortcutConflict(toolType, existingInfo.toolType, shortcut);
            qWarning() << "Permanent shortcut conflict detected:" 
                       << "Tool" << static_cast<int>(toolType) 
                       << "and Tool" << static_cast<int>(existingInfo.toolType)
                       << "both use shortcut" << shortcut.toString();
        }
        // 临时快捷键优先级低于永久快捷键，但给出警告
        else if (isTemporary && !existingInfo.isTemporary) {
            qWarning() << "Temporary shortcut conflicts with permanent shortcut:" 
                       << "Temporary tool" << static_cast<int>(toolType) 
                       << "conflicts with permanent tool" << static_cast<int>(existingInfo.toolType)
                       << "using shortcut" << shortcut.toString()
                       << "- temporary will be ignored when permanent is active";
        }
        // 临时快捷键之间的冲突
        else if (isTemporary && existingInfo.isTemporary) {
            qWarning() << "Temporary shortcut conflict detected:" 
                       << "Tool" << static_cast<int>(toolType) 
                       << "and Tool" << static_cast<int>(existingInfo.toolType)
                       << "both use temporary shortcut" << shortcut.toString();
        }
    }
}

bool ShortcutManager::isContextValid(ShortcutContext context) const
{
    switch (context) {
    case ShortcutContext::Global:
        return true; // 全局快捷键总是有效
        
    case ShortcutContext::HasSelection:
        return m_scene && !m_scene->selectedItems().isEmpty();
        
    case ShortcutContext::NoSelection:
        return m_scene && m_scene->selectedItems().isEmpty();
        
    case ShortcutContext::NodeEditMode:
        return m_toolManager && m_toolManager->currentToolType() == ToolType::NodeEdit;
        
    case ShortcutContext::PathEditMode:
        return m_toolManager && m_toolManager->currentToolType() == ToolType::PathEdit;
        
    case ShortcutContext::TextEditMode:
        return m_toolManager && m_toolManager->currentToolType() == ToolType::Text;
        
    default:
        return false;
    }
}

void ShortcutManager::activateTemporaryTool(ToolType toolType)
{
    if (!m_toolManager) {
        return;
    }
    
    // 保存当前工具
    if (!m_temporaryActive) {
        m_previousTool = m_toolManager->currentToolType();
    }
    
    // 切换到临时工具
    if (m_toolManager->switchTool(toolType)) {
        m_currentTemporaryTool = toolType;
        m_temporaryActive = true;
        m_temporaryTimer->start(TEMPORARY_TOOL_TIMEOUT);
        
        emit temporaryToolActivated(toolType);
        
        #ifdef QT_DEBUG
        qDebug() << "ShortcutManager: Activated temporary tool" << static_cast<int>(toolType);
        #endif
    }
}

void ShortcutManager::restorePreviousTool()
{
    if (!m_toolManager || !m_temporaryActive) {
        return;
    }
    
    // 恢复之前的工具
    if (m_previousTool != ToolType::Unknown) {
        m_toolManager->switchTool(m_previousTool);
        
        emit temporaryToolDeactivated(m_currentTemporaryTool);
        
        #ifdef QT_DEBUG
        qDebug() << "ShortcutManager: Restored previous tool" << static_cast<int>(m_previousTool);
        #endif
    }
    
    m_temporaryActive = false;
    m_currentTemporaryTool = ToolType::Unknown;
}

void ShortcutManager::onTemporaryTimeout()
{
    restorePreviousTool();
}