#include "action-manager.h"
#include "drawingscene.h"
#include "../core/drawing-canvas.h"
#include "file-manager.h"
#include "effect-manager.h"
#include "selection-manager.h"
#include "path-operations-manager.h"
#include "tool-controller.h"
#include "command-manager.h"
#include <QApplication>
#include <QMessageBox>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>

ActionManager::ActionManager(QMainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_scene(nullptr)
    , m_canvas(nullptr)
    , m_fileManager(nullptr)
    , m_effectManager(nullptr)
    , m_selectionManager(nullptr)
    , m_pathOperationsManager(nullptr)
    , m_toolController(nullptr)
{
}

ActionManager::~ActionManager()
{
}

void ActionManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
}

void ActionManager::setCanvas(DrawingCanvas *canvas)
{
    m_canvas = canvas;
}

void ActionManager::setFileManager(QObject *fileManager)
{
    m_fileManager = fileManager;
}

void ActionManager::setEffectManager(QObject *effectManager)
{
    m_effectManager = effectManager;
}

void ActionManager::setSelectionManager(QObject *selectionManager)
{
    m_selectionManager = selectionManager;
}

void ActionManager::setPathOperationsManager(QObject *pathOperationsManager)
{
    m_pathOperationsManager = pathOperationsManager;
}

void ActionManager::setToolController(QObject *toolController)
{
    m_toolController = toolController;
}

QAction* ActionManager::action(const QString &name) const
{
    return m_actions.value(name, nullptr);
}

QList<QAction*> ActionManager::allActions() const
{
    return m_actions.values();
}

void ActionManager::createActions()
{
    createFileActions();
    createEditActions();
    createViewActions();
    createToolActions();
    createAlignActions();
    onEffectActions();
    createPathActions();
    createHelpActions();
}

void ActionManager::setupShortcuts()
{
    // 文件操作快捷键
    m_newAction->setShortcut(QKeySequence::New);
    m_openAction->setShortcut(QKeySequence::Open);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_exportAction->setShortcut(QKeySequence("Ctrl+E"));
    
    // 编辑操作快捷键
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_duplicateAction->setShortcut(QKeySequence("Ctrl+D"));
    
    // 视图操作快捷键
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    m_resetZoomAction->setShortcut(QKeySequence("Ctrl+0"));
    m_fitToWindowAction->setShortcut(QKeySequence("Ctrl+F"));
    
    // 工具快捷键
    m_selectToolAction->setShortcut(QKeySequence("V"));
    m_rectangleToolAction->setShortcut(QKeySequence("R"));
    m_ellipseToolAction->setShortcut(QKeySequence("E"));
    m_bezierToolAction->setShortcut(QKeySequence("B"));
    m_textToolAction->setShortcut(QKeySequence("T"));
    m_brushToolAction->setShortcut(QKeySequence("P"));
    m_eraserToolAction->setShortcut(QKeySequence("X"));
}

