# Inkscape风格智能节点控制方案

## 核心改进

### 1. 智能节点类型识别算法

```cpp
class SmartNodeTypeDetector {
public:
    // 自动检测节点类型
    static BezierNodeType detectNodeType(const BezierNode &node) {
        if (!node.isValid()) return BezierNodeType::Corner;
        
        qreal inLength = node.inArm.length;
        qreal outLength = node.outArm.length;
        qreal angleDiff = qAbs(node.inArm.angle - node.outArm.angle);
        
        // 标准化角度差（考虑180度对称）
        qreal normalizedDiff = (angleDiff > M_PI) ? (2*M_PI - angleDiff) : angleDiff;
        
        // 尖角节点：控制杆独立，且角度差异大
        if (normalizedDiff > 60 * M_PI/180) {
            return BezierNodeType::Corner;
        }
        
        // 对称节点：控制杆长度相等，且角度基本对称
        if (qAbs(inLength - outLength) < 2.0 && 
            qAbs(normalizedDiff - M_PI) < 10 * M_PI/180) {
            return BezierNodeType::Symmetric;
        }
        
        // 平滑节点：控制杆在同一线上，长度可不同
        if (normalizedDiff < 5 * M_PI/180) {
            return BezierNodeType::Smooth;
        }
        
        // 默认情况下，根据角度差异决定
        return (normalizedDiff > 20 * M_PI/180) ? 
               BezierNodeType::Corner : BezierNodeType::Smooth;
    }
    
    // 智能转换节点类型
    static void smartConvertNodeType(BezierNode &node, BezierNodeType newType) {
        node.setNodeType(newType);
        
        // 智能调整控制杆
        switch (newType) {
        case BezierNodeType::Corner:
            // 尖角：保持当前控制杆独立状态
            break;
            
        case BezierNodeType::Smooth:
            // 平滑：保持长度，调整角度
            node.alignControlArms(false);
            break;
            
        case BezierNodeType::Symmetric:
            // 对称：统一长度和角度
            node.alignControlArms(true);
            break;
        }
    }
};
```

### 2. 单杆控制逻辑

```cpp
class SingleArmController {
public:
    // 处理单杆拖拽
    static void handleSingleArmDrag(BezierNode &node, bool isInArm, 
                                   const QPointF &newControlPoint) {
        switch (node.nodeType) {
        case BezierNodeType::Corner:
            // 尖角节点：只有拖拽的控制杆移动
            if (isInArm) {
                node.updateInArm(newControlPoint);
            } else {
                node.updateOutArm(newControlPoint);
            }
            break;
            
        case BezierNodeType::Smooth:
            // 平滑节点：移动拖拽的杆，另一个杆保持同线但不同长度
            if (isInArm) {
                node.updateInArm(newControlPoint);
            } else {
                node.updateOutArm(newControlPoint);
            }
            break;
            
        case BezierNodeType::Symmetric:
            // 对称节点：另一个控制杆同步移动但方向相反
            if (isInArm) {
                node.updateInArm(newControlPoint);
                // 计算对称位置
                QPointF symmetricOut = node.nodePoint + 
                    QPointF(-(newControlPoint.x() - node.nodePoint.x()),
                           -(newControlPoint.y() - node.nodePoint.y()));
                node.updateOutArm(symmetricOut);
            } else {
                node.updateOutArm(newControlPoint);
                // 计算对称位置
                QPointF symmetricIn = node.nodePoint + 
                    QPointF(-(newControlPoint.x() - node.nodePoint.x()),
                           -(newControlPoint.y() - node.nodePoint.y()));
                node.updateInArm(symmetricIn);
            }
            break;
        }
    }
    
    // 获取当前可移动的控制杆
    static QList<bool> getMovableArms(const BezierNode &node, bool isInArmDragging) {
        QList<bool> movable;
        
        switch (node.nodeType) {
        case BezierNodeType::Corner:
            movable << isInArmDragging << !isInArmDragging;
            break;
            
        case BezierNodeType::Smooth:
            movable << isInArmDragging << !isInArmDragging;
            break;
            
        case BezierNodeType::Symmetric:
            // 对称模式下，两个控制杆同时移动
            movable << true << true;
            break;
        }
        
        return movable;
    }
};
```

