#ifndef DRAWINGSCENE_H
#define DRAWINGSCENE_H

#include <QGraphicsScene>
#include "../core/drawing-group.h"
#include "../tools/tool-manager.h"

class DrawingShape;
class DrawingGroup;
class QGraphicsSceneMouseEvent;
class CommandManager;
// class SelectionLayer; // 已移除 - 老的选择层系统
class TransformCommand;
class GroupCommand;
class UngroupCommand;
class SnapManager;

class DrawingScene : public QGraphicsScene
{
    Q_OBJECT

public:
    // TransformState 结构体需要在 TransformCommand 类之前定义
    struct TransformState {
        QPointF position;
        QTransform transform;
        qreal rotation;
    };
    
    explicit DrawingScene(QObject *parent = nullptr);
    
    QUndoStack* undoStack();
    
    bool isModified() const { return m_isModified; }
    void setModified(bool modified);
    
    // 公共命令执行方法
    void executeCommand(QUndoCommand *command);
    
    void clearScene();
    
    // CommandManager 访问
    void setCommandManager(CommandManager *commandManager);
    CommandManager* commandManager() const { return m_commandManager; }
    
    // SnapManager 访问
    void setSnapManager(SnapManager *snapManager);
    SnapManager* snapManager() const { return m_snapManager; }
    
    // 选择层管理
    // SelectionLayer* selectionLayer() const { return m_selectionLayer; } // 已移除 - 老的选择层系统
    
    // 激活/停用选择工具时调用
    void activateSelectionTool();
    void deactivateSelectionTool();
    
    // 设置当前工具
    void setCurrentTool(int toolType);
    
    // 变换撤销支持
    enum TransformType {
        Move,
        Scale,
        Rotate,
        Skew,
        Generic
    };
    
    void beginTransform(TransformType type = Generic);
    void endTransform();
    
    // 组合和取消组合操作
    void groupSelectedItems();
    void ungroupSelectedItems();
    void endTransformWithStates(const QList<TransformState>& newStates);
    
    // Z序控制操作
    void bringToFront();
    void sendToBack();
    void bringForward();
    void sendBackward();
    
    // 网格功能
    void setGridVisible(bool visible);
    bool isGridVisible() const;
    void setGridSize(int size);
    int gridSize() const;
    void setGridColor(const QColor &color);
    QColor gridColor() const;
    
    // 网格对齐功能
    
    
    
    
    
    
    
    // 网格对齐开关
    void setGridAlignmentEnabled(bool enabled);
    bool isGridAlignmentEnabled() const;
    
    // Smart snapping settings已移至SnapManager
    
    // Guide line system
    struct Guide {
        Qt::Orientation orientation;
        qreal position;  // 场景坐标中的位置
        QColor color;
        bool visible;
        
        Guide(Qt::Orientation orient, qreal pos, QColor col = QColor(0, 120, 255, 150))
            : orientation(orient), position(pos), color(col), visible(true) {}
    };
    
    // 参考线方法（保留在Scene中）
    void addGuide(Qt::Orientation orientation, qreal position);
    void removeGuide(Qt::Orientation orientation, qreal position);
    void clearGuides();
    QList<Guide> guides() const { return m_guides; }
    void setGuideVisible(Qt::Orientation orientation, qreal position, bool visible);
    
    
    
    
    // Object snapping system已移至SnapManager
    
    
    
    // Scale hint structure
    struct ScaleHintResult {
        bool showHint;
        qreal scaleX;
        qreal scaleY;
        QString hintDescription;
        QPointF hintPosition;
        
        ScaleHintResult() : showHint(false), scaleX(1.0), scaleY(1.0) {}
    };
    
    // Rotate hint structure
    struct RotateHintResult {
        bool showHint;
        qreal angle;
        QString hintDescription;
        QPointF hintPosition;
        
        RotateHintResult() : showHint(false), angle(0.0) {}
    };
    
    // 对象吸附功能已移至SnapManager
    
    // 吸附相关方法已移至SnapManager
    
    // Scale hints
    void showScaleHint(const ScaleHintResult &hintResult);
    void clearScaleHint();
    ScaleHintResult calculateScaleHint(qreal sx, qreal sy, const QPointF &pos);
    
    // Rotate hints
    void showRotateHint(const RotateHintResult &hintResult);
    void clearRotateHint();
    RotateHintResult calculateRotateHint(qreal angle, const QPointF &pos);

private:
    // drawSnapIndicators方法，供SnapManager调用
    void drawSnapIndicators(QPainter *painter);
    
    Q_DISABLE_COPY(DrawingScene)

signals:
    void sceneModified(bool modified);
    void objectStateChanged(DrawingShape* shape); // 对象状态变化通知
    void selectionChanged(); // 选择变化通知
    void sceneAboutToBeCleared(); // 圳景即将被清理通知
    void allToolsClearHandles(); // 通知所有工具清理手柄
    void contextMenuRequested(const QPointF &pos); // 右键菜单请求信号
    void toolSwitchRequested(int toolType); // 工具切换请求信号
    void gridVisibilityChanged(bool visible);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    
    void keyPressEvent(QKeyEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private slots:
    void onSelectionChanged();

public:
    void emitSelectionChanged() { emit selectionChanged(); }

private:
    void drawGrid(QPainter *painter, const QRectF &rect);
    
    
    bool m_isModified;
    // SelectionLayer *m_selectionLayer; // 已移除 - 老的选择层系统
    
    // 网格相关
    bool m_gridVisible;
    bool m_gridAlignmentEnabled;  // 新增：网格对齐开关
    int m_gridSize;
    QColor m_gridColor;
    
    // 参考线属性（保留在Scene中）
    bool m_guidesEnabled;
    QList<Guide> m_guides;
    
    // Scale and rotate hints
    ScaleHintResult m_lastScaleHint;
    RotateHintResult m_lastRotateHint;
    bool m_scaleHintVisible;
    bool m_rotateHintVisible;
    
    // 变换撤销支持
    QList<TransformState> m_transformOldStates;
    QList<DrawingShape*> m_transformShapes;  // 保存变换时的图形引用
    TransformType m_currentTransformType;
    
    // CommandManager 引用
    CommandManager *m_commandManager;
    
    // SnapManager 引用（合并了网格和对象吸附）
    SnapManager *m_snapManager;
    
    // 当前工具类型
    int m_currentTool;
    
private:
    // 辅助方法：推送命令到撤销栈
    void pushCommand(QUndoCommand *command);
    
    
};

#endif // DRAWINGSCENE_H
