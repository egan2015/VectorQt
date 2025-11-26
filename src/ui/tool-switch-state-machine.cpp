#include "tool-switch-state-machine.h"
#include <QMouseEvent>
#include <QGraphicsItem>
#include <QDebug>
#include "../core/drawing-shape.h"

ToolSwitchStateMachine::ToolSwitchStateMachine(ToolManager* toolManager, QObject *parent)
    : QObject(parent)
    , m_toolManager(toolManager)
    , m_state(SwitchState::Idle)
    , m_lastTarget(ClickTarget::EmptySpace)
    , m_previousTool(ToolType::Unknown)
    , m_originalTool(ToolType::Unknown)
    , m_clickCount(0)
    , m_doubleClickInterval(500)  // 增加双击间隔到500ms
    , m_enabled(true)
    , m_isDragging(false)
    , m_hasMoved(false)
    , m_originalToolInitialized(false)
{
    
    
    // 初始化定时器
    m_doubleClickTimer = new QTimer(this);
    m_doubleClickTimer->setSingleShot(true);
    connect(m_doubleClickTimer, &QTimer::timeout, this, &ToolSwitchStateMachine::onDoubleClickTimer);
    
    if (m_toolManager) {
        m_previousTool = m_toolManager->currentToolType();
        m_originalTool = m_toolManager->currentToolType();
        m_originalToolInitialized = true;
    }
}

bool ToolSwitchStateMachine::handleMousePress(QMouseEvent* event, const QPointF& scenePos, QGraphicsItem* item)
{
    if (!m_enabled || !m_toolManager) {
        return false;
    }
    
    // 只处理左键
    if (event->button() != Qt::LeftButton) {
        return false;
    }
    
    // 确定点击目标
    ClickTarget target = determineClickTarget(scenePos, item);
    m_lastTarget = target;
    
    switch (m_state) {
    case SwitchState::Idle:
        // 第一次点击
        setState(SwitchState::FirstClick);
        m_firstClickTime = QDateTime::currentDateTime();
        m_firstClickPos = scenePos;
        m_clickCount = 1;
        m_isDragging = false;
        m_hasMoved = false;
        
        // 如果点击空白，直接切换回原始工具
        if (target == ClickTarget::EmptySpace) {
            qDebug() << "Single click on empty space, switching back to original tool";
            performToolSwitch(target);
            setState(SwitchState::Idle);
        } else {
            // 点击图形，启动双击定时器
            m_doubleClickTimer->start(m_doubleClickInterval);
        }
        break;
        
    case SwitchState::FirstClick:
        // 第二次点击 - 检测双击
        if (m_clickCount == 1) {
            qint64 elapsed = m_firstClickTime.msecsTo(QDateTime::currentDateTime());
            qreal distance = QLineF(m_firstClickPos, scenePos).length();
            
            if (elapsed < m_doubleClickInterval && distance < 10.0) {
                // 双击检测成功
                m_clickCount = 2;
                setState(SwitchState::DoubleClick);
                performToolSwitch(target);
                return true;
            } else {
                // 超时或距离太远，重置为第一次点击
                m_firstClickTime = QDateTime::currentDateTime();
                m_firstClickPos = scenePos;
                m_clickCount = 1;
                m_hasMoved = false;
                
                // 重启定时器
                m_doubleClickTimer->start(m_doubleClickInterval);
            }
        }
        break;
        
    default:
        break;
    }
    
    return false;
}

bool ToolSwitchStateMachine::handleMouseRelease(QMouseEvent* event)
{
    if (!m_enabled || !m_toolManager) {
        return false;
    }
    
    if (event->button() != Qt::LeftButton) {
        return false;
    }
    
    switch (m_state) {
    case SwitchState::FirstClick:
        // 单击完成，等待双击超时
        qDebug() << "MouseRelease: FirstClick state, hasMoved=" << m_hasMoved << " timerActive=" << m_doubleClickTimer->isActive() << " target=" << (int)m_lastTarget;
        if (!m_hasMoved && !m_doubleClickTimer->isActive()) {
            // 确认是单击，执行工具切换逻辑
            qDebug() << "Confirmed single click, calling performToolSwitch";
            performToolSwitch(m_lastTarget);
            setState(SwitchState::Idle);
        }
        break;
        
    case SwitchState::DoubleClick:
        // 双击完成
        setState(SwitchState::Idle);
        break;
        
    default:
        break;
    }
    
    // 停止定时器
    m_doubleClickTimer->stop();
    
    return false;
}

bool ToolSwitchStateMachine::handleMouseMove(QMouseEvent* event, const QPointF& scenePos)
{
    if (!m_enabled || !m_toolManager) {
        return false;
    }
    
    switch (m_state) {
    case SwitchState::FirstClick:
        // 检测是否开始拖拽
        if (!m_hasMoved) {
            qreal distance = QLineF(m_firstClickPos, scenePos).length();
            
            if (distance > 5.0) {
                m_hasMoved = true;
                m_isDragging = true;
                m_doubleClickTimer->stop(); // 取消双击检测
                setState(SwitchState::Idle);
            }
        }
        break;
        
    default:
        break;
    }
    
    return false;
}

void ToolSwitchStateMachine::onDoubleClickTimer()
{
    // 定时器超时，什么都不做，等待 mouseRelease 中的处理
    qDebug() << "onDoubleClickTimer timeout, state=" << (int)m_state;
}



void ToolSwitchStateMachine::setState(SwitchState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

ToolSwitchStateMachine::ClickTarget ToolSwitchStateMachine::determineClickTarget(const QPointF& scenePos, QGraphicsItem* item) const
{
    if (item) {
        // 检查是否是图形对象
        DrawingShape* shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            return ClickTarget::Shape;
        }
        
        // 检查是否是控制手柄
        if (item->type() >= QGraphicsItem::UserType) {
            return ClickTarget::Handle;
        }
    }
    
    return ClickTarget::EmptySpace;
}

void ToolSwitchStateMachine::performToolSwitch(ClickTarget target)
{
    if (!m_toolManager) {
        return;
    }
    
    ToolType currentTool = m_toolManager->currentToolType();
    ToolType newTool = ToolType::Unknown;
    
    // 初始化原始工具（如果还没有初始化）
    if (!m_originalToolInitialized) {
        m_originalTool = currentTool;
        m_originalToolInitialized = true;
    }
    
    switch (target) {
    case ClickTarget::Shape:
        // 双击图形：节点编辑 ↔ 选择工具
        // 如果当前不是节点编辑或选择工具，先更新原始工具
        if (currentTool != ToolType::NodeEdit && currentTool != ToolType::Select) {
            m_originalTool = currentTool;
        }
        
        if (currentTool != ToolType::NodeEdit) {
            newTool = ToolType::NodeEdit;
        } else {
            newTool = ToolType::Select;
        }
        break;
        
    case ClickTarget::EmptySpace:
        // 点击空白：直接回到原始工具
        newTool = m_originalTool;
        break;
        
    default:
        // 双击手柄：不做任何操作，保持当前工具
        break;
    }
    
    // 调试信息
    qDebug() << "ToolSwitch: current=" << (int)currentTool << " original=" << (int)m_originalTool << " new=" << (int)newTool;
    
    if (newTool != ToolType::Unknown) {
        // 保存当前工具作为上一个工具
        m_previousTool = currentTool;
        
        // 发出工具切换请求
        emit toolSwitchRequested(newTool);
    }
}



void ToolSwitchStateMachine::triggerToolSwitch(ClickTarget target)
{
    performToolSwitch(target);
}