void ActionManager::connectActions()
{
    // 文件操作
    connect(m_newAction, &QAction::triggered, this, &ActionManager::onNewFile);
    connect(m_openAction, &QAction::triggered, this, &ActionManager::onOpenFile);
    connect(m_saveAction, &QAction::triggered, this, &ActionManager::onSaveFile);
    connect(m_saveAsAction, &QAction::triggered, this, &ActionManager::onSaveFileAs);
    connect(m_exportAction, &QAction::triggered, this, &ActionManager::onExportFile);
    connect(m_exitAction, &QAction::triggered, this, &ActionManager::onExit);
    
    // 编辑操作
    connect(m_undoAction, &QAction::triggered, this, &ActionManager::onUndo);
    connect(m_redoAction, &QAction::triggered, this, &ActionManager::onRedo);
    connect(m_selectAllAction, &QAction::triggered, this, &ActionManager::onSelectAll);
    connect(m_deselectAllAction, &QAction::triggered, this, &ActionManager::onDeselectAll);
    connect(m_deleteAction, &QAction::triggered, this, &ActionManager::onDeleteSelected);
    connect(m_copyAction, &QAction::triggered, this, &ActionManager::onCopySelected);
    connect(m_pasteAction, &QAction::triggered, this, &ActionManager::onPaste);
    connect(m_duplicateAction, &QAction::triggered, this, &ActionManager::onDuplicate);
    
    // 视图操作
    connect(m_zoomInAction, &QAction::triggered, this, &ActionManager::onZoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &ActionManager::onZoomOut);
    connect(m_resetZoomAction, &QAction::triggered, this, &ActionManager::onResetZoom);
    connect(m_fitToWindowAction, &QAction::triggered, this, &ActionManager::onFitToWindow);
    connect(m_toggleGridAction, &QAction::triggered, this, &ActionManager::onToggleGrid);
    connect(m_toggleGridAlignmentAction, &QAction::triggered, this, &ActionManager::onToggleGridAlignment);
    
    // 工具切换
    connect(m_selectToolAction, &QAction::triggered, this, &ActionManager::onSelectTool);
    connect(m_rectangleToolAction, &QAction::triggered, this, &ActionManager::onRectangleTool);
    connect(m_ellipseToolAction, &QAction::triggered, this, &ActionManager::onEllipseTool);
    connect(m_bezierToolAction, &QAction::triggered, this, &ActionManager::onBezierTool);
    connect(m_nodeEditToolAction, &QAction::triggered, this, &ActionManager::onNodeEditTool);
    connect(m_polylineToolAction, &QAction::triggered, this, &ActionManager::onPolylineTool);
    connect(m_polygonToolAction, &QAction::triggered, this, &ActionManager::onPolygonTool);
    connect(m_brushToolAction, &QAction::triggered, this, &ActionManager::onBrushTool);
    connect(m_fillToolAction, &QAction::triggered, this, &ActionManager::onFillTool);
    connect(m_gradientFillToolAction, &QAction::triggered, this, &ActionManager::onGradientFillTool);
    connect(m_penToolAction, &QAction::triggered, this, &ActionManager::onPenTool);
    connect(m_eraserToolAction, &QAction::triggered, this, &ActionManager::onEraserTool);
    connect(m_lineToolAction, &QAction::triggered, this, &ActionManager::onLineTool);
    connect(m_pathEditToolAction, &QAction::triggered, this, &ActionManager::onPathEditTool);
    connect(m_textToolAction, &QAction::triggered, this, &ActionManager::onTextTool);
    
    // 对齐操作
    connect(m_alignLeftAction, &QAction::triggered, this, &ActionManager::onAlignLeft);
    connect(m_alignCenterAction, &QAction::triggered, this, &ActionManager::onAlignCenter);
    connect(m_alignRightAction, &QAction::triggered, this, &ActionManager::onAlignRight);
    connect(m_alignTopAction, &QAction::triggered, this, &ActionManager::onAlignTop);
    connect(m_alignMiddleAction, &QAction::triggered, this, &ActionManager::onAlignMiddle);
    connect(m_alignBottomAction, &QAction::triggered, this, &ActionManager::onAlignBottom);
    connect(m_distributeHorizontalAction, &QAction::triggered, this, &ActionManager::onDistributeHorizontal);
    connect(m_distributeVerticalAction, &QAction::triggered, this, &ActionManager::onDistributeVertical);
    
    // 层级操作
    connect(m_bringToFrontAction, &QAction::triggered, this, &ActionManager::onBringToFront);
    connect(m_sendToBackAction, &QAction::triggered, this, &ActionManager::onSendToBack);
    connect(m_bringForwardAction, &QAction::triggered, this, &ActionManager::onBringForward);
    connect(m_sendBackwardAction, &QAction::triggered, this, &ActionManager::onSendBackward);
    
    // 效果操作
    connect(m_applyBlurEffectAction, &QAction::triggered, this, &ActionManager::onApplyBlurEffect);
    connect(m_applyDropShadowEffectAction, &QAction::triggered, this, &ActionManager::onApplyDropShadowEffect);
    connect(m_clearFilterEffectAction, &QAction::triggered, this, &ActionManager::onClearFilterEffect);
    
    // 路径操作
    connect(m_pathUnionAction, &QAction::triggered, this, &ActionManager::onPathUnion);
    connect(m_pathSubtractAction, &QAction::triggered, this, &ActionManager::onPathSubtract);
    connect(m_pathIntersectAction, &QAction::triggered, this, &ActionManager::onPathIntersect);
    connect(m_pathXorAction, &QAction::triggered, this, &ActionManager::onPathXor);
    connect(m_pathSimplifyAction, &QAction::triggered, this, &ActionManager::onPathSimplify);
    connect(m_pathSmoothAction, &QAction::triggered, this, &ActionManager::onPathSmooth);
    connect(m_pathReverseAction, &QAction::triggered, this, &ActionManager::onPathReverse);
    
    // 帮助
    connect(m_aboutAction, &QAction::triggered, this, &ActionManager::onAbout);
}

