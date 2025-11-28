# 智能吸附提示系统修复方案

## 🔍 问题分析

### 当前问题
1. **绘制被注释**：在`DrawingScene::drawForeground()`中吸附指示器绘制被注释掉了
2. **残影问题**：清除逻辑不完善，导致移动时留下残影
3. **分离后集成问题**：功能分离后，视觉反馈没有正确集成

### 根本原因
虽然`SnapManager`负责计算吸附，`DrawingScene`有完整的绘制方法，但两者没有正确连接。

## 🚀 解决方案

### 1. 重新启用指示器绘制

**问题文件**：`src/ui/drawingscene.cpp` 第853-859行

```cpp
// 问题：被注释掉的绘制代码
// void DrawingScene::drawForeground(QPainter *painter, const QRectF &rect)
// {
//     // 吸附指示器已禁用
//     if (m_snapManager && m_snapManager->areSnapIndicatorsVisible()) {
//         // 让SnapManager处理吸附指示器的绘制
//         if (m_snapManager->hasActiveSnap()) {
//             drawSnapIndicators(painter);
//         }
//     }
// }
```

**修复方案**：
```cpp
void DrawingScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    // 重新启用吸附指示器绘制
    if (m_snapManager && m_snapManager->areSnapIndicatorsVisible()) {
        if (m_snapManager->hasActiveSnap()) {
            drawSnapIndicators(painter);
        }
    }
    
    // 其他绘制代码...
}
```

### 2. 改进的吸附指示器系统

#### A. 增强的指示器管理器

```cpp
class SnapIndicatorManager {
public:
    SnapIndicatorManager(DrawingScene* scene) : m_scene(scene) {}
    
    // 智能显示指示器
    void showSmartSnapIndicators(const ObjectSnapResult& snapResult) {
        clearAllIndicators();
        m_lastSnapResult = snapResult;
        m_hasActiveSnap = true;
        
        // 根据吸附类型创建不同的指示器
        switch (snapResult.snapType) {
        case SnapToLeft:
        case SnapToRight:
        case SnapToTop:
        case SnapToBottom:
            createEdgeSnapIndicators(snapResult);
            break;
        case SnapToCenterX:
        case SnapToCenterY:
            createCenterSnapIndicators(snapResult);
            break;
        case SnapToCenter:
            createCenterSnapIndicators(snapResult);
            break;
        case SnapToCorner:
            createCornerSnapIndicators(snapResult);
            break;
        }
        
        m_scene->update();
    }
    
    // 清除过期指示器
    void clearExpiredIndicators(const QPointF& currentPos) {
        if (!m_hasActiveSnap) return;
        
        qreal distance = QLineF(currentPos, m_lastSnapResult.snappedPos).length();
        if (distance > SNAP_CLEAR_THRESHOLD) {
            clearAllIndicators();
        }
    }
    
    // 强制清除所有指示器
    void clearAllIndicators() {
        for (auto indicator : m_indicators) {
            if (indicator && indicator->scene() == m_scene) {
                m_scene->removeItem(indicator);
                delete indicator;
            }
        }
        m_indicators.clear();
        m_hasActiveSnap = false;
    }

private:
    void createEdgeSnapIndicators(const ObjectSnapResult& snapResult) {
        QColor snapColor = QColor(255, 0, 100);
        
        // 创建吸附点
        auto snapPoint = new QGraphicsEllipseItem();
        snapPoint->setRect(snapResult.snappedPos.x() - 3, snapResult.snappedPos.y() - 3, 6, 6);
        snapPoint->setBrush(QBrush(snapColor));
        snapPoint->setPen(QPen(snapColor, 1));
        snapPoint->setZValue(1000); // 确保在最上层
        
        m_indicators.append(snapPoint);
        m_scene->addItem(snapPoint);
        
        // 创建吸附线
        createSnapLine(snapResult, snapColor);
    }
    
    void createCenterSnapIndicators(const ObjectSnapResult& snapResult) {
        QColor snapColor = QColor(0, 255, 0); // 绿色表示中心吸附
        
        // 创建中心标记
        auto centerMarker = new QGraphicsItemGroup();
        
        // 水平线
        QGraphicsLineItem* hLine = new QGraphicsLineItem(-8, 0, 8, 0);
        hLine->setPen(QPen(snapColor, 2));
        centerMarker->addToGroup(hLine);
        
        // 垂直线
        QGraphicsLineItem* vLine = new QGraphicsLineItem(0, -8, 0, 8);
        vLine->setPen(QPen(snapColor, 2));
        centerMarker->addToGroup(vLine);
        
        centerMarker->setPos(snapResult.snappedPos);
        centerMarker->setZValue(1000);
        
        m_indicators.append(centerMarker);
        m_scene->addItem(centerMarker);
        
        // 创建连接线
        createSnapLine(snapResult, snapColor);
    }
    
    void createCornerSnapIndicators(const ObjectSnapResult& snapResult) {
        QColor snapColor = QColor(255, 255, 0); // 黄色表示角点吸附
        
        // 创建角点标记（菱形）
        QPolygonF diamond;
        diamond << QPointF(0, -6) << QPointF(6, 0) 
                << QPointF(0, 6) << QPointF(-6, 0);
        
        auto cornerMarker = new QGraphicsPolygonItem(diamond);
        cornerMarker->setBrush(QBrush(snapColor));
        cornerMarker->setPen(QPen(snapColor, 1));
        cornerMarker->setPos(snapResult.snappedPos);
        cornerMarker->setZValue(1000);
        
        m_indicators.append(cornerMarker);
        m_scene->addItem(cornerMarker);
        
        // 创建连接线
        createSnapLine(snapResult, snapColor);
    }
    
    void createSnapLine(const ObjectSnapResult& snapResult, const QColor& color) {
        if (!snapResult.targetShape) return;
        
        QRectF targetBounds = snapResult.targetShape->boundingRect();
        QRectF targetSceneBounds = snapResult.targetShape->mapRectToScene(targetBounds);
        QPointF targetCenter = targetSceneBounds.center();
        
        auto line = new QGraphicsLineItem();
        line->setLine(QLineF(snapResult.snappedPos, targetCenter));
        line->setPen(QPen(color.lighter(120), 1.5, Qt::DashLine));
        line->setZValue(999); // 略低于标记点
        
        m_indicators.append(line);
        m_scene->addItem(line);
    }

private:
    DrawingScene* m_scene;
    QList<QGraphicsItem*> m_indicators;
    ObjectSnapResult m_lastSnapResult;
    bool m_hasActiveSnap = false;
    
    static constexpr qreal SNAP_CLEAR_THRESHOLD = 10.0;
};
```

