// SmartNodeControlExample.cpp
// Inkscape风格智能节点控制实现示例

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsEllipseItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <cmath>

// ==================== 核心智能控制类 ====================

class SmartNodeController {
public:
    // 自动检测并转换节点类型
    static void smartConvertNodeType(BezierNode& node, BezierNodeType targetType) {
        qDebug() << "Converting node from" << node.nodeType << "to" << targetType;
        
        switch (targetType) {
        case BezierNodeType::Corner:
            convertToCorner(node);
            break;
        case BezierNodeType::Smooth:
            convertToSmooth(node);
            break;
        case BezierNodeType::Symmetric:
            convertToSymmetric(node);
            break;
        }
    }
    
    // 智能拖拽处理
    static void handleSmartDrag(BezierNode& node, bool isInArm, const QPointF& newPos) {
        switch (node.nodeType) {
        case BezierNodeType::Corner:
            handleCornerDrag(node, isInArm, newPos);
            break;
        case BezierNodeType::Smooth:
            handleSmoothDrag(node, isInArm, newPos);
            break;
        case BezierNodeType::Symmetric:
            handleSymmetricDrag(node, isInArm, newPos);
            break;
        }
    }

private:
    static void convertToCorner(BezierNode& node) {
        // 尖角：保持控制杆完全独立
        node.nodeType = BezierNodeType::Corner;
        qDebug() << "Node converted to CORNER - arms are independent";
    }
    
    static void convertToSmooth(BezierNode& node) {
        // 平滑：控制杆在同一线上，长度可不同
        node.nodeType = BezierNodeType::Smooth;
        node.alignControlArms(false);
        qDebug() << "Node converted to SMOOTH - arms aligned with independent length";
    }
    
    static void convertToSymmetric(BezierNode& node) {
        // 对称：控制杆在同一线上，长度相等
        node.nodeType = BezierNodeType::Symmetric;
        node.alignControlArms(true);
        qDebug() << "Node converted to SYMMETRIC - arms aligned with equal length";
    }
    
    static void handleCornerDrag(BezierNode& node, bool isInArm, const QPointF& newPos) {
        // 尖角：只移动拖拽的控制杆
        if (isInArm) {
            node.updateInArm(newPos);
        } else {
            node.updateOutArm(newPos);
        }
        qDebug() << "CORNER drag: moved" << (isInArm ? "IN" : "OUT") << "arm to" << newPos;
    }
    
    static void handleSmoothDrag(BezierNode& node, bool isInArm, const QPointF& newPos) {
        // 平滑：移动拖拽的杆，另一个杆保持同线但长度独立
        if (isInArm) {
            node.updateInArm(newPos);
        } else {
            node.updateOutArm(newPos);
        }
        // 自动保持同一直线
        node.alignControlArms(false);
        qDebug() << "SMOOTH drag: moved" << (isInArm ? "IN" : "OUT") 
                 << "arm, aligned other arm";
    }
    
    static void handleSymmetricDrag(BezierNode& node, bool isInArm, const QPointF& newPos) {
        // 对称：拖拽时智能对称
        if (isInArm) {
            node.updateInArm(newPos);
            // 计算对称位置
            QPointF nodePos = node.nodePoint;
            QPointF symmetricOut = nodePos + QPointF(-(newPos.x() - nodePos.x()),
                                                    -(newPos.y() - nodePos.y()));
            node.updateOutArm(symmetricOut);
        } else {
            node.updateOutArm(newPos);
            // 计算对称位置
            QPointF nodePos = node.nodePoint;
            QPointF symmetricIn = nodePos + QPointF(-(newPos.x() - nodePos.x()),
                                                   -(newPos.y() - nodePos.y()));
            node.updateInArm(symmetricIn);
        }
        qDebug() << "SYMMETRIC drag: moved" << (isInArm ? "IN" : "OUT") 
                 << "arm, automatically symmetric";
    }
};