// 槽函数实现
void ActionManager::onNewFile()
{
    if (m_fileManager) {
        QMetaObject::invokeMethod(m_fileManager, "newFile");
    }
    emit actionTriggered("newFile");
}

void ActionManager::onOpenFile()
{
    if (m_fileManager) {
        QMetaObject::invokeMethod(m_fileManager, "openFile");
    }
    emit actionTriggered("openFile");
}

void ActionManager::onSaveFile()
{
    if (m_fileManager) {
        QMetaObject::invokeMethod(m_fileManager, "saveFile");
    }
    emit actionTriggered("saveFile");
}

void ActionManager::onSaveFileAs()
{
    if (m_fileManager) {
        QMetaObject::invokeMethod(m_fileManager, "saveFileAs");
    }
    emit actionTriggered("saveFileAs");
}

void ActionManager::onExportFile()
{
    if (m_fileManager) {
        QMetaObject::invokeMethod(m_fileManager, "exportFile");
    }
    emit actionTriggered("exportFile");
}

void ActionManager::onExit()
{
    m_mainWindow->close();
    emit actionTriggered("exit");
}

void ActionManager::onUndo()
{
    if (m_scene && m_scene->commandManager()) {
        m_scene->commandManager()->undo();
    }
    emit actionTriggered("undo");
}

void ActionManager::onRedo()
{
    if (m_scene && m_scene->commandManager()) {
        m_scene->commandManager()->redo();
    }
    emit actionTriggered("redo");
}

void ActionManager::onSelectAll()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "selectAll");
    }
    emit actionTriggered("selectAll");
}

void ActionManager::onDeselectAll()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "deselectAll");
    }
    emit actionTriggered("deselectAll");
}

void ActionManager::onDeleteSelected()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "deleteSelected");
    }
    emit actionTriggered("deleteSelected");
}

void ActionManager::onCopySelected()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "copySelected");
    }
    emit actionTriggered("copySelected");
}

void ActionManager::onPaste()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "paste");
    }
    emit actionTriggered("paste");
}

void ActionManager::onDuplicate()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "duplicate");
    }
    emit actionTriggered("duplicate");
}

void ActionManager::onZoomIn()
{
    if (m_canvas) {
        m_canvas->zoomIn();
    }
    emit actionTriggered("zoomIn");
}

void ActionManager::onZoomOut()
{
    if (m_canvas) {
        m_canvas->zoomOut();
    }
    emit actionTriggered("zoomOut");
}

void ActionManager::onResetZoom()
{
    if (m_canvas) {
        m_canvas->resetZoom();
    }
    emit actionTriggered("resetZoom");
}

void ActionManager::onFitToWindow()
{
    if (m_canvas) {
        m_canvas->fitToWindow();
    }
    emit actionTriggered("fitToWindow");
}

void ActionManager::onToggleGrid()
{
    // TODO: 实现网格切换逻辑
    emit actionTriggered("toggleGrid");
}

void ActionManager::onToggleGridAlignment()
{
    // TODO: 实现网格对齐切换逻辑
    emit actionTriggered("toggleGridAlignment");
}

// 工具切换槽函数
void ActionManager::onSelectTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "selectTool");
    }
    emit actionTriggered("selectTool");
}

void ActionManager::onRectangleTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "rectangleTool");
    }
    emit actionTriggered("rectangleTool");
}

void ActionManager::onEllipseTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "ellipseTool");
    }
    emit actionTriggered("ellipseTool");
}

void ActionManager::onBezierTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "bezierTool");
    }
    emit actionTriggered("bezierTool");
}

void ActionManager::onNodeEditTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "nodeEditTool");
    }
    emit actionTriggered("nodeEditTool");
}

