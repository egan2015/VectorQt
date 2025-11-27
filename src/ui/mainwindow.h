#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QActionGroup>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QColorDialog>
#include <QTimer>
#include <QUndoStack>
#include <QUndoView>
#include <QKeyEvent>

class DrawingScene;
class DrawingShape;
class DrawingView;
class DrawingCanvas;
class ToolBase;
class OutlinePreviewTransformTool;
class PropertyPanel;
class TabbedPropertyPanel;
class ToolsPanel;
class LayerPanel;
class LayerManager;
class Ruler;
class ColorPalette;
class PathEditor;
class ScrollableToolBar;
class ToolStateManager;
class ToolManager;
class ShortcutManager;

class EffectManager;
class SnapManager;
class PathOperationsManager;
class SelectionManager;
class CommandManager;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    // 获取当前填充颜色
    QColor getCurrentFillColor() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    // 文件操作槽函数（委托给FileManager）
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void exportFile();
    
    
    void onStatusMessageChanged(const QString &message);
    void onWindowTitleChanged(const QString &title);
    
    // EffectManager信号处理
    void onEffectApplied(const QString &effectName);
    void onEffectCleared();
    void undo();
    void redo();
    void selectTool();
    void rectangleTool();
    void ellipseTool();
    void bezierTool();
    
    void nodeEditTool();
    void polylineTool();
    void polygonTool();
    void brushTool();
    void fillTool();
    void gradientFillTool();
    void penTool();
    void eraserTool();
    void lineTool();
    void pathEditTool();
    void textTool();
    
    // 工具面板槽函数
    void onBrushSizeChanged(int size);
    void onBrushOpacityChanged(int opacity);
    void onBrushSmoothingChanged(int smoothing);
    void onEraserSizeChanged(int size);
    void onFillToleranceChanged(int tolerance);
    void onStrokeWidthChanged(double width);
    void onAntialiasingChanged(bool enabled);
    void onSnapToGridChanged(bool enabled);
    
    // 钢笔工具特定设置
    void onPenPressureSupportChanged(bool enabled);
    void onPenJoinStyleChanged(int style);
    void onPenCapStyleChanged(int style);
    
    void applyBlurEffect();  // 应用高斯模糊
    void applyDropShadowEffect();  // 应用阴影效果
    void clearFilterEffect();  // 清除滤镜效果
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWindow();
    
    void showGridSettings();
    void about();
    void onSelectionChanged();
    void onSceneChanged();
    void updateZoomLabel();
    void updateRulerSelection();
    void onObjectStateChanged(DrawingShape* shape);
    void updateStatusBar(const QString &message);
    void showContextMenu(const QPointF &pos);
    void onToolSwitchRequested(int toolType);
    void updateToolBarState(int currentTool);
    
    // 调色板颜色应用槽函数
    void onApplyColorToSelection(const QColor &color, bool isFill);
    
    // 🌟 参考线相关槽函数
    void onGuideRequested(const QPointF &position, Qt::Orientation orientation);
    void clearAllGuides();
    
    // 性能面板槽函数
    void togglePerformancePanel();