// ==================== 扩展现有的节点编辑工具 ====================

class EnhancedNodeEditTool : public DrawingNodeEditTool {
private:
    bool m_smartControlEnabled = true;
    BezierNode* m_currentEditingNode = nullptr;
    bool m_draggingInArm = false;

public:
    // 启用/禁用智能控制
    void enableSmartControl(bool enabled) {
        m_smartControlEnabled = enabled;
        qDebug() << "Smart control" << (enabled ? "ENABLED" : "DISABLED");
    }
    
    // 重写鼠标事件处理，添加智能控制
    bool mouseMoveEvent(QMouseEvent* event, const QPointF& scenePos) override {
        if (m_dragging && m_activeHandle && m_selectedShape) {
            // 获取节点信息
            NodeHandleManager::NodeHandleInfo handleInfo = 
                m_handleManager->getHandleInfo(m_activeHandle);
            
            if (!handleInfo.handle) return false;
            
            // 检查是否在编辑贝塞尔节点
            if (m_currentEditingNode && m_smartControlEnabled) {
                // 应用智能约束
                QPointF constrainedPos = applySmartConstraints(
                    scenePos, *m_currentEditingNode, m_draggingInArm);
                
                // 使用智能拖拽处理
                SmartNodeController::handleSmartDrag(
                    *m_currentEditingNode, m_draggingInArm, constrainedPos);
                
                // 更新控制点位置
                updateControlPointsFromNode(*m_currentEditingNode);
                
                m_scene->update();
                return true;
            }
        }
        
        return DrawingNodeEditTool::mouseMoveEvent(event, scenePos);
    }
    
    // 重写键盘事件处理
    void keyPressEvent(QKeyEvent* event) override {
        if (m_currentEditingNode && m_smartControlEnabled) {
            switch (event->key()) {
            case Qt::Key_C:
                // C键：切换到尖角模式
                SmartNodeController::smartConvertNodeType(
                    *m_currentEditingNode, BezierNodeType::Corner);
                updateControlPointsFromNode(*m_currentEditingNode);
                m_scene->update();
                break;
                
            case Qt::Key_S:
                // S键：切换到平滑模式
                SmartNodeController::smartConvertNodeType(
                    *m_currentEditingNode, BezierNodeType::Smooth);
                updateControlPointsFromNode(*m_currentEditingNode);
                m_scene->update();
                break;
                
            case Qt::Key_Y:
                // Y键：切换到对称模式
                SmartNodeController::smartConvertNodeType(
                    *m_currentEditingNode, BezierNodeType::Symmetric);
                updateControlPointsFromNode(*m_currentEditingNode);
                m_scene->update();
                break;
                
            default:
                DrawingNodeEditTool::keyPressEvent(event);
                break;
            }
        } else {
            DrawingNodeEditTool::keyPressEvent(event);
        }
    }

private:
    // 应用智能约束
    QPointF applySmartConstraints(const QPointF& pos, const BezierNode& node, bool isInArm) {
        if (!m_smartControlEnabled) return pos;
        
        switch (node.nodeType) {
        case BezierNodeType::Corner:
            // 尖角：无特殊约束
            return pos;
            
        case BezierNodeType::Smooth:
            // 平滑：确保控制杆在同一直线上
            return enforceSmoothConstraint(node, isInArm, pos);
            
        case BezierNodeType::Symmetric:
            // 对称：确保对称性
            return enforceSymmetricConstraint(node, isInArm, pos);
            
        default:
            return pos;
        }
    }
    
    // 强制平滑约束
    QPointF enforceSmoothConstraint(const BezierNode& node, bool isInArm, const QPointF& pos) {
        QPointF nodePos = node.nodePoint;
        QLineF nodeToPos(nodePos, pos);
        
        // 获取其他控制杆的角度
        qreal otherAngle = isInArm ? node.outArm.angle : node.inArm.angle;
        qreal targetAngle = otherAngle + M_PI;
        
        // 计算在目标角度上的位置
        qreal distance = nodeToPos.length();
        return nodePos + QPointF(distance * std::cos(targetAngle), 
                                distance * std::sin(targetAngle));
    }
    
