#ifndef TOOL_MANAGER_H
#define TOOL_MANAGER_H

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QElapsedTimer>
#include <memory>
#include "tool-state-manager.h"
#include "../core/toolbase.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"

// 前向声明工具类
class ToolBase;
class OutlinePreviewTransformTool;
class LegacyRectangleTool;
class LegacyEllipseTool;
class DrawingBezierTool;
class DrawingNodeEditTool;
class DrawingToolBrush;
class DrawingToolPen;
class DrawingToolFill;

class DrawingScene;
class DrawingView;
class ToolStateManager;

/**
 * 工具类型枚举
 */
enum class ToolType {
    Select,           // 选择工具
    Rectangle,        // 矩形工具
    Ellipse,          // 椭圆工具
    Bezier,           // 贝塞尔工具
    NodeEdit,         // 节点编辑工具
    Polyline,         // 折线工具
    Polygon,          // 多边形工具
    Brush,            // 画笔工具
    Fill,             // 填充工具
    GradientFill,     // 渐变填充工具
    Pen,              // 钢笔工具
    Eraser,           // 橡皮擦工具
    Line,             // 直线工具
    PathEdit,         // 路径编辑工具
    Text,             // 文本工具
    Unknown           // 未知工具
};

/**
 * 工具管理器 - 负责工具的注册、切换和生命周期管理
 * 解耦工具切换逻辑，提供统一的工具管理接口
 */
class ToolManager : public QObject
{
    Q_OBJECT

public:
    explicit ToolManager(QObject *parent = nullptr);
    ~ToolManager();

    // 工具创建和管理
    template<typename T>
    void createAndRegisterTool(ToolType type, QObject* parent = nullptr);
    void unregisterTool(ToolType type);
    bool hasTool(ToolType type) const;
    
    // 工具切换
    bool switchTool(ToolType type);
    bool switchTool(ToolBase* tool);
    ToolBase* currentTool() const { return m_currentTool.data(); }
    ToolType currentToolType() const { return m_currentToolType; }
    
    // 场景和视图设置
    void setScene(DrawingScene* scene);
    void setView(DrawingView* view);
    DrawingScene* scene() const { return m_scene; }
    DrawingView* view() const { return m_view; }
    
    // 工具状态管理
    void setToolStateManager(ToolStateManager* stateManager);
    ToolStateManager* toolStateManager() const { return m_toolStateManager; }
    
    // 性能监控
    struct PerformanceMetrics {
        qint64 lastSwitchTime;      // 最后一次切换耗时
        int totalSwitches;          // 总切换次数
        qint64 totalTime;           // 总耗时
        double averageTime() const { return totalSwitches > 0 ? static_cast<double>(totalTime) / totalSwitches : 0.0; }
    };
    
    const PerformanceMetrics& performanceMetrics() const { return m_performanceMetrics; }
    void resetPerformanceMetrics();
    
    // 工具查询
    ToolBase* getTool(ToolType type) const;
    QList<ToolType> availableTools() const;
    QString toolName(ToolType type) const;
    
    // 工具工厂方法
    template<typename T>
    T* getToolAs(ToolType type) const;

signals:
    void toolChanged(ToolBase* oldTool, ToolBase* newTool);
    void toolTypeChanged(ToolType oldType, ToolType newType);
    void toolSwitchFailed(ToolType type, const QString& reason);

private slots:
    void onToolActivationComplete();
    void onToolDeactivationComplete();

private:
    void updateCurrentTool(ToolBase* newTool, ToolType type);
    void recordPerformanceMetrics(qint64 switchTime);
    QString getToolTypeName(ToolType type) const;

    // 工具存储 - 使用普通指针，ToolManager负责内存管理
    QHash<ToolType, ToolBase*> m_tools;
    
    // 当前工具状态
    QPointer<ToolBase> m_currentTool;
    ToolType m_currentToolType;
    
    // 场景和视图引用
    QPointer<DrawingScene> m_scene;
    QPointer<DrawingView> m_view;
    
    // 状态管理器
    QPointer<ToolStateManager> m_toolStateManager;
    
    // 性能监控
    PerformanceMetrics m_performanceMetrics;
    QElapsedTimer m_switchTimer;
    
    // 工具名称映射
    static const QHash<ToolType, QString> s_toolNames;
};

// 模板实现必须在头文件中
template<typename T>
void ToolManager::createAndRegisterTool(ToolType type, QObject* parent)
{
    if (hasTool(type)) {
        qWarning() << "ToolManager: Tool" << getToolTypeName(type) << "already registered";
        return;
    }
    
    T* tool = new T(parent);
    m_tools[type] = tool;
    
    #ifdef QT_DEBUG
    qDebug() << "ToolManager: Created and registered tool" << getToolTypeName(type) 
             << "at" << tool;
    #endif
}

template<typename T>
T* ToolManager::getToolAs(ToolType type) const
{
    ToolBase* tool = getTool(type);
    return qobject_cast<T*>(tool);
}

#endif // TOOL_MANAGER_H