### 3. 智能手柄显示

```cpp
class SmartHandleDisplay {
public:
    // 根据节点类型决定显示哪些手柄
    static void updateHandleVisibility(NodeHandleManager *manager, 
                                      const BezierNode &node) {
        QList<NodeHandleManager::NodeHandleInfo> handles = manager->getAllHandles();
        
        for (auto &handleInfo : handles) {
            switch (handleInfo.type) {
            case NodeHandleManager::BezierControlIn:
                handleInfo.handle->setVisible(node.inArm.isVisible);
                break;
                
            case NodeHandleManager::BezierControlOut:
                handleInfo.handle->setVisible(node.outArm.isVisible);
                break;
                
            // 其他手柄类型...
            }
        }
        
        // 根据节点类型调整手柄样式
        switch (node.nodeType) {
        case BezierNodeType::Corner:
            setCornerHandleStyle(handles);
            break;
            
        case BezierNodeType::Smooth:
            setSmoothHandleStyle(handles);
            break;
            
        case BezierNodeType::Symmetric:
            setSymmetricHandleStyle(handles);
            break;
        }
    }
    
private:
    static void setCornerHandleStyle(const QList<NodeHandleManager::NodeHandleInfo> &handles) {
        // 尖角节点：独立控制，手柄颜色区分
        for (auto &handle : handles) {
            if (handle.type == NodeHandleManager::BezierControlIn) {
                handle.handle->setColor(Qt::blue);    // 蓝色：进入控制
            } else if (handle.type == NodeHandleManager::BezierControlOut) {
                handle.handle->setColor(Qt::red);     // 红色：离开控制
            }
        }
    }
    
    static void setSmoothHandleStyle(const QList<NodeHandleManager::NodeHandleInfo> &handles) {
        // 平滑节点：可独立长度，手柄颜色相同
        for (auto &handle : handles) {
            handle.handle->setColor(Qt::green);       // 绿色：平滑控制
        }
    }
    
    static void setSymmetricHandleStyle(const QList<NodeHandleManager::NodeHandleInfo> &handles) {
        // 对称节点：完全同步，手柄颜色特殊
        for (auto &handle : handles) {
            handle.handle->setColor(Qt::magenta);     // 紫色：对称控制
            handle.handle->setShape(HandleShape::Diamond); // 菱形表示对称
        }
    }
};
```

### 4. 拖拽智能约束