    // 强制对称约束
    QPointF enforceSymmetricConstraint(const BezierNode& node, bool isInArm, const QPointF& pos) {
        // 先应用平滑约束
        QPointF smoothPos = enforceSmoothConstraint(node, isInArm, pos);
        
        // 然后确保长度对称
        QPointF nodePos = node.nodePoint;
        QLineF nodeToSmooth(nodePos, smoothPos);
        
        const BezierControlArm& otherArm = isInArm ? node.outArm : node.inArm;
        qreal targetLength = otherArm.length;
        
        return nodePos + QPointF(targetLength * std::cos(nodeToSmooth.angle()),
                                targetLength * std::sin(nodeToSmooth.angle()));
    }
    
    // 从节点更新控制点
    void updateControlPointsFromNode(const BezierNode& node) {
        if (m_selectedShape && m_selectedShape->shapeType() == DrawingShape::Path) {
            DrawingPath* path = static_cast<DrawingPath*>(m_selectedShape);
            
            // 更新贝塞尔控制点
            QVector<QPointF> controlPoints = path->controlPoints();
            
            // 这里需要根据具体的节点索引更新控制点
            // 实际实现中需要维护节点索引映射
            
            path->updatePathFromControlPoints();
        }
    }
};

// ==================== 演示程序 ====================

class SmartNodeDemo : public QGraphicsView {
private:
    QGraphicsScene* m_scene;
    BezierNode* m_testNode;

public:
    SmartNodeDemo() {
        m_scene = new QGraphicsScene(this);
        setScene(m_scene);
        setRenderHint(QPainter::Antialiasing);
        
        // 创建测试贝塞尔节点
        createTestNode();
        
        // 设置交互
        setMouseTracking(true);
    }
    
    void createTestNode() {
        m_testNode = new BezierNode(QPointF(200, 200), BezierNodeType::Corner);
        
        // 添加控制杆
        m_testNode->inArm = BezierControlArm(QPointF(50, 0));
        m_testNode->outArm = BezierControlArm(QPointF(-50, 0));
        
        m_testNode->inArm.isVisible = true;
        m_testNode->outArm.isVisible = true;
        
        // 绘制节点和控制杆
        drawNode(*m_testNode);
        
        qDebug() << "Created test node at" << m_testNode->nodePoint;
        qDebug() << "IN arm:" << m_testNode->getInControlPoint();
        qDebug() << "OUT arm:" << m_testNode->getOutControlPoint();
    }
    
    void drawNode(const BezierNode& node) {
        // 清除之前的绘制
        m_scene->clear();
        
        // 绘制节点（锚点）
        QGraphicsEllipseItem* nodeItem = m_scene->addEllipse(
            node.nodePoint.x() - 5, node.nodePoint.y() - 5, 10, 10,
            QPen(Qt::blue, 2), QBrush(Qt::blue));
        
        // 绘制控制杆
        if (node.inArm.isVisible) {
            m_scene->addLine(node.nodePoint.x(), node.nodePoint.y(),
                           node.getInControlPoint().x(), node.getInControlPoint().y(),
                           QPen(Qt::blue, 1, Qt::DashLine));
            
            m_scene->addEllipse(node.getInControlPoint().x() - 4,
                              node.getInControlPoint().y() - 4, 8, 8,
                              QPen(Qt::blue), QBrush(Qt::blue));
        }
        
        if (node.outArm.isVisible) {
            m_scene->addLine(node.nodePoint.x(), node.nodePoint.y(),
                           node.getOutControlPoint().x(), node.getOutControlPoint().y(),
                           QPen(Qt::red, 1, Qt::DashLine));
            
            m_scene->addEllipse(node.getOutControlPoint().x() - 4,
                              node.getOutControlPoint().y() - 4, 8, 8,
                              QPen(Qt::red), QBrush(Qt::red));
        }
        
        // 显示节点类型
        QString typeText;
        switch (node.nodeType) {
        case BezierNodeType::Corner: typeText = "CORNER"; break;
        case BezierNodeType::Smooth: typeText = "SMOOTH"; break;
        case BezierNodeType::Symmetric: typeText = "SYMMETRIC"; break;
        }
        
        m_scene->addText(QString("Node Type: %1").arg(typeText),
                        QFont("Arial", 12))->setPos(10, 10);
        
        // 显示快捷键提示
        QString helpText = "Keys: C=Corner, S=Smooth, Y=Symmetric\nDrag blue/red dots to test control";
        m_scene->addText(helpText, QFont("Arial", 10))->setPos(10, 40);
    }
    
