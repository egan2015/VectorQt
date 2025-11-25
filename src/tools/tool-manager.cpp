#include <QDebug>
#include <QMetaObject>
#include "tool-manager.h"

// 工具名称映射
const QHash<ToolType, QString> ToolManager::s_toolNames = {
    {ToolType::Select, "选择"},
    {ToolType::Rectangle, "矩形"},
    {ToolType::Ellipse, "椭圆"},
    {ToolType::Bezier, "贝塞尔"},
    {ToolType::NodeEdit, "节点编辑"},
    {ToolType::Polyline, "折线"},
    {ToolType::Polygon, "多边形"},
    {ToolType::Brush, "画笔"},
    {ToolType::Fill, "填充"},
    {ToolType::GradientFill, "渐变填充"},
    {ToolType::Pen, "钢笔"},
    {ToolType::Eraser, "橡皮擦"},
    {ToolType::Line, "直线"},
    {ToolType::PathEdit, "路径编辑"},
    {ToolType::Text, "文本"},
    {ToolType::Unknown, "未知"}
};

ToolManager::ToolManager(QObject *parent)
    : QObject(parent)
    , m_currentTool(nullptr)
    , m_currentToolType(ToolType::Unknown)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_toolStateManager(nullptr)
{
    // 初始化性能指标
    m_performanceMetrics.lastSwitchTime = 0;
    m_performanceMetrics.totalSwitches = 0;
    m_performanceMetrics.totalTime = 0;
}

ToolManager::~ToolManager()
{
    // 显式删除所有工具，确保在场景析构前完成
    qDebug() << "Cleaning up tools...";
    for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
        delete it.value();  // 手动删除工具对象
    }
    m_tools.clear();
    qDebug() << "Tools cleaned up.";
}



void ToolManager::unregisterTool(ToolType type)
{
    auto it = m_tools.find(type);
    if (it != m_tools.end()) {
        // 如果是当前工具，先切换到未知工具
        if (m_currentToolType == type) {
            switchTool(ToolType::Unknown);
        }
        
        m_tools.erase(it);
        
        #ifdef QT_DEBUG
        qDebug() << "ToolManager: Unregistered tool" << getToolTypeName(type);
        #endif
    }
}

bool ToolManager::hasTool(ToolType type) const
{
    return m_tools.contains(type);
}

bool ToolManager::switchTool(ToolType type)
{
    auto it = m_tools.find(type);
    if (it == m_tools.end()) {
        QString reason = QString("Tool %1 not registered").arg(getToolTypeName(type));
        emit toolSwitchFailed(type, reason);
        return false;
    }
    
    return switchTool(it.value());
}

bool ToolManager::switchTool(ToolBase* tool)
{
    if (!tool) {
        emit toolSwitchFailed(ToolType::Unknown, "Null tool provided");
        return false;
    }
    
    // 开始性能计时
    m_switchTimer.start();
    
    // 查找工具类型
    ToolType newType = ToolType::Unknown;
    for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
        if (it.value() == tool) {
            newType = it.key();
            break;
        }
    }
    
    // 保存当前工具状态
    if (m_currentTool && m_toolStateManager) {
        m_toolStateManager->saveToolState(m_currentTool, "hasSelection", 
            m_scene ? m_scene->selectedItems().count() > 0 : false);
        m_toolStateManager->saveToolState(m_currentTool, "lastUsed", QDateTime::currentDateTime());
    }
    
    // 停用当前工具
    ToolBase* oldTool = m_currentTool.data();
    ToolType oldType = m_currentToolType;
    
    if (m_currentTool) {
        m_currentTool->deactivate();
    }
    
    // 结束任何进行中的变换操作
    if (m_scene) {
        m_scene->endTransform();
    }
    
    // 激活新工具
    updateCurrentTool(tool, newType);
    
    if (m_currentTool) {
        // 恢复工具状态
        if (m_toolStateManager) {
            bool hadSelection = m_toolStateManager->restoreToolState(m_currentTool, "hasSelection", false).toBool();
        }
        
        m_currentTool->activate(m_scene, m_view);
        
        // 更新视图
        if (m_view) {
            m_view->setCurrentTool(m_currentTool);
        }
    }
    
    // 记录性能指标
    recordPerformanceMetrics(m_switchTimer.elapsed());
    
    // 发出信号
    emit toolChanged(oldTool, m_currentTool.data());
    emit toolTypeChanged(oldType, m_currentToolType);
    
    #ifdef QT_DEBUG
    if (m_switchTimer.elapsed() > 5) {
        qDebug() << "ToolManager: Tool switch took" << m_switchTimer.elapsed() << "ms";
    }
    #endif
    
    return true;
}

void ToolManager::setScene(DrawingScene* scene)
{
    m_scene = scene;
}

void ToolManager::setView(DrawingView* view)
{
    m_view = view;
}

void ToolManager::setToolStateManager(ToolStateManager* stateManager)
{
    m_toolStateManager = stateManager;
}

void ToolManager::resetPerformanceMetrics()
{
    m_performanceMetrics.lastSwitchTime = 0;
    m_performanceMetrics.totalSwitches = 0;
    m_performanceMetrics.totalTime = 0;
}

ToolBase* ToolManager::getTool(ToolType type) const
{
    auto it = m_tools.find(type);
    return (it != m_tools.end()) ? it.value() : nullptr;
}



QList<ToolType> ToolManager::availableTools() const
{
    QList<ToolType> tools;
    for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
        tools.append(it.key());
    }
    return tools;
}

QString ToolManager::toolName(ToolType type) const
{
    return getToolTypeName(type);
}

void ToolManager::onToolActivationComplete()
{
    // 工具激活完成后的处理
    #ifdef QT_DEBUG
    qDebug() << "ToolManager: Tool activation completed";
    #endif
}

void ToolManager::onToolDeactivationComplete()
{
    // 工具停用完成后的处理
    #ifdef QT_DEBUG
    qDebug() << "ToolManager: Tool deactivation completed";
    #endif
}

void ToolManager::updateCurrentTool(ToolBase* newTool, ToolType type)
{
    m_currentTool = newTool;
    m_currentToolType = type;
}

void ToolManager::recordPerformanceMetrics(qint64 switchTime)
{
    m_performanceMetrics.lastSwitchTime = switchTime;
    m_performanceMetrics.totalSwitches++;
    m_performanceMetrics.totalTime += switchTime;
}

QString ToolManager::getToolTypeName(ToolType type) const
{
    return s_toolNames.value(type, "Unknown");
}