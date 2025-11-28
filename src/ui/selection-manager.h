#ifndef SELECTION_MANAGER_H
#define SELECTION_MANAGER_H

#include <QObject>
#include <QList>
#include <QPointF>

class DrawingScene;
class DrawingShape;
class MainWindow;
class CommandManager;

class SelectionManager : public QObject
{
    Q_OBJECT

public:
    explicit SelectionManager(MainWindow *parent = nullptr);
    ~SelectionManager();

    void setScene(DrawingScene *scene);
    DrawingScene *scene() const;
    
    void setCommandManager(CommandManager *commandManager);
    CommandManager *commandManager() const;

    // 获取当前选择
    QList<DrawingShape*> selectedShapes() const;
    bool hasSelection() const;
    int selectionCount() const;

signals:
    void selectionChanged(int count);
    void alignmentCompleted(const QString &alignmentType);
    void distributionCompleted(const QString &distributionType);
    void statusMessageChanged(const QString &message);

public slots:
    // 选择操作
    void selectAll();
    void deselectAll();
    void deleteSelected();
    void copySelected();
    void paste();
    void duplicate();

    // 对齐操作
    void alignLeft();
    void alignCenter();
    void alignRight();
    void alignTop();
    void alignMiddle();
    void alignBottom();

    // 分布操作
    void distributeHorizontal();
    void distributeVertical();
    
    // 尺寸操作
    void sameWidth();
    void sameHeight();
    void sameSize();

    // 层级操作
    void bringToFront();
    void sendToBack();
    void bringForward();
    void sendBackward();

    // 分组操作
    void groupSelected();
    void ungroupSelected();

private:
    // 辅助函数
    QRectF getSelectionBounds() const;
    qreal getLeftmostEdge() const;
    qreal getRightmostEdge() const;
    qreal getTopmostEdge() const;
    qreal getBottommostEdge() const;
    qreal getHorizontalCenter() const;
    qreal getVerticalCenter() const;
    
    QList<qreal> getHorizontalPositions() const;
    QList<qreal> getVerticalPositions() const;
    void distributePositions(QList<qreal> &positions, qreal start, qreal end);

private:
    MainWindow *m_mainWindow;
    DrawingScene *m_scene;
    CommandManager *m_commandManager;
};

#endif // SELECTION_MANAGER_H