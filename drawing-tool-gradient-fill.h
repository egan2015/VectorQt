#ifndef DRAWING_TOOL_GRADIENT_FILL_H
#define DRAWING_TOOL_GRADIENT_FILL_H

#include "toolbase.h"
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <QGradientStop>

class DrawingScene;
class DrawingView;
class DrawingShape;

/**
 * 渐进填充工具
 * 支持多种渐变模式的智能填充工具
 */
class DrawingToolGradientFill : public ToolBase
{
    Q_OBJECT

public:
    enum GradientMode {
        LinearGradient,    // 线性渐变
        RadialGradient,    // 径向渐变
        ConicalGradient    // 锥形渐变
    };

    enum FillMode {
        ShapeFill,         // 形状填充
        RegionFill         // 区域填充（未来扩展）
    };

    explicit DrawingToolGradientFill(QObject *parent = nullptr);
    
    void activate(DrawingScene *scene, DrawingView *view) override;
    void deactivate() override;
    
    bool mousePressEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos) override;
    bool keyPressEvent(QKeyEvent *event);
    
    // 获取工具光标类型
    CursorManager::CursorType getCursorType() const override { return CursorManager::FillCursor; }

    // 设置渐变模式
    void setGradientMode(GradientMode mode);
    
    // 设置填充模式
    void setFillMode(FillMode mode);
    
    // 添加渐变色标
    void addGradientStop(qreal position, const QColor &color);
    
    // 清除渐变色标
    void clearGradientStops();
    
    // 获取当前渐变
    QGradient* getCurrentGradient() const;

public slots:
    // 响应颜色变化
    void onFillColorChanged(const QColor &color);
    
private:
    // 查找点击位置的封闭图形
    DrawingShape* findEnclosedShape(const QPointF &scenePos);
    
    // 创建线性渐变
    QLinearGradient* createLinearGradient(const QPointF &start, const QPointF &end) const;
    
    // 创建径向渐变
    QRadialGradient* createRadialGradient(const QPointF &center, qreal radius) const;
    
    // 创建锥形渐变
    QConicalGradient* createConicalGradient(const QPointF &center, qreal angle) const;
    
    // 应用渐变到形状
    void applyGradientToShape(DrawingShape *shape, QGradient *gradient);
    
    // 获取当前填充颜色
    QColor getCurrentFillColor() const;
    
    // 更新预览
    void updatePreview(const QPointF &scenePos);
    
    GradientMode m_gradientMode;
    FillMode m_fillMode;
    QPointF m_startPoint;
    QPointF m_endPoint;
    bool m_isDragging;
    QList<QGradientStop> m_gradientStops;
    QColor m_currentFillColor;
    
    // 预览相关
    QGraphicsItem *m_previewItem;
    
    // 目标形状相关
    DrawingShape *m_targetShape;
    QFlags<QGraphicsItem::GraphicsItemFlag> m_originalFlags;
    QBrush m_originalFillBrush;
};

#endif // DRAWING_TOOL_GRADIENT_FILL_H