void ActionManager::onPolylineTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "polylineTool");
    }
    emit actionTriggered("polylineTool");
}

void ActionManager::onPolygonTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "polygonTool");
    }
    emit actionTriggered("polygonTool");
}

void ActionManager::onBrushTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "brushTool");
    }
    emit actionTriggered("brushTool");
}

void ActionManager::onFillTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "fillTool");
    }
    emit actionTriggered("fillTool");
}

void ActionManager::onGradientFillTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "gradientFillTool");
    }
    emit actionTriggered("gradientFillTool");
}

void ActionManager::onPenTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "penTool");
    }
    emit actionTriggered("penTool");
}

void ActionManager::onEraserTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "eraserTool");
    }
    emit actionTriggered("eraserTool");
}

void ActionManager::onLineTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "lineTool");
    }
    emit actionTriggered("lineTool");
}

void ActionManager::onPathEditTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "pathEditTool");
    }
    emit actionTriggered("pathEditTool");
}

void ActionManager::onTextTool()
{
    if (m_toolController) {
        QMetaObject::invokeMethod(m_toolController, "textTool");
    }
    emit actionTriggered("textTool");
}

// 对齐操作槽函数
void ActionManager::onAlignLeft()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "alignLeft");
    }
    emit actionTriggered("alignLeft");
}

void ActionManager::onAlignCenter()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "alignCenter");
    }
    emit actionTriggered("alignCenter");
}

void ActionManager::onAlignRight()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "alignRight");
    }
    emit actionTriggered("alignRight");
}

void ActionManager::onAlignTop()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "alignTop");
    }
    emit actionTriggered("alignTop");
}

void ActionManager::onAlignMiddle()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "alignMiddle");
    }
    emit actionTriggered("alignMiddle");
}

void ActionManager::onAlignBottom()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "alignBottom");
    }
    emit actionTriggered("alignBottom");
}

void ActionManager::onDistributeHorizontal()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "distributeHorizontal");
    }
    emit actionTriggered("distributeHorizontal");
}

void ActionManager::onDistributeVertical()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "distributeVertical");
    }
    emit actionTriggered("distributeVertical");
}

// 层级操作槽函数
void ActionManager::onBringToFront()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "bringToFront");
    }
    emit actionTriggered("bringToFront");
}

void ActionManager::onSendToBack()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "sendToBack");
    }
    emit actionTriggered("sendToBack");
}

void ActionManager::onBringForward()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "bringForward");
    }
    emit actionTriggered("bringForward");
}

void ActionManager::onSendBackward()
{
    if (m_selectionManager) {
        QMetaObject::invokeMethod(m_selectionManager, "sendBackward");
    }
    emit actionTriggered("sendBackward");
}

// 效果操作槽函数
void ActionManager::onApplyBlurEffect()
{
    if (m_effectManager) {
        QMetaObject::invokeMethod(m_effectManager, "applyBlurEffect");
    }
    emit actionTriggered("applyBlurEffect");
}

void ActionManager::onApplyDropShadowEffect()
{
    if (m_effectManager) {
        QMetaObject::invokeMethod(m_effectManager, "applyDropShadowEffect");
    }
    emit actionTriggered("applyDropShadowEffect");
}

void ActionManager::onClearFilterEffect()
{
    if (m_effectManager) {
        QMetaObject::invokeMethod(m_effectManager, "clearFilterEffect");
    }
    emit actionTriggered("clearFilterEffect");
}

// 路径操作槽函数
void ActionManager::onPathUnion()
{
    if (m_pathOperationsManager) {
        QMetaObject::invokeMethod(m_pathOperationsManager, "pathUnion");
    }
    emit actionTriggered("pathUnion");
}

void ActionManager::onPathSubtract()
{
    if (m_pathOperationsManager) {
        QMetaObject::invokeMethod(m_pathOperationsManager, "pathSubtract");
    }
    emit actionTriggered("pathSubtract");
}

void ActionManager::onPathIntersect()
{
    if (m_pathOperationsManager) {
        QMetaObject::invokeMethod(m_pathOperationsManager, "pathIntersect");
    }
    emit actionTriggered("pathIntersect");
}

