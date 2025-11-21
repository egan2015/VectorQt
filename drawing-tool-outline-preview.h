#ifndef DRAWING_TOOL_OUTLINE_PREVIEW_H
#define DRAWING_TOOL_OUTLINE_PREVIEW_H

#include "toolbase.h"
#include "handle-item.h"
#include "drawing-transform.h"
#include "drawingscene.h"
#include "handle-types.h"
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QList>
#include <QGraphicsPathItem>
#include <QKeyEvent>
#include <QString>

class DrawingView;
class DrawingShape;
class HandleManager;

/**
 * @brief 基于轮廓预览的变换工具
 * @details 变换时显示虚线预览，结束后应用到真实对象
 */
class OutlinePreviewTransformTool : public ToolBase
{
    Q_OBJECT

public:
    explicit OutlinePreviewTransformTool(QObject *parent = nullptr);
    ~OutlinePreviewTransformTool() override;

    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool keyPressEvent(QKeyEvent *event) override; // 支持ESC取消
    
    CursorManager::CursorType getCursorType() const override { return CursorManager::SelectCursor; }

signals:
    void statusMessageChanged(const QString &message);

private slots:
    void onSelectionChanged();
    void onObjectStateChanged(DrawingShape* shape);
    void updateDashOffset();

private:
    void cleanupInvalidShapes(); // 清理无效的图形引用

private:
    // 状态机：仿 Inkscape grab()/transform()/ungrab()
    enum State {
        STATE_IDLE,
        STATE_GRABBED,      // 变换中
        STATE_DRAG_CENTER   // 拖动旋转中心
    };

    
    
    // 核心流程
    void grab(TransformHandle::HandleType handleType, const QPointF &mousePos, Qt::KeyboardModifiers modifiers);
    void transform(const QPointF &mousePos, Qt::KeyboardModifiers modifiers);
    void ungrab(bool apply = true, const QPointF &finalMousePos = QPointF()); // apply=false 时取消变换

    // 计算逻辑
    QPointF calculateOpposite(const QRectF &bounds, TransformHandle::HandleType type) const;
    QPointF calculateOrigin(const QRectF &bounds, const QPointF &opposite, 
                            Qt::KeyboardModifiers modifiers) const;
    QRectF calculateInitialSelectionBounds() const;
    void updateHandlePositions();
    void resetState();
    void disableInternalSelectionIndicators();
    void enableInternalSelectionIndicators();
    void createVisualHelpers();
    void destroyVisualHelpers();
    void updateVisualHelpers(const QPointF &mousePos);
    void updateOutlinePreview();

    // 旋转中心设置
    void setRotationCenter(const QPointF &center);
    void resetRotationCenter();
    bool hasCustomRotationCenter() const;
    
    // 模式管理
    void toggleMode();
    void setMode(HandleMode::Mode mode);
    HandleMode::Mode currentMode() const;

    // 状态
    State m_state = STATE_IDLE;
    TransformHandle::HandleType m_activeHandle = TransformHandle::None;
    
    // 变换基准数据（grab 时保存，全程不变）
    QPointF m_grabMousePos;           // 鼠标按下位置
    QRectF m_initialBounds;           // 选择框初始边界（场景坐标）
    QPointF m_oppositeHandle;         // 手柄对角点（用于计算比例）
    QPointF m_transformOrigin;        // 变换矩阵原点（受修饰键影响）
    QPointF m_scaleAnchor;            // 固定的缩放锚点（场景坐标）
    

    // 旋转中心
    bool m_useCustomRotationCenter = false;  // 是否使用自定义旋转中心
    QPointF m_customRotationCenter;          // 自定义旋转中心（场景坐标）
    
    // 模式管理
    HandleMode::Mode m_currentMode = HandleMode::Scale;  // 当前模式（默认为缩放模式）

    
    
    // 选中图形管理
    QList<DrawingShape*> m_selectedShapes;
    QHash<DrawingShape*, QTransform> m_originalTransforms; // 保存每个图形的初始变换

    
    // 手柄管理
    HandleManager *m_handleManager = nullptr;
    QRectF m_handleBounds; // 手柄始终基于这个边界（初始边界）
    
    // 视觉辅助元素
    CustomHandleItem *m_anchorPoint = nullptr;            // 锚点（红色十字）
    CustomHandleItem *m_dragPoint = nullptr;              // 拖动点（绿色十字）
    CustomHandleItem *m_rotationCenter = nullptr;           // 旋转中心（浅蓝色）
    QGraphicsPathItem *m_outlinePreview = nullptr;      // 整体轮廓预览（类似Inkscape）
    QTimer *m_dashTimer = nullptr;                      // 蚂蚁线动画定时器
};

#endif // DRAWING_TOOL_OUTLINE_PREVIEW_H