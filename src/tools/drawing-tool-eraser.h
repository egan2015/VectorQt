#ifndef DRAWING_TOOL_ERASER_H
#define DRAWING_TOOL_ERASER_H

#include "../core/toolbase.h"
#include <QPointF>
#include <QList>
#include <QRectF>

class DrawingScene;
class DrawingView;
class DrawingShape;
class QGraphicsEllipseItem;

/**
 * 橡皮擦工具
 * 用于擦除图形或图形的部分区域
 */
class DrawingToolEraser : public ToolBase
{
    Q_OBJECT

public:
    enum EraserMode {
        WholeErase,     // 整体擦除：点击完全删除图形
        PartialErase    // 局部擦除：擦除图形的部分区域
    };

    explicit DrawingToolEraser(QObject *parent = nullptr);
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool keyPressEvent(QKeyEvent *event) override;
    bool keyReleaseEvent(QKeyEvent *event);
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::DefaultCursor; }

    // 设置擦除模式
    void setEraserMode(EraserMode mode);
    
    // 设置橡皮擦大小
    void setEraserSize(qreal size);
    
    // 获取橡皮擦大小
    qreal getEraserSize() const { return m_eraserSize; }
    int eraserSizeForPanel() const { return static_cast<int>(m_eraserSize); }

private:
    // 查找擦除区域内的图形
    QList<DrawingShape*> findShapesInArea(const QPointF &center, qreal radius);
    
    // 整体擦除图形
    void eraseShape(DrawingShape *shape);
    
    // 局部擦除图形（未来实现）
    void partialEraseShape(DrawingShape *shape, const QPointF &center, qreal radius);
    
    // 更新橡皮擦预览
    void updateEraserPreview(const QPointF &scenePos);
    
    // 显示橡皮擦光标
    void showEraserCursor(const QPointF &scenePos);
    
    // 隐藏橡皮擦光标
    void hideEraserCursor();
    
    // 检查点是否在擦除范围内
    bool isInRange(const QPointF &point, const QPointF &center, qreal radius);

private:
    DrawingScene *m_scene;
    DrawingView *m_view;
    
    EraserMode m_mode;
    qreal m_eraserSize;
    bool m_isErasing;
    
    // 预览相关
    QGraphicsEllipseItem *m_previewItem;
    
    // 擦除历史（用于撤销）
    QList<DrawingShape*> m_erasedShapes;
    QList<QRectF> m_erasedAreas;
};

#endif // DRAWING_TOOL_ERASER_H