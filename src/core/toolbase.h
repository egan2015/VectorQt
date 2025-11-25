#ifndef TOOLBASE_H
#define TOOLBASE_H

#include <QObject>
#include <QPointF>
#include <QGraphicsItem>
#include "../ui/cursor-manager.h"

// Qt命名空间中的前置声明
QT_BEGIN_NAMESPACE
class QMouseEvent;
class QKeyEvent;
QT_END_NAMESPACE

class DrawingScene;
class DrawingView;
class DrawingRectangle;
class DrawingEllipse;
class DrawingShape;

/**
 * 工具状态枚举
 */
enum class ToolState {
    Inactive,    // 未激活
    Activating,  // 激活中
    Active,      // 已激活
    Deactivating,// 停用中
    Error        // 错误状态
};

class ToolBase : public QObject
{
    Q_OBJECT

public:
    explicit ToolBase(QObject *parent = nullptr);
    virtual ~ToolBase() = default;
    
    // 工具生命周期管理
    virtual void activate(DrawingScene *scene, DrawingView *view);
    virtual void deactivate();
    virtual bool canDeactivate() const { return true; }
    virtual void cleanup();
    
    // 状态管理
    ToolState state() const { return m_state; }
    void setState(ToolState state);
    bool isActive() const { return m_state == ToolState::Active; }
    bool isActivating() const { return m_state == ToolState::Activating; }
    
    // 工具属性
    virtual QString toolName() const { return "Unknown Tool"; }
    virtual QString toolDescription() const { return ""; }
    virtual QString toolCategory() const { return "General"; }
    virtual bool isTemporary() const { return false; } // 是否为临时工具
    
    // 获取工具对应的光标类型
    virtual CursorManager::CursorType getCursorType() const { return CursorManager::DefaultCursor; }
    
    virtual bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos);
    virtual bool keyPressEvent(QKeyEvent *event);
    
    DrawingScene* scene() const { return m_scene; }
    DrawingView* view() const { return m_view; }
    
    // 设置工具光标
    void setToolCursor();
    
    // 恢复默认光标
    void restoreDefaultCursor();

signals:
    // 形状创建完成信号
    void shapeFinished(DrawingShape *shape);
    
    // 状态变化信号
    void stateChanged(ToolState oldState, ToolState newState);
    void activated();
    void deactivated();
    void errorOccurred(const QString& error);

protected:
    // 生命周期钩子方法
    virtual void onActivated() {}
    virtual void onDeactivated() {}
    virtual void onStateChanged(ToolState oldState, ToolState newState) {}
    
    // 智能吸附辅助函数
    QPointF smartSnap(const QPointF &scenePos, DrawingShape *excludeShape = nullptr);
    
    DrawingScene *m_scene;
    DrawingView *m_view;
    ToolState m_state;
};



class LegacyRectangleTool : public ToolBase
{
    Q_OBJECT

public:
    explicit LegacyRectangleTool(QObject *parent = nullptr);
    virtual ~LegacyRectangleTool();
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::RectangleCursor; }

private:
    bool m_drawing;
    QPointF m_startPos;
    DrawingRectangle *m_previewItem;
    DrawingRectangle *m_currentItem;  // 当前正在创建的矩形
};

class LegacyEllipseTool : public ToolBase
{
    Q_OBJECT

public:
    explicit LegacyEllipseTool(QObject *parent = nullptr);
    virtual ~LegacyEllipseTool();
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::EllipseCursor; }

private:
    bool m_drawing;
    QPointF m_startPos;
    DrawingEllipse *m_previewItem;
    DrawingEllipse *m_currentItem;  // 当前正在创建的椭圆
};

#endif // TOOLBASE_H