void ActionManager::onPathXor()
{
    if (m_pathOperationsManager) {
        QMetaObject::invokeMethod(m_pathOperationsManager, "pathXor");
    }
    emit actionTriggered("pathXor");
}

void ActionManager::onPathSimplify()
{
    if (m_pathOperationsManager) {
        QMetaObject::invokeMethod(m_pathOperationsManager, "pathSimplify");
    }
    emit actionTriggered("pathSimplify");
}

void ActionManager::onPathSmooth()
{
    if (m_pathOperationsManager) {
        QMetaObject::invokeMethod(m_pathOperationsManager, "pathSmooth");
    }
    emit actionTriggered("pathSmooth");
}

void ActionManager::onPathReverse()
{
    if (m_pathOperationsManager) {
        QMetaObject::invokeMethod(m_pathOperationsManager, "pathReverse");
    }
    emit actionTriggered("pathReverse");
}

void ActionManager::onAbout()
{
    QMessageBox::about(m_mainWindow, "关于 VectorQt", 
                      "VectorQt 是一个强大的矢量绘图应用\n\n"
                      "版本: 1.0\n"
                      "基于 Qt 框架开发");
    emit actionTriggered("about");
}

// 私有方法实现
void ActionManager::createFileActions()
{
    m_newAction = new QAction("新建(&N)", this);
    m_openAction = new QAction("打开(&O)...", this);
    m_saveAction = new QAction("保存(&S)", this);
    m_saveAsAction = new QAction("另存为(&A)...", this);
    m_exportAction = new QAction("导出(&E)...", this);
    m_exitAction = new QAction("退出(&X)", this);
    
    m_actions["new"] = m_newAction;
    m_actions["open"] = m_openAction;
    m_actions["save"] = m_saveAction;
    m_actions["saveAs"] = m_saveAsAction;
    m_actions["export"] = m_exportAction;
    m_actions["exit"] = m_exitAction;
}

void ActionManager::createEditActions()
{
    m_undoAction = new QAction("撤销(&U)", this);
    m_redoAction = new QAction("重做(&R)", this);
    m_selectAllAction = new QAction("全选(&A)", this);
    m_deselectAllAction = new QAction("取消全选", this);
    m_deleteAction = new QAction("删除(&Del)", this);
    m_copyAction = new QAction("复制(&C)", this);
    m_pasteAction = new QAction("粘贴(&V)", this);
    m_duplicateAction = new QAction("复制(&D)", this);
    
    m_actions["undo"] = m_undoAction;
    m_actions["redo"] = m_redoAction;
    m_actions["selectAll"] = m_selectAllAction;
    m_actions["deselectAll"] = m_deselectAllAction;
    m_actions["delete"] = m_deleteAction;
    m_actions["copy"] = m_copyAction;
    m_actions["paste"] = m_pasteAction;
    m_actions["duplicate"] = m_duplicateAction;
}

void ActionManager::createViewActions()
{
    m_zoomInAction = new QAction("放大(&I)", this);
    m_zoomOutAction = new QAction("缩小(&O)", this);
    m_resetZoomAction = new QAction("重置缩放(&0)", this);
    m_fitToWindowAction = new QAction("适应窗口(&F)", this);
    m_toggleGridAction = new QAction("显示网格(&G)", this);
    m_toggleGridAlignmentAction = new QAction("网格对齐", this);
    
    m_actions["zoomIn"] = m_zoomInAction;
    m_actions["zoomOut"] = m_zoomOutAction;
    m_actions["resetZoom"] = m_resetZoomAction;
    m_actions["fitToWindow"] = m_fitToWindowAction;
    m_actions["toggleGrid"] = m_toggleGridAction;
    m_actions["toggleGridAlignment"] = m_toggleGridAlignmentAction;
}

