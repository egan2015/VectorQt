#ifndef NODE_HANDLE_MANAGER_H
#define NODE_HANDLE_MANAGER_H

#include <QObject>
#include <QList>
#include <QPointF>
#include <QRectF>
#include "../tools/handle-item.h"
#include "../tools/handle-types.h"
#include "../core/drawing-shape.h"

class DrawingScene;
class DrawingShape;

/**
 * @brief 节点手柄管理器 - 专门用于节点编辑工具的手柄管理
 * 替代老的EditHandle系统，使用新的CustomHandleItem
 */
class NodeHandleManager : public QObject
{
    Q_OBJECT

public:
    explicit NodeHandleManager(DrawingScene *scene, QObject *parent = nullptr);
    ~NodeHandleManager();

    // 节点手柄类型（扩展基础手柄类型）
    enum NodeHandleType {
        CornerRadiusHandle = 100,    // 圆角控制手柄
        SizeControlHandle = 101,     // 尺寸控制手柄
        PathNodeHandle = 102,        // 路径节点手柄
        PathControlHandle = 103,     // 路径控制点手柄
        CustomNodeHandle = 104       // 自定义节点手柄
    };

    // 手柄信息结构
    struct NodeHandleInfo {
        CustomHandleItem *handle;
        NodeHandleType type;
        int nodeIndex;
        QPointF originalPos;
        
        NodeHandleInfo() : handle(nullptr), type(CustomNodeHandle), nodeIndex(-1) {}
    };

    // 更新手柄位置和显示状态
    void updateHandles(DrawingShape *shape);
    
    // 清除所有手柄
    void clearHandles();
    
    // 获取指定位置的手柄
    CustomHandleItem* getHandleAt(const QPointF &scenePos) const;
    
    // 获取手柄信息
    NodeHandleInfo getHandleInfo(CustomHandleItem *handle) const;
    
    // 获取所有手柄
    QList<NodeHandleInfo> getAllHandles() const { return m_handleInfos; }
    
    // 显示/隐藏手柄
    void showHandles();
    void hideHandles();
    
    // 设置活动手柄
    void setActiveHandle(CustomHandleItem *handle);
    CustomHandleItem* activeHandle() const { return m_activeHandle; }
    
    // 更新手柄位置（用于拖动过程中的实时更新）
    void updateHandlePosition(CustomHandleItem *handle, const QPointF &newPos);
    
    // 应用手柄位置变化到图形
    void applyHandleChanges();
    
    // 更新现有手柄位置（不重新创建）
    void updateExistingHandlePositions(DrawingShape *shape);

private:
    // 创建不同类型的节点手柄
    CustomHandleItem* createCornerRadiusHandle(const QPointF &pos);
    CustomHandleItem* createSizeControlHandle(const QPointF &pos);
    CustomHandleItem* createPathNodeHandle(const QPointF &pos);
    CustomHandleItem* createPathControlHandle(const QPointF &pos);
    CustomHandleItem* createCustomNodeHandle(const QPointF &pos, int index);
    
    // 根据图形类型创建手柄
    void createHandlesForRectangle(DrawingRectangle *rect);
    void createHandlesForEllipse(DrawingEllipse *ellipse);
    void createHandlesForPath(DrawingPath *path);
    void createCustomNodeHandles(DrawingShape *shape);
    
    // 计算手柄位置（考虑图形变换）
    QPointF calculateHandlePosition(const QPointF &localPoint, DrawingShape *shape) const;
    
    // 设置手柄样式
    void setupHandleStyle(CustomHandleItem *handle, NodeHandleType type);

private:
    DrawingScene *m_scene;
    DrawingShape *m_currentShape;
    QList<NodeHandleInfo> m_handleInfos;
    CustomHandleItem *m_activeHandle;
    bool m_handlesVisible;
    
    // 手柄样式配置
    static const qreal DEFAULT_HANDLE_SIZE;
    static const QColor CORNER_RADIUS_COLOR;
    static const QColor SIZE_CONTROL_COLOR;
    static const QColor PATH_NODE_COLOR;
    static const QColor PATH_CONTROL_COLOR;
};

#endif // NODE_HANDLE_MANAGER_H