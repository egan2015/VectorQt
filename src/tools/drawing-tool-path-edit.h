#ifndef DRAWING_TOOL_PATH_EDIT_H
#define DRAWING_TOOL_PATH_EDIT_H

#include "../core/toolbase.h"
#include "../core/patheditor.h"
#include <QPointF>
#include <QUndoCommand>

class DrawingPath;
class DrawingShape;
class PathOperationCommand;

/**
 * 路径编辑工具 - 处理复杂路径操作
 */
class DrawingToolPathEdit : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingToolPathEdit(QObject *parent = nullptr);
    ~DrawingToolPathEdit();
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 设置操作类型
    enum EditMode {
        SelectMode,         // 选择模式
        BooleanMode,        // 布尔运算模式
        TransformMode       // 变换模式
    };
    
    void setEditMode(EditMode mode) { m_editMode = mode; }
    EditMode editMode() const { return m_editMode; }
    
    // 设置布尔运算类型
    void setBooleanOperation(PathEditor::BooleanOperation op) { m_booleanOp = op; }
    PathEditor::BooleanOperation booleanOperation() const { return m_booleanOp; }

private:
    // 执行路径操作
    void executePathOperation();
    void showContextMenu(const QPointF &scenePos);
    void showTemporaryMessage(const QString &message, const QPointF &pos);
    void hideTemporaryMessage();
    void updateSelectedPathsFromScene();
    
private slots:
    void onObjectStateChanged(DrawingShape* shape); // 处理对象状态变化

private:
    EditMode m_editMode;
    PathEditor::BooleanOperation m_booleanOp;
    PathEditor *m_pathEditor;
    QList<DrawingShape*> m_selectedPaths;
    QPointF m_lastPos;
    bool m_isDragging;
};

#endif // DRAWING_TOOL_PATH_EDIT_H