#### B. 改进的SnapManager集成

```cpp
// 在SnapManager中添加指示器管理器
class SnapManager {
private:
    QScopedPointer<SnapIndicatorManager> m_indicatorManager;

public:
    SnapManager(DrawingScene* scene, QObject* parent = nullptr)
        : QObject(parent)
        , m_scene(scene)
        , m_indicatorManager(new SnapIndicatorManager(scene))
    {
        // 指示器管理器会自动处理绘制和清理
    }
    
    void showSnapIndicators(const ObjectSnapResult& snapResult) {
        if (m_snapIndicatorsVisible) {
            m_indicatorManager->showSmartSnapIndicators(snapResult);
        }
    }
    
    void clearSnapIndicators() {
        m_indicatorManager->clearAllIndicators();
        m_hasActiveSnap = false;
    }
    
    void clearExpiredSnapIndicators(const QPointF& currentPos) {
        m_indicatorManager->clearExpiredIndicators(currentPos);
    }
};
```

### 3. 优化残影清除机制

#### 智能清除策略

```cpp
class SmartIndicatorCleanup {
public:
    // 根据移动速度智能调整清除阈值
    static void smartCleanup(DrawingScene* scene, const QPointF& lastPos, 
                           const QPointF& currentPos, const QPointF& velocity) {
        // 高速移动时，降低清除阈值以减少残影
        qreal speed = velocity.length();
        qreal dynamicThreshold = calculateDynamicThreshold(speed);
        
        qreal distance = QLineF(lastPos, currentPos).length();
        if (distance > dynamicThreshold) {
            // 清除过期指示器
            scene->clearExpiredSnapIndicators(currentPos);
        }
    }

private:
    static qreal calculateDynamicThreshold(qreal speed) {
        if (speed > 100) return 5.0;      // 高速移动
        if (speed > 50) return 8.0;       // 中速移动
        return 12.0;                      // 低速移动
    }
};
```

