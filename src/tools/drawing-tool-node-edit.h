#ifndef DRAWING_TOOL_NODE_EDIT_H
#define DRAWING_TOOL_NODE_EDIT_H

#include <QPointF>
#include <QGraphicsItem>
#include <QUndoCommand>
#include "../core/toolbase.h"

class DrawingScene;
class DrawingView;
class DrawingShape;
class CustomHandleItem;
class NodeHandleManager;

/**
 * 节点编辑撤销命令
 */
class NodeEditCommand : public QUndoCommand
{
public:
    NodeEditCommand(DrawingScene *scene, DrawingShape *shape, int nodeIndex, 
                   const QPointF &oldPos, const QPointF &newPos, 
                   qreal oldCornerRadius = -1.0, qreal newCornerRadius = -1.0,
                   bool skipInitialRedo = false, QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    
private:
    DrawingScene *m_scene;
    DrawingShape *m_shape;
    int m_nodeIndex;
    QPointF m_oldPos;
    QPointF m_newPos;
    qreal m_oldCornerRadius;  // 原始圆角半径（-1表示不适用）
    qreal m_newCornerRadius;  // 新圆角半径（-1表示不适用）
    bool m_skipInitialRedo;   // 是否跳过初始redo（避免二次变换）
    bool m_hasSkippedFirstRedo; // 是否已经跳过第一次redo
};

/**
 * 节点编辑工具 - 用于编辑图形的内部属性
 * 参考Inkscape的节点工具设计
 */
class DrawingNodeEditTool : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingNodeEditTool(QObject *parent = nullptr);
    ~DrawingNodeEditTool() override;

    // 工具属性
    QString toolName() const override { return "节点编辑"; }
    QString toolDescription() const override { return "编辑图形节点和控制点"; }
    QString toolCategory() const override { return "编辑"; }

    // 事件处理 - 重写基类方法
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::NodeEditCursor; }
    
    // 激活/停用
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;

private:
    // 内部方法
    void updateNodeHandles();
    void updateOtherNodeHandles(int draggedIndex, const QPointF &draggedPos);  // 更新除拖动手柄外的其他手柄
    void clearNodeHandles();
    void onSceneSelectionChanged(); // 处理场景选择变化
    void onObjectStateChanged(DrawingShape* shape); // 处理对象状态变化
    
    // 状态变量
    DrawingShape *m_selectedShape;  // 当前选中的形状
    CustomHandleItem *m_activeHandle;     // 当前激活的编辑手柄
    bool m_dragging;                // 是否正在拖动
    QPointF m_dragStartPos;         // 拖动起始位置
    QPointF m_originalValue;        // 原始值（用于撤销）
    qreal m_originalCornerRadius;   // 原始圆角半径（用于撤销）
    NodeHandleManager *m_handleManager; // 节点手柄管理器
};

#endif // DRAWING_TOOL_NODE_EDIT_H