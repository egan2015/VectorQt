#ifndef ACTION_MANAGER_H
#define ACTION_MANAGER_H

#include <QObject>
#include <QAction>
#include <QKeySequence>
#include <QMainWindow>

class DrawingScene;
class DrawingCanvas;
class FileManager;
class EffectManager;
class SelectionManager;
class PathOperationsManager;
class ToolController;

class ActionManager : public QObject
{
    Q_OBJECT

public:
    explicit ActionManager(QMainWindow *mainWindow, QObject *parent = nullptr);
    ~ActionManager();

    // 组件设置
    void setScene(DrawingScene *scene);
    void setCanvas(DrawingCanvas *canvas);
    void setFileManager(QObject *fileManager);
    void setEffectManager(QObject *effectManager);
    void setSelectionManager(QObject *selectionManager);
    void setPathOperationsManager(QObject *pathOperationsManager);
    void setToolController(QObject *toolController);

    // 动作获取
    QAction* action(const QString &name) const;
    QList<QAction*> allActions() const;

    // 动作创建和设置
    void createActions();
    void setupShortcuts();
    void connectActions();

signals:
    void actionTriggered(const QString &actionName);
    void statusMessageChanged(const QString &message);

private slots:
    // 文件操作
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onSaveFileAs();
    void onExportFile();
    void onExit();

    // 编辑操作
    void onUndo();
    void onRedo();
    void onSelectAll();
    void onDeselectAll();
    void onDeleteSelected();
    void onCopySelected();
    void onPaste();
    void onDuplicate();

    // 视图操作
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();
    void onFitToWindow();
    void onToggleGrid();
    void onToggleGridAlignment();

    // 工具切换
    void onSelectTool();
    void onRectangleTool();
    void onEllipseTool();
    void onBezierTool();
    void onNodeEditTool();
    void onPolylineTool();
    void onPolygonTool();
    void onBrushTool();
    void onFillTool();
    void onGradientFillTool();
    void onPenTool();
    void onEraserTool();
    void onLineTool();
    void onPathEditTool();
    void onTextTool();

    // 对齐操作
    void onAlignLeft();
    void onAlignCenter();
    void onAlignRight();
    void onAlignTop();
    void onAlignMiddle();
    void onAlignBottom();
    void onDistributeHorizontal();
    void onDistributeVertical();

    // 层级操作
    void onBringToFront();
    void onSendToBack();
    void onBringForward();
    void onSendBackward();

    // 效果操作
    void onApplyBlurEffect();
    void onApplyDropShadowEffect();
    void onClearFilterEffect();

    // 路径操作
    void onPathUnion();
    void onPathSubtract();
    void onPathIntersect();
    void onPathXor();
    void onPathSimplify();
    void onPathSmooth();
    void onPathReverse();

    // 帮助
    void onAbout();

private:
    void createFileActions();
    void createEditActions();
    void createViewActions();
    void createToolActions();
    void createAlignActions();
    void onEffectActions();
    void createPathActions();
    void createHelpActions();

private:
    QMainWindow *m_mainWindow;
    
    // 组件引用
    DrawingScene *m_scene;
    DrawingCanvas *m_canvas;
    QObject *m_fileManager;
    QObject *m_effectManager;
    QObject *m_selectionManager;
    QObject *m_pathOperationsManager;
    QObject *m_toolController;
    
    // 动作映射
    QMap<QString, QAction*> m_actions;
    
    // 文件操作动作
    QAction *m_newAction;
    QAction *m_openAction;
    QAction *m_saveAction;
    QAction *m_saveAsAction;
    QAction *m_exportAction;
    QAction *m_exitAction;
    
    // 编辑操作动作
    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_selectAllAction;
    QAction *m_deselectAllAction;
    QAction *m_deleteAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_duplicateAction;
    
    // 视图操作动作
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_resetZoomAction;
    QAction *m_fitToWindowAction;
    QAction *m_toggleGridAction;
    QAction *m_toggleGridAlignmentAction;
    
    // 工具动作
    QAction *m_selectToolAction;
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
    
    // 对齐动作
    QAction *m_alignLeftAction;
    QAction *m_alignCenterAction;
    QAction *m_alignRightAction;
    QAction *m_alignTopAction;
    QAction *m_alignMiddleAction;
    QAction *m_alignBottomAction;
    QAction *m_distributeHorizontalAction;
    QAction *m_distributeVerticalAction;
    
    // 层级动作
    QAction *m_bringToFrontAction;
    QAction *m_sendToBackAction;
    QAction *m_bringForwardAction;
    QAction *m_sendBackwardAction;
    
    // 效果动作
    QAction *m_applyBlurEffectAction;
    QAction *m_applyDropShadowEffectAction;
    QAction *m_clearFilterEffectAction;
    
    // 路径动作
    QAction *m_pathUnionAction;
    QAction *m_pathSubtractAction;
    QAction *m_pathIntersectAction;
    QAction *m_pathXorAction;
    QAction *m_pathSimplifyAction;
    QAction *m_pathSmoothAction;
    QAction *m_pathReverseAction;
    
    // 帮助动作
    QAction *m_aboutAction;
};

#endif // ACTION_MANAGER_H