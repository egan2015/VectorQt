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
#include <QUndoStack>
#include <QUndoView>

class DrawingScene;
class DrawingView;
class DrawingCanvas;
class ToolBase;
class OutlinePreviewTransformTool;
class PropertyPanel;
class Ruler;
class ColorPalette;
class PathEditor;
class ScrollableToolBar;

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

private slots:
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void exportFile();
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
    // void textTool()  // Not implemented yet
    void deleteSelected();
    void copySelected();
    void paste();
    void duplicate();  // Ctrl+D 快速复制粘贴
    void selectAll();
    void deselectAll();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWindow();
    void toggleGrid();
    void toggleGridAlignment();
    void groupSelected();
    void ungroupSelected();
    void alignLeft();
    void alignCenter();
    void alignRight();
    void alignTop();
    void alignMiddle();
    void alignBottom();
    void distributeHorizontal();
    void distributeVertical();
    void showGridSettings();
    void about();
    void onSelectionChanged();
    void onSceneChanged();
    void updateZoomLabel();
    void updateRulerSelection();
    void updateStatusBar(const QString &message);
    
    // 调色板颜色应用槽函数
    void onApplyColorToSelection(const QColor &color, bool isFill);
    
    // 🌟 参考线相关槽函数
    void onGuideRequested(const QPointF &position, Qt::Orientation orientation);
    void clearAllGuides();
    
    // 路径布尔运算槽函数
    void pathUnion();
    void pathSubtract();
    void pathIntersect();
    void pathXor();
    
    // 执行路径布尔运算的通用方法
    void performPathBooleanOperation(int op, const QString &opName);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void setupUI();
    void setupMenus();
    void setupToolbars();
    void setupDocks();
    void setupStatusBar();
    void setupColorPalette();
    void createActions();
    void connectActions();
    void updateUI();
    void setCurrentTool(ToolBase *tool);
    
    
    DrawingScene *m_scene;
    DrawingCanvas *m_canvas;
    PropertyPanel *m_propertyPanel;
    QUndoView *m_undoView;
    ToolBase *m_currentTool;
    ToolBase *m_outlinePreviewTool;      // 选择工具（轮廓预览变换）
    
    ToolBase *m_rectangleTool;
    ToolBase *m_ellipseTool;
    ToolBase *m_bezierTool;        // 贝塞尔曲线工具
    
    ToolBase *m_nodeEditTool;      // 节点编辑工具
    ToolBase *m_polylineTool;    // 折线工具
    ToolBase *m_polygonTool;     // 多边形工具
    ToolBase *m_brushTool;       // 画笔工具
    ToolBase *m_fillTool;        // 填充工具
    ToolBase *m_gradientFillTool; // 渐进填充工具
    ToolBase *m_penTool;         // 钢笔工具
    ToolBase *m_eraserTool;      // 橡皮擦工具
    ToolBase *m_lineTool;        // 直线工具
    ToolBase *m_pathEditTool;    // 路径编辑工具
    
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
    QAction *m_pathUnionAction;
    QAction *m_pathSubtractAction;
    QAction *m_pathIntersectAction;
    QAction *m_pathXorAction;
    QAction *m_deleteAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_duplicateAction;
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
    QAction *m_alignLeftAction;
    QAction *m_alignCenterAction;
    QAction *m_alignRightAction;
    QAction *m_alignTopAction;
    QAction *m_alignMiddleAction;
    QAction *m_alignBottomAction;
    QAction *m_distributeHorizontalAction;
    QAction *m_distributeVerticalAction;
    QAction *m_aboutAction;
    QActionGroup *m_toolGroup;
    
    // Color palette
    ColorPalette *m_colorPalette;
    
    // Scrollable toolbar
    ScrollableToolBar *m_scrollableToolBar;
    
    QLabel *m_statusLabel;
    QLabel *m_zoomLabel;
    QLabel *m_positionLabel;
    QString m_currentFile;
    bool m_isModified;
};

#endif // MAINWINDOW_H
