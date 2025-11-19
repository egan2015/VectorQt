#ifndef TRANSFORM_HANDLE_H
#define TRANSFORM_HANDLE_H

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QList>

class DrawingScene;

/**
 * @brief 变换手柄类型枚举
 */
class TransformHandle
{
public:
    enum HandleType {
        None = 0,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Left,
        Right,
        Top,
        Bottom,
        Center,
        Rotate  // 旋转手柄
    };
};

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
    
    // 检测点击位置的手柄
    TransformHandle::HandleType getHandleAtPosition(const QPointF &scenePos) const;
    
    // 设置活动手柄
    void setActiveHandle(TransformHandle::HandleType type);
    
    // 获取手柄位置
    QPointF getHandlePosition(TransformHandle::HandleType type) const;
    
    // 设置中心手柄位置（用于自定义旋转中心）
    void setCenterHandlePosition(const QPointF &pos);

private:
    // 创建手柄
    void createHandles();
    void destroyHandles();
    
    // 更新单个手柄位置
    void updateHandlePosition(TransformHandle::HandleType type, const QPointF &pos);
    
    // 获取手柄的视觉大小
    qreal getHandleSize() const { return 8.0; }
    
    DrawingScene *m_scene;
    QRectF m_bounds;
    
    // 手柄图形项
    QList<QGraphicsRectItem*> m_cornerHandles;  // 角点手柄
    QList<QGraphicsRectItem*> m_edgeHandles;    // 边缘手柄
    QGraphicsEllipseItem* m_centerHandle;       // 中心手柄
    QGraphicsEllipseItem* m_rotateHandle;       // 旋转手柄
    
    TransformHandle::HandleType m_activeHandle;
    
    // 手柄颜色
    static const QColor HANDLE_COLOR;
    static const QColor ACTIVE_HANDLE_COLOR;
    static const QColor ROTATE_HANDLE_COLOR;
};

#endif // TRANSFORM_HANDLE_H