void ActionManager::createToolActions()
{
    m_selectToolAction = new QAction("选择工具(&V)", this);
    m_rectangleToolAction = new QAction("矩形工具(&R)", this);
    m_ellipseToolAction = new QAction("椭圆工具(&E)", this);
    m_bezierToolAction = new QAction("贝塞尔工具(&B)", this);
    m_nodeEditToolAction = new QAction("节点编辑(&N)", this);
    m_polylineToolAction = new QAction("折线工具(&L)", this);
    m_polygonToolAction = new QAction("多边形工具(&P)", this);
    m_brushToolAction = new QAction("画笔工具(&P)", this);
    m_fillToolAction = new QAction("填充工具(&F)", this);
    m_gradientFillToolAction = new QAction("渐变填充(&G)", this);
    m_penToolAction = new QAction("钢笔工具(&P)", this);
    m_eraserToolAction = new QAction("橡皮擦(&X)", this);
    m_lineToolAction = new QAction("直线工具(&L)", this);
    m_pathEditToolAction = new QAction("路径编辑(&P)", this);
    m_textToolAction = new QAction("文本工具(&T)", this);
    
    m_actions["selectTool"] = m_selectToolAction;
    m_actions["rectangleTool"] = m_rectangleToolAction;
    m_actions["ellipseTool"] = m_ellipseToolAction;
    m_actions["bezierTool"] = m_bezierToolAction;
    m_actions["nodeEditTool"] = m_nodeEditToolAction;
    m_actions["polylineTool"] = m_polylineToolAction;
    m_actions["polygonTool"] = m_polygonToolAction;
    m_actions["brushTool"] = m_brushToolAction;
    m_actions["fillTool"] = m_fillToolAction;
    m_actions["gradientFillTool"] = m_gradientFillToolAction;
    m_actions["penTool"] = m_penToolAction;
    m_actions["eraserTool"] = m_eraserToolAction;
    m_actions["lineTool"] = m_lineToolAction;
    m_actions["pathEditTool"] = m_pathEditToolAction;
    m_actions["textTool"] = m_textToolAction;
}

void ActionManager::createAlignActions()
{
    m_alignLeftAction = new QAction("左对齐", this);
    m_alignCenterAction = new QAction("水平居中", this);
    m_alignRightAction = new QAction("右对齐", this);
    m_alignTopAction = new QAction("顶对齐", this);
    m_alignMiddleAction = new QAction("垂直居中", this);
    m_alignBottomAction = new QAction("底对齐", this);
    m_distributeHorizontalAction = new QAction("水平分布", this);
    m_distributeVerticalAction = new QAction("垂直分布", this);
    
    m_actions["alignLeft"] = m_alignLeftAction;
    m_actions["alignCenter"] = m_alignCenterAction;
    m_actions["alignRight"] = m_alignRightAction;
    m_actions["alignTop"] = m_alignTopAction;
    m_actions["alignMiddle"] = m_alignMiddleAction;
    m_actions["alignBottom"] = m_alignBottomAction;
    m_actions["distributeHorizontal"] = m_distributeHorizontalAction;
    m_actions["distributeVertical"] = m_distributeVerticalAction;
}

void ActionManager::onEffectActions()
{
    m_applyBlurEffectAction = new QAction("模糊效果", this);
    m_applyDropShadowEffectAction = new QAction("阴影效果", this);
    m_clearFilterEffectAction = new QAction("清除效果", this);
    
    m_actions["applyBlurEffect"] = m_applyBlurEffectAction;
    m_actions["applyDropShadowEffect"] = m_applyDropShadowEffectAction;
    m_actions["clearFilterEffect"] = m_clearFilterEffectAction;
}

void ActionManager::createPathActions()
{
    m_pathUnionAction = new QAction("路径联合", this);
    m_pathSubtractAction = new QAction("路径减去", this);
    m_pathIntersectAction = new QAction("路径相交", this);
    m_pathXorAction = new QAction("路径异或", this);
    m_pathSimplifyAction = new QAction("路径简化", this);
    m_pathSmoothAction = new QAction("路径平滑", this);
    m_pathReverseAction = new QAction("路径反转", this);
    
    m_actions["pathUnion"] = m_pathUnionAction;
    m_actions["pathSubtract"] = m_pathSubtractAction;
    m_actions["pathIntersect"] = m_pathIntersectAction;
    m_actions["pathXor"] = m_pathXorAction;
    m_actions["pathSimplify"] = m_pathSimplifyAction;
    m_actions["pathSmooth"] = m_pathSmoothAction;
    m_actions["pathReverse"] = m_pathReverseAction;
}

void ActionManager::createHelpActions()
{
    m_aboutAction = new QAction("关于(&A)", this);
    m_actions["about"] = m_aboutAction;
}