```cpp
class SmartDragConstraints {
public:
    // 应用智能拖拽约束
    static QPointF applyConstraints(const BezierNode &node, bool isInArm,
                                   const QPointF &currentPos, 
                                   const QPointF &dragStartPos) {
        QPointF constrainedPos = currentPos;
        
        switch (node.nodeType) {
        case BezierNodeType::Corner:
            // 尖角：无特殊约束，只确保在有效范围内
            break;
            
        case BezierNodeType::Smooth:
            // 平滑：保持控制杆在同一直线上
            constrainedPos = enforceSmoothConstraint(node, isInArm, currentPos);
            break;
            
        case BezierNodeType::Symmetric:
            // 对称：确保对称性
            constrainedPos = enforceSymmetricConstraint(node, isInArm, currentPos);
            break;
        }
        
        return constrainedPos;
    }
    
private:
    static QPointF enforceSmoothConstraint(const BezierNode &node, bool isInArm,
                                         const QPointF &currentPos) {
        QPointF nodePos = node.nodePoint;
        QLineF nodeToCurrent(nodePos, currentPos);
        
        // 获取其他控制杆的角度
        qreal otherAngle = isInArm ? node.outArm.angle : node.inArm.angle;
        qreal targetAngle = otherAngle + M_PI; // 相反方向
        
        // 计算在目标角度上的位置
        qreal distance = nodeToCurrent.length();
        QPointF constrainedPos = nodePos + 
            QPointF(distance * std::cos(targetAngle), 
                   distance * std::sin(targetAngle));
        
        return constrainedPos;
    }
    
    static QPointF enforceSymmetricConstraint(const BezierNode &node, bool isInArm,
                                            const QPointF &currentPos) {
        // 首先应用平滑约束
        QPointF smoothPos = enforceSmoothConstraint(node, isInArm, currentPos);
        
        // 然后确保长度对称
        QPointF nodePos = node.nodePoint;
        QLineF nodeToSmooth(nodePos, smoothPos);
        
        // 找到对应的控制杆
        const BezierControlArm &otherArm = isInArm ? node.outArm : node.inArm;
        qreal targetLength = otherArm.length;
        
        // 调整到对称长度
        QPointF symmetricPos = nodePos + 
            QPointF(targetLength * std::cos(nodeToSmooth.angle()),
                   targetLength * std::sin(nodeToSmooth.angle()));
        
        return symmetricPos;
    }
};
```

### 5. 键盘快捷键支持

```cpp
class NodeEditKeyHandler {
public:
    // 处理键盘输入
    static bool handleKeyPress(QKeyEvent *event, BezierNode &selectedNode) {
        switch (event->key()) {
        case Qt::Key_C:
            // C键：切换到尖角模式
            SmartNodeTypeDetector::smartConvertNodeType(selectedNode, BezierNodeType::Corner);
            return true;
            
        case Qt::Key_S:
            // S键：切换到平滑模式
            SmartNodeTypeDetector::smartConvertNodeType(selectedNode, BezierNodeType::Smooth);
            return true;
            
        case Qt::Key_Y:
            // Y键：切换到对称模式
            SmartNodeTypeDetector::smartConvertNodeType(selectedNode, BezierNodeType::Symmetric);
            return true;
            
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            // 删除键：删除当前节点
            deleteCurrentNode(selectedNode);
            return true;
            
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // 回车键：添加新节点
            addNodeAfter(selectedNode);
            return true;
            
        default:
            return false;
        }
    }
};
```

## 集成到现有系统

### 1. 扩展NodeHandleManager

```cpp
// 在NodeHandleManager中添加智能控制方法
class NodeHandleManager {
public:
    // 智能控制方法
    void enableSmartControl() { m_smartControlEnabled = true; }
    void disableSmartControl() { m_smartControlEnabled = false; }
    
    // 处理智能节点转换
    void convertNodeType(int nodeIndex, BezierNodeType newType);
    
    // 实时更新智能显示
    void updateSmartDisplay(const QList<BezierNode> &nodes);
    
private:
    bool m_smartControlEnabled = true;
};
```

### 2. 集成到节点编辑工具

```cpp
// 在DrawingNodeEditTool中添加智能控制
class DrawingNodeEditTool {
private:
    void handleSmartNodeEdit(const QPointF &scenePos, Qt::KeyboardModifiers modifiers);
    void updateSmartHandles();
    void applySmartConstraints(QPointF &pos, const BezierNode &node);
};
```

## 用户体验改进

### 视觉反馈
- **颜色编码**：不同节点类型使用不同颜色
- **手柄形状**：方形、圆形、菱形区分控制类型
- **实时预览**：拖拽时实时显示效果

### 操作便捷性
- **键盘切换**：C/S/Y快速切换节点类型
- **智能检测**：自动识别并建议最佳节点类型
- **单杆控制**：精准控制每个控制杆

### 智能特性
- **自动对齐**：自动检测并对齐控制杆
- **平滑连接**：自动保持路径的平滑性
- **对称约束**：智能保持对称性

这个方案将让您的节点编辑器达到Inkscape级别的智能化水平！