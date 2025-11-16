#ifndef DRAWING_TOOL_BEZIER_EDIT_H
#define DRAWING_TOOL_BEZIER_EDIT_H

#include "toolbase.h"
#include <QVector>
#include <QPointF>

class DrawingScene;
class DrawingPath;
class EditHandle;

/**
 * 贝塞尔曲线控制点编辑工具
 * 允许用户调整已绘制贝塞尔曲线的控制点位置
 */
class DrawingBezierEditTool : public ToolBase
{
    Q_OBJECT

public:
    explicit DrawingBezierEditTool(QObject *parent = nullptr);
    ~DrawingBezierEditTool() override;

    // 事件处理 - 重写基类方法
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    
    // 激活/停用
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;

private:
    // 创建控制点把手
    void createControlPointHandles();
    
    // 更新控制点把手位置
    void updateControlPointHandles();
    
    // 清理控制点把手
    void cleanupControlPointHandles();
    
private slots:
    void onObjectStateChanged(DrawingShape* shape); // 处理对象状态变化

private:
    // 状态变量
    DrawingPath *m_selectedPath;  // 当前选中的路径
    QVector<EditHandle*> m_controlPointHandles;  // 控制点把手
    EditHandle *m_activeHandle;  // 当前活动的把手
    QVector<QPointF> m_originalControlPoints;  // 原始控制点位置
    bool m_isDragging;  // 是否正在拖动
};

#endif // DRAWING_TOOL_BEZIER_EDIT_H