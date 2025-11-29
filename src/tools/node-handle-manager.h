#ifndef NODE_HANDLE_MANAGER_H
#define NODE_HANDLE_MANAGER_H

#include <QObject>
#include <QList>
#include <QPointF>
#include <QRectF>
#include "handle-item.h"
#include "handle-types.h"
#include "bezier-control-arm.h"
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
        PathNodeHandle = 102,        // 路径节点手柄（锚点）- 方形
        PathControlHandle = 103,     // 路径控制点手柄 - 圆形
        BezierNodeIn = 104,          // 贝塞尔进入节点 - 方形
        BezierNodeOut = 105,         // 贝塞尔离开节点 - 方形
        BezierControlIn = 106,       // 贝塞尔进入控制点 - 圆形（蓝色）
        BezierControlOut = 107,      // 贝塞尔离开控制点 - 圆形（绿色）
        CustomNodeHandle = 108       // 自定义节点手柄
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
    
    // 获取手柄数量（用于调试）
    int getHandleCount() const { return m_handleInfos.size(); }
    
    // 获取手柄信息
    NodeHandleInfo getHandleInfo(CustomHandleItem *handle) const;
    
    // 获取所有手柄
    QList<NodeHandleInfo> getAllHandles() const { return m_handleInfos; }
    
    // 显示/隐藏手柄
    void showHandles();
    void hideHandles();
    
    // 控制杆相关方法
    void updateBezierControlArms(DrawingShape *shape);
    void createBezierControlHandles(DrawingShape *shape);
    void updateControlArmLines();
    void createSimpleControlArm(DrawingShape *shape);
    
    // 设置活动手柄
    void setActiveHandle(CustomHandleItem *handle);
    CustomHandleItem* activeHandle() const { return m_activeHandle; }
    
    // 更新手柄位置（用于拖动过程中的实时更新）
    void updateHandlePosition(CustomHandleItem *handle, const QPointF &newPos);
    
    // 应用手柄位置变化到图形
    void applyHandleChanges();
    
    // 智能控制杆处理
    void handleSmartControlArmDrag(CustomHandleItem *handle, const QPointF &newPos);
    void updateNodeFromHandle(CustomHandleItem *handle, const QPointF &newPos);
    
    // 更新现有手柄位置（不重新创建）
    void updateExistingHandlePositions(DrawingShape *shape);
    
    // 基于NodeInfo的智能手柄管理
    void updateHandlesFromNodeInfo(DrawingShape *shape);
    void createHandlesForNodeInfo(const NodeInfo &nodeInfo, int index);
    NodeHandleType getHandleTypeFromNodeInfo(const NodeInfo &nodeInfo) const;
    void setupHandleStyleFromNodeInfo(CustomHandleItem *handle, const NodeInfo &nodeInfo) const;

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
    
    // 路径控制点连线管理
    void updatePathControlLines();
    void clearPathControlLines();

private:
    DrawingScene *m_scene;
    DrawingShape *m_currentShape;
    QList<NodeHandleInfo> m_handleInfos;
    CustomHandleItem *m_activeHandle;
    bool m_handlesVisible;
    
    // 控制点连线
    QList<QGraphicsLineItem*> m_controlLines;
    
    // 贝塞尔控制杆系统
    QList<BezierNode> m_bezierNodes;
    QList<QGraphicsLineItem*> m_controlArmLines;  // 控制杆连线
    
    // 手柄样式配置
    static const qreal DEFAULT_HANDLE_SIZE;
    static const QColor CORNER_RADIUS_COLOR;
    static const QColor SIZE_CONTROL_COLOR;
    static const QColor PATH_NODE_COLOR;        // 节点（方形）- 深蓝色
    static const QColor PATH_CONTROL_COLOR;     // 控制点（圆形）- 浅蓝色
    static const QColor BEZIER_NODE_COLOR;      // 贝塞尔节点 - 深蓝色
    static const QColor BEZIER_CONTROL_IN_COLOR; // 贝塞尔进入控制点 - 蓝色
    static const QColor BEZIER_CONTROL_OUT_COLOR; // 贝塞尔离开控制点 - 白色
    
    // 智能节点类型颜色配置
    static const QColor CORNER_NODE_COLOR;      // 尖角节点 - 红色
    static const QColor SMOOTH_NODE_COLOR;      // 平滑节点 - 绿色
    static const QColor SYMMETRIC_NODE_COLOR;   // 对称节点 - 紫色
    static const QColor CURVE_NODE_COLOR;       // 曲线节点 - 橙色

};

#endif // NODE_HANDLE_MANAGER_H