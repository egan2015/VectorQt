#ifndef TOOL_SWITCH_STATE_MACHINE_H
#define TOOL_SWITCH_STATE_MACHINE_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include "../tools/tool-manager.h"

/**
 * 工具切换状态机 - 基于鼠标操作的工具快速切换
 * 
 * 状态机逻辑：
 * 1. 双击空白区域：当前工具 ↔ 选择工具
 * 2. 双击图形：当前工具 ↔ 节点编辑工具
 * 3. 双击手柄：当前工具 ↔ 选择工具
 */
class ToolSwitchStateMachine : public QObject
{
    Q_OBJECT

public:
    enum class SwitchState {
        Idle,           // 空闲状态
        FirstClick,     // 第一次点击
        DoubleClick     // 双击检测
    };
    
    enum class ClickTarget {
        EmptySpace,     // 空白区域
        Shape,          // 图形对象
        Handle          // 控制手柄
    };

    explicit ToolSwitchStateMachine(ToolManager* toolManager, QObject *parent = nullptr);
    
    // 鼠标事件处理
    bool handleMousePress(QMouseEvent* event, const QPointF& scenePos, QGraphicsItem* item = nullptr);
    bool handleMouseRelease(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event, const QPointF& scenePos);
    
    // 配置选项
    void setDoubleClickInterval(int ms) { m_doubleClickInterval = ms; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    // 状态查询
    SwitchState currentState() const { return m_state; }
    bool isEnabled() const { return m_enabled; }
    
    // 公共方法：手动触发工具切换（用于双击事件）
    void triggerToolSwitch(ClickTarget target);

signals:
    void toolSwitchRequested(ToolType newTool);
    void stateChanged(SwitchState newState);
    void shortcutConflict(ToolType tool1, ToolType tool2, const QKeySequence& shortcut);

private slots:
    void onDoubleClickTimer();

private:
    void setState(SwitchState newState);
    ClickTarget determineClickTarget(const QPointF& scenePos, QGraphicsItem* item) const;
    void performToolSwitch(ClickTarget target);
    
    // 状态机组件
    ToolManager* m_toolManager;
    SwitchState m_state;
    ClickTarget m_lastTarget;
    
    // 定时器
    QTimer* m_doubleClickTimer;
    
    // 状态数据
    QDateTime m_firstClickTime;
    QPointF m_firstClickPos;
    ToolType m_previousTool;
    ToolType m_originalTool;  // 原始工具，用于空白点击时恢复
    int m_clickCount;
    
    // 配置参数
    int m_doubleClickInterval;
    bool m_enabled;
    
    // 状态标志
    bool m_isDragging;
    bool m_hasMoved;
    
    // 初始化标志
    bool m_originalToolInitialized;
};

#endif // TOOL_SWITCH_STATE_MACHINE_H