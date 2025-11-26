#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <QObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QLabel>

class DrawingScene;
class DrawingCanvas;
class DrawingView;
class ColorPalette;
class Ruler;
class ScrollableToolBar;

class UIManager : public QObject
{
    Q_OBJECT

public:
    explicit UIManager(QMainWindow *mainWindow, QObject *parent = nullptr);
    ~UIManager();

    // UI组件设置
    void setupUI();
    void setupMenus();
    void setupToolbars();
    void setupDocks();
    void setupStatusBar();
    
    // 组件获取
    QMainWindow* mainWindow() const { return m_mainWindow; }
    DrawingScene* scene() const { return m_scene; }
    DrawingCanvas* canvas() const { return m_canvas; }
    DrawingView* view() const { return m_view; }
    
    // 状态栏组件
    QLabel* statusLabel() const { return m_statusLabel; }
    QLabel* zoomLabel() const { return m_zoomLabel; }
    QLabel* positionLabel() const { return m_positionLabel; }
    
    // 标尺组件
    Ruler* horizontalRuler() const { return m_horizontalRuler; }
    Ruler* verticalRuler() const { return m_verticalRuler; }

signals:
    void uiInitialized();
    void statusMessageChanged(const QString &message);

public slots:
    void updateStatusBar(const QString &message);
    void updateZoomLabel(const QString &zoomText);
    void updatePositionLabel(const QString &positionText);

private:
    void createCentralWidget();
    void createColorPalette();
    void setupRulers();
    void connectUIComponents();
    
    void updateRulers();

private:
    QMainWindow *m_mainWindow;
    
    // 核心组件
    DrawingScene *m_scene;
    DrawingCanvas *m_canvas;
    DrawingView *m_view;
    
    // UI组件
    ColorPalette *m_colorPalette;
    ScrollableToolBar *m_scrollableToolBar;
    
    // 标尺
    Ruler *m_horizontalRuler;
    Ruler *m_verticalRuler;
    QWidget *m_cornerWidget;
    
    // 状态栏
    QLabel *m_statusLabel;
    QLabel *m_zoomLabel;
    QLabel *m_positionLabel;
    
    // 性能面板（保留用于菜单兼容性）
    QDockWidget *m_performanceDock;
};

#endif // UI_MANAGER_H