protected:
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void setupUI();
    void setupMenus();
    void setupToolbars();
    void setupDocks();
    void setupStatusBar();
    void setupColorPalette();
    void setupUndoView();
    void createActions();
    void connectActions();
    void updateUI();
    void setCurrentTool(ToolBase *tool);
    
    // File management methods (从FileManager合并而来)
    bool maybeSave();
    void loadFile(const QString &filePath);
    bool saveFile(const QString &filePath);
    bool exportFile(const QString &filePath);
    void setCurrentFile(const QString &filePath);
    void updateWindowTitle();
    
    
    DrawingScene *m_scene;
    DrawingCanvas *m_canvas;
    PropertyPanel *m_propertyPanel;
    TabbedPropertyPanel *m_tabbedPropertyPanel;
    QUndoView *m_undoView;
    LayerManager *m_layerManager;
    
    // 注意：只有当前工具指针由MainWindow保留，用于快速访问
    // 所有其他工具都由ToolManager管理，这样更安全
    ToolBase *m_currentTool;
    
    // Rulers
    Ruler *m_horizontalRuler;
    Ruler *m_verticalRuler;
    QWidget *m_cornerWidget;
    
    QAction *m_newAction;
    QAction *m_openAction;
    QAction *m_saveAction;
    QAction *m_saveAsAction;
    QAction *m_exportAction;
    QAction *m_exitAction;
    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_outlinePreviewToolAction;
    QAction *m_rectangleToolAction;
    QAction *m_ellipseToolAction;
    QAction *m_bezierToolAction;
    
    QAction *m_nodeEditToolAction;
    QAction *m_polylineToolAction;
    QAction *m_polygonToolAction;
    QAction *m_brushToolAction;
    QAction *m_fillToolAction;
    QAction *m_gradientFillToolAction;
    QAction *m_penToolAction;
    QAction *m_eraserToolAction;
    QAction *m_lineToolAction;
    QAction *m_pathEditToolAction;
    QAction *m_textToolAction;
    QAction *m_pathUnionAction;
    QAction *m_pathSubtractAction;
    QAction *m_pathIntersectAction;
    QAction *m_pathXorAction;
    QAction *m_pathSimplifyAction;
    QAction *m_togglePerformancePanelAction;
    QAction *m_pathSmoothAction;
    QAction *m_pathReverseAction;
    QAction *m_generateShapeAction;
    QAction *m_deleteAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_duplicateAction;
    QAction *m_blurEffectAction;
    QAction *m_dropShadowEffectAction;
    QAction *m_clearFilterAction;
    QAction *m_convertTextToPathAction;
    QAction *m_selectAllAction;
    QAction *m_deselectAllAction;
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_resetZoomAction;
    QAction *m_fitToWindowAction;
    QAction *m_toggleGridAction;
    QAction *m_gridSizeAction;
    QAction *m_gridColorAction;
    QAction *m_toggleGridAlignmentAction;
    QAction *m_clearAllGuidesAction;
    QAction *m_groupAction;
    QAction *m_ungroupAction;
    QAction *m_bringToFrontAction;
    QAction *m_sendToBackAction;
    QAction *m_bringForwardAction;
    QAction *m_sendBackwardAction;
    QAction *m_alignLeftAction;
    QAction *m_alignCenterAction;
    QAction *m_alignRightAction;
    QAction *m_alignTopAction;
    QAction *m_alignMiddleAction;
    QAction *m_alignBottomAction;
    QAction *m_sameWidthAction;
    QAction *m_sameHeightAction;
    QAction *m_sameSizeAction;
    QAction *m_distributeHorizontalAction;
    QAction *m_distributeVerticalAction;
    QAction *m_aboutAction;
    QActionGroup *m_toolGroup;
    
    // Color palette
    ColorPalette *m_colorPalette;
    
    // Scrollable toolbar
    ScrollableToolBar *m_scrollableToolBar;
    
    // Tool management system
    ToolStateManager *m_toolStateManager;
    ToolManager *m_toolManager;
    ShortcutManager *m_shortcutManager;
    
    
    
    // Effect management system
    EffectManager *m_effectManager;
    
    // Snap management system (合并了GridManager)
    SnapManager *m_snapManager;
    
    // Selection management system
    PathOperationsManager *m_pathOperationsManager;
    
    // Selection management system
    SelectionManager *m_selectionManager;
    
    // Command management system
    CommandManager *m_commandManager;
    
    // File management system (从FileManager合并而来)
    QString m_currentFilePath;
    bool m_isUntitled;
    
    QLabel *m_statusLabel;
    QLabel *m_zoomLabel;
    QLabel *m_positionLabel;
    
    // 性能面板（现在集成在属性面板tab中）
    QDockWidget *m_performanceDock; // 保留用于菜单兼容性
    
    // UI更新定时器
    QTimer *m_uiUpdateTimer;
    int m_lastSelectedCount;
    
    
};

#endif // MAINWINDOW_H
