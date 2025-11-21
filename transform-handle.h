#ifndef TRANSFORM_HANDLE_H
#define TRANSFORM_HANDLE_H

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QList>
#include "handle-item.h"
#include "handle-types.h"

class DrawingScene;



/**
 * @brief 手柄管理器
 * @details 负责创建、显示和管理变换手柄
 */
class HandleManager : public QObject
{
    Q_OBJECT

public:
    explicit HandleManager(DrawingScene *scene, QObject *parent = nullptr);
    ~HandleManager();

    // 显示/隐藏手柄
    void showHandles();
    void hideHandles();
    
    // 更新手柄位置
    void updateHandles(const QRectF &bounds);
    
    // 模式管理
    void setHandleMode(HandleMode::Mode mode);
    HandleMode::Mode handleMode() const { return m_handleMode; }
    
    // 检测点击位置的手柄
    TransformHandle::HandleType getHandleAtPosition(const QPointF &scenePos) const;
    
    // 设置活动手柄
    void setActiveHandle(TransformHandle::HandleType type);
    
    // 更新手柄悬停状态
    void updateHandleHover(const QPointF &scenePos);
    
    // 获取手柄位置
    QPointF getHandlePosition(TransformHandle::HandleType type) const;
    
    // 设置中心手柄位置（用于自定义旋转中心）
    void setCenterHandlePosition(const QPointF &pos);
    
    // 🌟 检查并确保手柄被添加到场景中
    void ensureHandlesInScene();
    
    // 更新手柄可见性（根据模式）
    void updateHandlesVisibility();
    
    // 控制手柄显示/隐藏的公共方法
    void setShowHandles(bool show);
    bool shouldShowHandles() const;
    
private:
    // 更新旋转角点手柄位置
    void updateRotateCornerHandle(int index, const QPointF &pos);

private:
    // 创建手柄
    void createHandles();
    void destroyHandles();
    
    // 更新单个手柄位置
    void updateHandlePosition(TransformHandle::HandleType type, const QPointF &pos);
    
    // 更新单个手柄的悬停效果
    void updateHandleHoverEffect(QGraphicsItem *handle, bool isHovered);
    
    // 获取手柄的视觉大小
    qreal getHandleSize() const { return 8.0; }
    
    DrawingScene *m_scene;
    QRectF m_bounds;
    HandleMode::Mode m_handleMode;
    
    // 手柄图形项 - 使用自定义绘制的手柄
    QList<CustomHandleItem*> m_cornerHandles;   // 角点手柄
    QList<CustomHandleItem*> m_edgeHandles;     // 边缘手柄
    CustomHandleItem* m_centerHandle;           // 中心手柄
    CustomHandleItem* m_rotateHandle;           // 旋转手柄
    QGraphicsRectItem* m_selectionBorder;       // 选择边框线
    
    // 旋转模式专用手柄（4个角点的旋转手柄）
    QList<CustomHandleItem*> m_rotateCornerHandles;
    
    TransformHandle::HandleType m_activeHandle;
    
    // 控制手柄是否应该显示
    bool m_shouldShowHandles = true;
    
    // 手柄颜色
    static const QColor HANDLE_COLOR;
    static const QColor ACTIVE_HANDLE_COLOR;
    static const QColor ROTATE_HANDLE_COLOR;
};

#endif // TRANSFORM_HANDLE_H