#ifndef DRAWINGVIEW_H
#define DRAWINGVIEW_H

#include <QGraphicsView>

class ToolBase;
class ToolManager;


// 前向声明
enum class ToolType : int;

class DrawingView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DrawingView(QGraphicsScene *scene, QWidget *parent = nullptr);
    
    double zoomLevel() const { return m_zoomLevel; }
    void setZoomLevel(double zoom);
    void setCurrentTool(ToolBase *tool);
    ToolBase* currentTool() const { return m_currentTool; }
    
    // 设置工具管理器和状态机
    void setToolManager(ToolManager* toolManager);
    
    
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWindow();
    
    // 设置光标样式
    void setCursorForTool(ToolBase *tool);

signals:
    void zoomChanged(double zoom);
    void mousePositionChanged(const QPointF &pos);
    void viewportChanged(); // 视口变化信号，用于更新标尺

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private slots:
    void onToolSwitchRequested(ToolType newTool);
    
private:
    void updateZoomLabel();
    
    double m_zoomLevel;
    ToolBase *m_currentTool;
    ToolManager* m_toolManager;
    
};

#endif // DRAWINGVIEW_H