    void mousePressEvent(QMouseEvent* event) override {
        QPointF pos = mapToScene(event->pos());
        
        // 检查是否点击了控制点
        QLineF inLine(m_testNode->nodePoint, m_testNode->getInControlPoint());
        QLineF outLine(m_testNode->nodePoint, m_testNode->getOutControlPoint());
        
        qreal inDistance = inLine.length();
        qreal outDistance = outLine.length();
        
        if (inDistance < 30) {
            qDebug() << "Clicked IN control arm";
            m_draggingInArm = true;
        } else if (outDistance < 30) {
            qDebug() << "Clicked OUT control arm";
            m_draggingInArm = false;
        } else {
            m_draggingInArm = false;
        }
        
        update();
    }
    
    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            QPointF pos = mapToScene(event->pos());
            
            if (m_draggingInArm) {
                SmartNodeController::handleSmartDrag(*m_testNode, true, pos);
            } else {
                SmartNodeController::handleSmartDrag(*m_testNode, false, pos);
            }
            
            drawNode(*m_testNode);
        }
    }
    
    void keyPressEvent(QKeyEvent* event) override {
        switch (event->key()) {
        case Qt::Key_C:
            SmartNodeController::smartConvertNodeType(*m_testNode, BezierNodeType::Corner);
            drawNode(*m_testNode);
            break;
            
        case Qt::Key_S:
            SmartNodeController::smartConvertNodeType(*m_testNode, BezierNodeType::Smooth);
            drawNode(*m_testNode);
            break;
            
        case Qt::Key_Y:
            SmartNodeController::smartConvertNodeType(*m_testNode, BezierNodeType::Symmetric);
            drawNode(*m_testNode);
            break;
            
        default:
            QGraphicsView::keyPressEvent(event);
            break;
        }
    }

private:
    bool m_draggingInArm = false;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    SmartNodeDemo demo;
    demo.resize(800, 600);
    demo.show();
    
    qDebug() << "Smart Node Control Demo Started";
    qDebug() << "Use C/S/Y keys to change node type";
    qDebug() << "Drag blue/red dots to test control";
    
    return app.exec();
}

/*
使用说明：

1. 编译运行此演示程序
2. 按C/S/Y键切换节点类型：
   - C: Corner（尖角）- 控制杆独立
   - S: Smooth（平滑）- 控制杆同线，长度可不同  
   - Y: Symmetric（对称）- 控制杆同线，长度相等
3. 拖拽蓝色（IN）或红色（OUT）控制点测试智能控制
4. 观察不同节点类型的控制行为差异

这个演示展示了Inkscape风格的核心智能控制逻辑：
- 自动类型检测和转换
- 智能拖拽约束
- 单杆控制逻辑
- 实时视觉反馈

集成到主项目时，需要：
1. 将SmartNodeController集成到NodeHandleManager
2. 扩展DrawingNodeEditTool的智能控制功能
3. 添加更多的节点操作快捷键
4. 完善视觉样式和用户反馈
*/