#### 动画效果指示器

```cpp
class AnimatedSnapIndicator : public QGraphicsItem {
public:
    AnimatedSnapIndicator(const QPointF& pos, const QColor& color)
        : m_pos(pos), m_color(color), m_animationPhase(0) {
        startAnimation();
    }
    
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        
        // 脉冲动画效果
        qreal pulseScale = 1.0 + 0.3 * std::sin(m_animationPhase);
        qreal alpha = 1.0 - 0.5 * std::sin(m_animationPhase);
        
        QColor animatedColor = m_color;
        animatedColor.setAlphaF(alpha * 255);
        
        painter->setPen(QPen(animatedColor, 2));
        painter->setBrush(QBrush(animatedColor));
        
        qreal size = 8.0 * pulseScale;
        painter->drawEllipse(QRectF(-size/2, -size/2, size, size));
    }
    
    QRectF boundingRect() const override {
        return QRectF(-15, -15, 30, 30);
    }

private:
    void startAnimation() {
        // 使用QTimer实现动画
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_animationPhase += 0.2;
            update();
        });
        m_timer.start(50); // 20 FPS
    }
    
    QPointF m_pos;
    QColor m_color;
    qreal m_animationPhase;
    QTimer m_timer;
};
```

### 4. 完整的集成方案

#### 增强的DrawingScene

```cpp
class DrawingScene {
private:
    QScopedPointer<SnapIndicatorManager> m_snapIndicatorManager;

public:
    void enableAdvancedSnapIndicators(bool enabled) {
        if (enabled) {
            if (!m_snapIndicatorManager) {
                m_snapIndicatorManager.reset(new SnapIndicatorManager(this));
            }
        } else {
            m_snapIndicatorManager.reset();
        }
    }
    
    void updateSnapIndicators(const ObjectSnapResult& snapResult) {
        if (m_snapIndicatorManager) {
            m_snapIndicatorManager->showSmartSnapIndicators(snapResult);
        }
    }
    
    void clearSnapIndicators() {
        if (m_snapIndicatorManager) {
            m_snapIndicatorManager->clearAllIndicators();
        }
    }

protected:
    void drawForeground(QPainter* painter, const QRectF& rect) override {
        // 绘制吸附指示器
        if (m_snapIndicatorManager && m_snapIndicatorManager->hasActiveSnap()) {
            m_snapIndicatorManager->paint(painter);
        }
        
        // 绘制其他前景元素...
        QGraphicsScene::drawForeground(painter, rect);
    }
};
```

## 🎯 实施步骤

### 步骤1：立即修复（5分钟）
```bash
# 取消注释drawForeground中的吸附指示器绘制
sed -i '853,859s/^\/\/ //' src/ui/drawingscene.cpp
```

### 步骤2：改进清除机制（30分钟）
1. 在SnapManager中添加`clearExpiredSnapIndicators`调用
2. 在工具类的鼠标移动事件中添加清除逻辑

### 步骤3：增强功能（1小时）
1. 创建`SnapIndicatorManager`类
2. 实现动画指示器
3. 添加智能清除机制

### 步骤4：测试和优化（30分钟）
1. 测试各种吸附场景
2. 调整清除阈值和动画参数
3. 性能优化

## 💡 关键改进点

1. **即时清除**：移动时立即清除过期指示器
2. **智能动画**：脉冲效果增强视觉反馈
3. **类型区分**：不同吸附类型使用不同颜色和形状
4. **性能优化**：使用ItemGroup减少绘制调用
5. **动态阈值**：根据移动速度调整清除策略

实施后，吸附指示器将提供：
- ✅ 清晰的无残影体验
- ✅ 实时的视觉反馈
- ✅ 专业的绘图工具感觉
- ✅ 优秀的性能表现