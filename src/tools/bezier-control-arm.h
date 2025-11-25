#ifndef BEZIER_CONTROL_ARM_H
#define BEZIER_CONTROL_ARM_H

#include <QObject>
#include <QPointF>
#include <QLineF>
#include <QDebug>

/**
 * @brief 贝塞尔曲线节点类型枚举
 * 参考Inkscape的节点类型设计
 */
enum class BezierNodeType {
    Corner,     // 尖角节点 - 两侧控制点独立移动
    Smooth,     // 平滑节点 - 两侧控制点保持在同一直线上，但长度可独立
    Symmetric   // 对称节点 - 两侧控制点保持在同一直线上且长度相等
};

/**
 * @brief 贝塞尔控制杆结构
 * 每个控制杆包含一个控制点和相关属性
 */
struct BezierControlArm {
    QPointF controlPoint;      // 控制点位置（相对于节点）
    qreal length;              // 控制杆长度
    qreal angle;               // 控制杆角度（弧度）
    bool isVisible;            // 是否显示
    
    BezierControlArm() : length(0.0), angle(0.0), isVisible(false) {}
    BezierControlArm(const QPointF &point) : controlPoint(point), isVisible(true) {
        updateFromPoint();
    }
    
    // 从控制点位置更新长度和角度
    void updateFromPoint() {
        length = QLineF(QPointF(0, 0), controlPoint).length();
        if (length > 0.001) {
            angle = std::atan2(controlPoint.y(), controlPoint.x());
        }
    }
    
    // 从长度和角度更新控制点位置
    void updatePoint() {
        controlPoint = QPointF(length * std::cos(angle), length * std::sin(angle));
    }
    
    // 设置控制点位置并更新长度角度
    void setControlPoint(const QPointF &point) {
        controlPoint = point;
        updateFromPoint();
    }
    
    // 设置长度并更新控制点位置
    void setLength(qreal newLength) {
        length = qMax(0.0, newLength);
        updatePoint();
    }
    
    // 设置角度并更新控制点位置
    void setAngle(qreal newAngle) {
        angle = newAngle;
        updatePoint();
    }
};

/**
 * @brief 贝塞尔节点数据结构
 * 包含节点位置、类型和两个控制杆（进入和离开）
 */
struct BezierNode {
    QPointF nodePoint;              // 节点位置（锚点）
    BezierNodeType nodeType;        // 节点类型
    BezierControlArm inArm;         // 进入控制杆（控制前一段曲线）
    BezierControlArm outArm;        // 离开控制杆（控制后一段曲线）
    bool isSelected;                // 是否被选中
    
    BezierNode() : nodeType(BezierNodeType::Corner), isSelected(false) {}
    
    BezierNode(const QPointF &point, BezierNodeType type = BezierNodeType::Corner)
        : nodePoint(point), nodeType(type), isSelected(false) {}
    
    // 设置节点类型并调整控制杆
    void setNodeType(BezierNodeType newType) {
        if (nodeType == newType) return;
        
        nodeType = newType;
        
        switch (nodeType) {
        case BezierNodeType::Corner:
            // 尖角节点：控制杆完全独立
            break;
            
        case BezierNodeType::Smooth:
            // 平滑节点：控制杆保持同一直线，长度可独立
            alignControlArms(false);
            break;
            
        case BezierNodeType::Symmetric:
            // 对称节点：控制杆保持同一直线且长度相等
            alignControlArms(true);
            break;
        }
    }
    
    // 对齐控制杆到同一直线
    void alignControlArms(bool makeSymmetric = false) {
        if (!inArm.isVisible && !outArm.isVisible) return;
        
        if (inArm.isVisible && outArm.isVisible) {
            // 两个控制杆都可见，计算平均角度
            qreal avgAngle = (inArm.angle + outArm.angle) / 2.0;
            
            if (makeSymmetric) {
                // 对称模式：使用平均长度
                qreal avgLength = (inArm.length + outArm.length) / 2.0;
                inArm.setAngle(avgAngle);
                inArm.setLength(avgLength);
                outArm.setAngle(avgAngle + M_PI);  // 相反方向
                outArm.setLength(avgLength);
            } else {
                // 平滑模式：保持原长度，只调整角度
                qreal inLength = inArm.length;
                qreal outLength = outArm.length;
                
                inArm.setAngle(avgAngle);
                outArm.setAngle(avgAngle + M_PI);  // 相反方向
                
                inArm.setLength(inLength);
                outArm.setLength(outLength);
            }
        } else if (inArm.isVisible) {
            // 只有进入控制杆可见
            outArm.setAngle(inArm.angle + M_PI);
            if (makeSymmetric) {
                outArm.setLength(inArm.length);
            }
        } else if (outArm.isVisible) {
            // 只有离开控制杆可见
            inArm.setAngle(outArm.angle + M_PI);
            if (makeSymmetric) {
                inArm.setLength(outArm.length);
            }
        }
    }
    
    // 更新进入控制杆位置
    void updateInArm(const QPointF &newControlPoint) {
        inArm.setControlPoint(newControlPoint - nodePoint);
        
        if (nodeType != BezierNodeType::Corner) {
            // 非尖角节点，需要同步更新离开控制杆
            alignControlArms(nodeType == BezierNodeType::Symmetric);
        }
    }
    
    // 更新离开控制杆位置
    void updateOutArm(const QPointF &newControlPoint) {
        outArm.setControlPoint(newControlPoint - nodePoint);
        
        if (nodeType != BezierNodeType::Corner) {
            // 非尖角节点，需要同步更新进入控制杆
            alignControlArms(nodeType == BezierNodeType::Symmetric);
        }
    }
    
    // 获取进入控制点的绝对位置
    QPointF getInControlPoint() const {
        return nodePoint + inArm.controlPoint;
    }
    
    // 获取离开控制点的绝对位置
    QPointF getOutControlPoint() const {
        return nodePoint + outArm.controlPoint;
    }
    
    // 检查节点是否有效（至少有一个控制杆）
    bool isValid() const {
        return inArm.isVisible || outArm.isVisible;
    }
};

#endif // BEZIER_CONTROL_ARM_H