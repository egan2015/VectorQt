#include "mainwindow.h"
#include "drawingscene.h"
#include "drawingview.h"
#include "drawing-canvas.h"
#include "toolbase.h"
#include "propertypanel.h"
#include "drawing-tool-bezier.h"
#include "drawing-tool-bezier-edit.h"
#include "drawing-tool-node-edit.h"
#include "drawing-tool-polyline.h"
#include "drawing-tool-polygon.h"
#include "drawing-tool-brush.h"
#include "drawing-tool-fill.h"
#include "drawing-tool-gradient-fill.h"
#include "drawing-tool-pen.h"
#include "drawing-tool-eraser.h"
#include "drawing-tool-line.h"
#include "drawing-tool-path-edit.h"
#include "drawing-tool-outline-preview.h"
#include "patheditor.h"
#include "selection-layer.h"
#include "ruler.h"
#include "scrollable-toolbar.h"
// #include "layermanager.h"  // Not implemented yet
// #include "layerpanel.h"    // Not implemented yet
// #include "advancedtools.h" // Not implemented yet
#include "svghandler.h"
#include <algorithm>
#include "drawing-shape.h"
#include "colorpalette.h"
#include "drawing-group.h"
#include <QMenuBar>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QMenu>
#include <QTimer>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QDockWidget>
#include <QUndoView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QInputDialog>
#include <QColorDialog>
#include <QScrollBar>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_scene(nullptr), m_canvas(nullptr), m_propertyPanel(nullptr), m_undoView(nullptr), m_currentTool(nullptr), m_outlinePreviewTool(nullptr), m_rectangleTool(nullptr), m_ellipseTool(nullptr), m_bezierTool(nullptr),
      m_colorPalette(nullptr), m_scrollableToolBar(nullptr),
      m_horizontalRuler(nullptr), m_verticalRuler(nullptr), m_cornerWidget(nullptr), m_isModified(false)
{
    createActions();
    setupUI();
    connectActions();

    // Create initial scene
    newFile();

    // qDebug() << "MainWindow initialized, default tool already set";
    
    // Setup color palette
    m_colorPalette = new ColorPalette(this);
    m_colorPalette->setScene(m_scene);
    
    // 连接调色板的颜色应用信号
    connect(m_colorPalette, &ColorPalette::applyColorToSelection,
            this, &MainWindow::onApplyColorToSelection);
    
    // Create a dock widget to contain the color palette
    QDockWidget *colorPaletteDock = new QDockWidget("", this);  // 空标题
    colorPaletteDock->setWidget(m_colorPalette);
    colorPaletteDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    colorPaletteDock->setFeatures(QDockWidget::NoDockWidgetFeatures);  // 禁用所有dock功能，使其固定
    colorPaletteDock->setTitleBarWidget(new QWidget());  // 完全移除标题栏
    addDockWidget(Qt::BottomDockWidgetArea, colorPaletteDock);

    // 初始化标尺和网格
    if (m_horizontalRuler && m_verticalRuler && m_canvas && m_canvas->view())
    {
        QPoint origin = m_canvas->view()->mapFromScene(QPoint(0, 0));
        m_horizontalRuler->setOrigin(origin.x());
        m_verticalRuler->setOrigin(origin.y());
        m_horizontalRuler->setScale(1.0);
        m_verticalRuler->setScale(1.0);
    }

    // 触发网格更新
    if (m_scene)
    {
        m_scene->update();
    }

    // 不设置默认工具，让用户手动选择

    resize(1200, 800);
    setWindowTitle("VectorQt - 专业矢量绘图工具");
}

MainWindow::~MainWindow()
{
    // 在析构MainWindow之前，先清理所有工具
    // 确保工具被正确停用并清理资源
    
    // 停用当前工具
    if (m_currentTool) {
        m_currentTool->deactivate();
        m_currentTool = nullptr;
    }
    
    // 显式删除所有工具对象，确保它们在场景销毁之前被清理
    if (m_nodeEditTool) {
        delete m_nodeEditTool;
        m_nodeEditTool = nullptr;
    }
    
    
    
    if (m_bezierTool) {
        delete m_bezierTool;
        m_bezierTool = nullptr;
    }
    
    if (m_ellipseTool) {
        delete m_ellipseTool;
        m_ellipseTool = nullptr;
    }
    
    if (m_rectangleTool) {
        delete m_rectangleTool;
        m_rectangleTool = nullptr;
    }
    
    
    
    if (m_outlinePreviewTool) {
        delete m_outlinePreviewTool;
        m_outlinePreviewTool = nullptr;
    }
    
    
    
    // 清理场景
    if (m_scene) {
        delete m_scene;
        m_scene = nullptr;
    }
}

void MainWindow::setupUI()
{
    // Create scene
    m_scene = new DrawingScene(this);
    m_scene->setSceneRect(0, 0, 1000, 800);
    m_scene->setGridVisible(true); // 确保网格初始可见
    m_scene->setGridAlignmentEnabled(true); // 默认启用网格对齐
    m_scene->setSnapEnabled(true); // 启用智能吸附
    m_scene->setObjectSnapEnabled(true); // 启用对象吸附
    m_scene->setSnapTolerance(3); // 设置吸附容差（降低灵敏度）
    m_scene->setObjectSnapTolerance(3); // 设置对象吸附容差（降低灵敏度）
    
    // Create rulers
    m_horizontalRuler = new Ruler(Ruler::Horizontal, this);
    m_verticalRuler = new Ruler(Ruler::Vertical, this);

    // Create drawing canvas with grid functionality
    m_canvas = new DrawingCanvas(this);
    m_canvas->setScene(m_scene);

    // 确保视图正确初始化并显示网格
    if (m_canvas->view())
    {
        // 设置视图左上角对齐场景原点
        m_canvas->view()->centerOn(0, 0);
        // 确保滚动条范围正确设置
        m_canvas->view()->ensureVisible(m_scene->sceneRect());
        // 强制更新视图以确保网格显示
        m_canvas->view()->update();
        m_scene->update();
    }

    // 初始化标尺原点和缩放
    if (m_canvas->view())
    {
        // Set view for rulers to enable proper coordinate conversion
        m_horizontalRuler->setView(m_canvas->view());
        m_verticalRuler->setView(m_canvas->view());

        QPoint origin = m_canvas->view()->mapFromScene(QPoint(0, 0));
        m_horizontalRuler->setOrigin(origin.x());
        m_verticalRuler->setOrigin(origin.y());
        m_horizontalRuler->setScale(m_canvas->view()->zoomLevel());
        m_verticalRuler->setScale(m_canvas->view()->zoomLevel());
        // qDebug() << "Initial ruler setup - origin:" << origin << "zoom:" << m_canvas->view()->zoomLevel();
    }

    // Connect unit change signals to keep both rulers synchronized
    connect(m_horizontalRuler, &Ruler::unitChangedForAll,
            this, [this](Ruler::Unit unit)
            {
                if (m_verticalRuler) {
                    m_verticalRuler->blockSignals(true); // 防止信号循环
                    m_verticalRuler->setUnit(unit);
                    m_verticalRuler->blockSignals(false);
                } });

    connect(m_verticalRuler, &Ruler::unitChangedForAll,
            this, [this](Ruler::Unit unit)
            {
                if (m_horizontalRuler) {
                    m_horizontalRuler->blockSignals(true); // 防止信号循环
                    m_horizontalRuler->setUnit(unit);
                    m_horizontalRuler->blockSignals(false);
                } });

    // 🌟 连接参考线创建信号
    connect(m_horizontalRuler, &Ruler::guideRequested,
            this, &MainWindow::onGuideRequested);
    connect(m_verticalRuler, &Ruler::guideRequested,
            this, &MainWindow::onGuideRequested);

    // Create corner widget - match ruler size to provide proper alignment
    m_cornerWidget = new QWidget(this);
    m_cornerWidget->setFixedSize(Ruler::rulerSize(), Ruler::rulerSize());
    m_cornerWidget->setStyleSheet("background-color: transparent;");

    // Create central widget with rulers
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // Create horizontal ruler container
    QWidget *topWidget = new QWidget(centralWidget);
    QHBoxLayout *topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    topLayout->addWidget(m_cornerWidget);
    topLayout->addWidget(m_horizontalRuler);

    // Create main content widget
    QWidget *mainWidget = new QWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_verticalRuler);
    mainLayout->addWidget(m_canvas);

    // Assemble the layout
    centralLayout->addWidget(topWidget);
    centralLayout->addWidget(mainWidget);

    setCentralWidget(centralWidget);

    // Setup UI components
    setupDocks();
    setupToolbars();
    setupMenus();
    setupStatusBar();

    // Create tools
    m_outlinePreviewTool = new OutlinePreviewTransformTool(this);
    // 连接状态栏更新信号
    connect(m_outlinePreviewTool, SIGNAL(statusMessageChanged(const QString&)),
            this, SLOT(updateStatusBar(const QString&)));
    m_rectangleTool = new LegacyRectangleTool(this);
    m_ellipseTool = new LegacyEllipseTool(this);
    m_bezierTool = new DrawingBezierTool(this);
    m_nodeEditTool = new DrawingNodeEditTool(this);
    m_polylineTool = new DrawingToolPolyline(this);
    m_polygonTool = new DrawingToolPolygon(this);
    m_brushTool = new DrawingToolBrush(this);
    m_fillTool = new DrawingToolFill(this);
    m_gradientFillTool = new DrawingToolGradientFill(this);
    m_penTool = new DrawingToolPen(this);
    m_eraserTool = new DrawingToolEraser(this);
    m_lineTool = new DrawingToolLine(this);
    m_pathEditTool = new DrawingToolPathEdit(this);
    // m_textTool = new TextTool(this);  // Not implemented yet
    
    // Connect color palette signals to fill tool (moved after all tools are created)
    // This connection will be made later after all initialization is complete

    // Create layer manager - Not implemented yet
    // m_layerManager = new LayerManager(this);
    // m_layerManager->setScene(m_scene);

    // Create initial layer - Not implemented yet
    // Layer *defaultLayer = m_layerManager->createLayer("图层 1");
    // m_layerManager->addLayer(defaultLayer);
    // m_layerManager->setActiveLayer(defaultLayer);

    // Connect layer manager to scene - Not implemented yet
    // m_scene->setLayerManager(m_layerManager);

    // Connect signals
    connect(m_scene, &DrawingScene::selectionChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_scene, &DrawingScene::sceneModified,
            this, &MainWindow::onSceneChanged);
    // 连接DrawingCanvas的缩放信号
    connect(m_canvas, &DrawingCanvas::zoomChanged,
            this, &MainWindow::updateZoomLabel);
            
    // Connect color palette signals to fill tool (after all objects are created)
    if (m_colorPalette && m_fillTool) {
        connect(m_colorPalette, SIGNAL(fillColorChanged(QColor)), 
                m_fillTool, SLOT(onFillColorChanged(QColor)));
    }
    
    // Connect color palette signals to gradient fill tool (after all objects are created)
    if (m_colorPalette && m_gradientFillTool) {
        connect(m_colorPalette, SIGNAL(fillColorChanged(QColor)), 
                m_gradientFillTool, SLOT(onFillColorChanged(QColor)));
    }
    
    // Connect color palette signals to pen tool (after all objects are created)
    if (m_colorPalette && m_penTool) {
        connect(m_colorPalette, SIGNAL(strokeColorChanged(QColor)), 
                m_penTool, SLOT(onStrokeColorChanged(QColor)));
        connect(m_colorPalette, SIGNAL(fillColorChanged(QColor)), 
                m_penTool, SLOT(onFillColorChanged(QColor)));
    }
    
    // Connect undo stack signals to update menu states
    if (m_scene && m_scene->undoStack()) {
        connect(m_scene->undoStack(), &QUndoStack::canUndoChanged,
                this, [this](bool canUndo) { m_undoAction->setEnabled(canUndo); });
        connect(m_scene->undoStack(), &QUndoStack::canRedoChanged,
                this, [this](bool canRedo) { m_redoAction->setEnabled(canRedo); });
    }

    DrawingView *drawingView = qobject_cast<DrawingView *>(m_canvas->view());
    if (drawingView)
    {
        connect(drawingView, &DrawingView::mousePositionChanged,
                this, [this](const QPointF &pos)
                { 
                    m_positionLabel->setText(QString("X: %1, Y: %2").arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1));
                    // 更新标尺的鼠标位置
                    if (m_canvas && m_canvas->view() && m_horizontalRuler && m_verticalRuler) {
                        // 获取鼠标在视图中的坐标
                        QPoint viewPos = m_canvas->view()->mapFromScene(pos);
                        // 直接使用视图坐标，因为标尺和视图是相邻的
                        // 水平标尺：X坐标与视图对齐，考虑标尺的偏移
                        // 垂直标尺：Y坐标与视图对齐，考虑标尺的偏移
                        m_horizontalRuler->setMousePos(QPointF(viewPos.x(), 0));
                        m_verticalRuler->setMousePos(QPointF(0, viewPos.y()));
                        
                        // 触发重绘
                        m_horizontalRuler->update();
                        m_verticalRuler->update();
                    } });
    }

    // 连接视口变化信号来更新标尺坐标
    DrawingView *drawingView2 = qobject_cast<DrawingView *>(m_canvas->view());
    if (drawingView2)
    {
        connect(drawingView2, &DrawingView::viewportChanged,
                this, [this, drawingView2]()
                {
                    if (m_horizontalRuler && m_verticalRuler && m_canvas && m_canvas->view()) {
                        // 更新标尺原点以反映当前视图位置
                        QPoint origin = m_canvas->view()->mapFromScene(QPoint(0, 0));
                        m_horizontalRuler->setOrigin(origin.x());
                        m_verticalRuler->setOrigin(origin.y());
                        
                        // 同时更新缩放比例
                        double zoom = drawingView2->zoomLevel();
                        m_horizontalRuler->setScale(zoom);
                        m_verticalRuler->setScale(zoom);
                        
                        // 触发重绘
                        m_horizontalRuler->update();
                        m_verticalRuler->update();
                    } });

        // 初始化标尺
        if (m_horizontalRuler && m_verticalRuler)
        {
            QPoint origin = m_canvas->view()->mapFromScene(QPoint(0, 0));
            m_horizontalRuler->setOrigin(origin.x());
            m_verticalRuler->setOrigin(origin.y());
            m_horizontalRuler->setScale(1.0);
            m_verticalRuler->setScale(1.0);
            
            // 触发重绘
            m_horizontalRuler->update();
            m_verticalRuler->update();
        }
    }

    // 设置默认工具为选择工具
    setCurrentTool(m_outlinePreviewTool);
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&文件");
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    // Edit menu
    QMenu *editMenu = menuBar()->addMenu("&编辑");
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_deleteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addAction(m_duplicateAction);
    editMenu->addSeparator();
    editMenu->addAction(m_selectAllAction);
    editMenu->addAction(m_deselectAllAction);
    editMenu->addSeparator();
    editMenu->addAction(m_groupAction);
    editMenu->addAction(m_ungroupAction);
    editMenu->addSeparator();
    editMenu->addAction(m_alignLeftAction);
    editMenu->addAction(m_alignCenterAction);
    editMenu->addAction(m_alignRightAction);
    editMenu->addSeparator();
    editMenu->addAction(m_alignTopAction);
    editMenu->addAction(m_alignMiddleAction);
    editMenu->addAction(m_alignBottomAction);
    editMenu->addSeparator();
    editMenu->addAction(m_distributeHorizontalAction);
    editMenu->addAction(m_distributeVerticalAction);

    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_resetZoomAction);
    viewMenu->addAction(m_fitToWindowAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_toggleGridAction);
    viewMenu->addAction(m_toggleGridAlignmentAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_clearAllGuidesAction);
    viewMenu->addAction(m_gridSizeAction);
    viewMenu->addAction(m_gridColorAction);

    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu("&工具");
    toolsMenu->addAction(m_outlinePreviewToolAction);
    toolsMenu->addAction(m_rectangleToolAction);
    toolsMenu->addAction(m_ellipseToolAction);
    toolsMenu->addAction(m_bezierToolAction);
    toolsMenu->addAction(m_polylineToolAction);
    toolsMenu->addAction(m_polygonToolAction);
    toolsMenu->addAction(m_brushToolAction);
    toolsMenu->addAction(m_fillToolAction);
    toolsMenu->addAction(m_gradientFillToolAction);
    toolsMenu->addAction(m_penToolAction);
    toolsMenu->addAction(m_eraserToolAction);
    toolsMenu->addAction(m_lineToolAction);
    // toolsMenu->addAction(m_textToolAction);  // Not implemented yet

    // Path menu
    QMenu *pathMenu = menuBar()->addMenu("&路径");
    pathMenu->addAction(m_pathEditToolAction);
    
    // 添加路径布尔运算操作
    pathMenu->addSeparator();
    m_pathUnionAction = new QAction("联合(&U)", this);
    m_pathUnionAction->setStatusTip("将选中的图形联合成一个路径");
    pathMenu->addAction(m_pathUnionAction);
    
    m_pathSubtractAction = new QAction("减去(&S)", this);
    m_pathSubtractAction->setStatusTip("从第一个选中图形中减去其他图形");
    pathMenu->addAction(m_pathSubtractAction);
    
    m_pathIntersectAction = new QAction("相交(&I)", this);
    m_pathIntersectAction->setStatusTip("获取选中图形的相交部分");
    pathMenu->addAction(m_pathIntersectAction);
    
    m_pathXorAction = new QAction("异或(&X)", this);
    m_pathXorAction->setStatusTip("获取选中图形的异或部分");
    pathMenu->addAction(m_pathXorAction);

    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&帮助");
    helpMenu->addAction(m_aboutAction);
}



void MainWindow::setupToolbars()
{
    // 设置工具栏样式 - 支持暗色主题
    setStyleSheet(R"(
        QToolBar {
            spacing: 3px;
            padding: 4px;
            background: palette(window);
            border: 1px solid palette(mid);
            border-radius: 4px;
            margin: 2px;
        }
        QToolBar QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px;
            margin: 1px;
        }
        QToolBar QToolButton:hover {
            background: palette(highlight);
            color: palette(highlighted-text);
        }
        QToolBar QToolButton:pressed {
            background: palette(dark);
            color: palette(bright-text);
        }
        QToolBar QToolButton:checked {
            background: palette(highlight);
            color: palette(highlighted-text);
        }
        QToolBar::separator {
            background: palette(mid);
            width: 1px;
            margin: 4px 2px;
        }
    )");
    
    // Main toolbar - 只包含文件操作
    QToolBar *mainToolBar = addToolBar("文件");
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mainToolBar->setIconSize(QSize(24, 24));
    mainToolBar->addAction(m_newAction);
    mainToolBar->addAction(m_openAction);
    mainToolBar->addAction(m_saveAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_undoAction);
    mainToolBar->addAction(m_redoAction);
    
    // 添加文件操作图标
    m_newAction->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    m_openAction->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_saveAction->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_undoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    m_redoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));

    // Tools toolbar - moved to left side with scrolling support
    m_scrollableToolBar = new ScrollableToolBar("绘图工具", this);
    addToolBar(Qt::LeftToolBarArea, m_scrollableToolBar);
    m_scrollableToolBar->setOrientation(Qt::Vertical);
    m_scrollableToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly); // 只显示图标
    m_scrollableToolBar->setIconSize(QSize(32, 32)); // 工具图标稍大一些
    m_scrollableToolBar->addAction(m_outlinePreviewToolAction);
    m_scrollableToolBar->addSeparator();
    
    // 基础形状工具
    m_scrollableToolBar->addAction(m_rectangleToolAction);
    m_scrollableToolBar->addAction(m_ellipseToolAction);
    m_scrollableToolBar->addAction(m_lineToolAction);
    m_scrollableToolBar->addSeparator();
    
    // 路径工具
    m_scrollableToolBar->addAction(m_bezierToolAction);
    m_scrollableToolBar->addAction(m_nodeEditToolAction);
    m_scrollableToolBar->addAction(m_pathEditToolAction);
    m_scrollableToolBar->addAction(m_polylineToolAction);
    m_scrollableToolBar->addAction(m_polygonToolAction);
    m_scrollableToolBar->addSeparator();
    
    // 绘制工具
    m_scrollableToolBar->addAction(m_brushToolAction);
    m_scrollableToolBar->addAction(m_penToolAction);
    m_scrollableToolBar->addSeparator();
    
    // 填充工具
    m_scrollableToolBar->addAction(m_fillToolAction);
    m_scrollableToolBar->addAction(m_gradientFillToolAction);
    m_scrollableToolBar->addAction(m_eraserToolAction);
    // m_scrollableToolBar->addAction(m_textToolAction);  // Not implemented yet

    // View toolbar - 包含视图、组合和对齐操作
    QToolBar *viewToolBar = addToolBar("视图");
    viewToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    viewToolBar->setIconSize(QSize(24, 24));
    viewToolBar->addAction(m_zoomInAction);
    viewToolBar->addAction(m_zoomOutAction);
    viewToolBar->addAction(m_resetZoomAction);
    viewToolBar->addAction(m_fitToWindowAction);
    viewToolBar->addSeparator();
    viewToolBar->addAction(m_toggleGridAction);
    viewToolBar->addAction(m_toggleGridAlignmentAction);
    viewToolBar->addSeparator();
    // Group and alignment tools
    viewToolBar->addAction(m_groupAction);
    viewToolBar->addAction(m_ungroupAction);
    viewToolBar->addSeparator();
    // Alignment tools
    viewToolBar->addAction(m_alignLeftAction);
    viewToolBar->addAction(m_alignCenterAction);
    viewToolBar->addAction(m_alignRightAction);
    viewToolBar->addAction(m_alignTopAction);
    viewToolBar->addAction(m_alignMiddleAction);
    viewToolBar->addAction(m_alignBottomAction);
    viewToolBar->addSeparator();
    viewToolBar->addAction(m_distributeHorizontalAction);
    viewToolBar->addAction(m_distributeVerticalAction);
    
    // 添加视图操作图标
    m_zoomInAction->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_zoomOutAction->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_resetZoomAction->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_fitToWindowAction->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_toggleGridAction->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    m_toggleGridAlignmentAction->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
    
    // 添加组合和对齐工具图标
    m_groupAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_ungroupAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogInfoView));
    m_alignLeftAction->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    m_alignCenterAction->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
    m_alignRightAction->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    m_alignTopAction->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_alignMiddleAction->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_alignBottomAction->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    
    // 分布工具图标
    m_distributeHorizontalAction->setIcon(style()->standardIcon(QStyle::SP_ToolBarHorizontalExtensionButton));
    m_distributeVerticalAction->setIcon(style()->standardIcon(QStyle::SP_ToolBarVerticalExtensionButton));
}

void MainWindow::setupDocks()
{
    // Properties dock
    QDockWidget *propertiesDock = new QDockWidget("属性", this);
    m_propertyPanel = new PropertyPanel(propertiesDock);
    m_propertyPanel->setScene(m_scene);
    propertiesDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    // Undo history dock
    QDockWidget *historyDock = new QDockWidget("历史记录", this);
    m_undoView = new QUndoView(m_scene->undoStack(), historyDock);
    historyDock->setWidget(m_undoView);
    addDockWidget(Qt::RightDockWidgetArea, historyDock);

    // Layer panel dock - Not implemented yet
    // QDockWidget *layerDock = new QDockWidget("图层", this);
    // LayerPanel *layerPanel = new LayerPanel(layerDock);
    // layerPanel->setLayerManager(m_layerManager);
    // layerDock->setWidget(layerPanel);
    // addDockWidget(Qt::RightDockWidgetArea, layerDock);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("就绪");
    statusBar()->addWidget(m_statusLabel);

    m_zoomLabel = new QLabel("100%");
    statusBar()->addPermanentWidget(m_zoomLabel);

    m_positionLabel = new QLabel("X: 0, Y: 0");
    statusBar()->addPermanentWidget(m_positionLabel);
}

void MainWindow::createActions()
{
    // File actions
    m_newAction = new QAction("&新建", this);
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setStatusTip("创建新文档");

    m_openAction = new QAction("&打开...", this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip("打开现有文档");

    m_saveAction = new QAction("&保存", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip("保存文档到磁盘");

    m_saveAsAction = new QAction("另存为...", this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip("以新名称保存文档");

    m_exportAction = new QAction("&导出...", this);
    m_exportAction->setStatusTip("导出文档");

    m_exitAction = new QAction("退出(&X)", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip("退出应用程序");

    // Edit actions
    m_undoAction = new QAction("&撤销", this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setStatusTip("撤销上一个操作");

    m_redoAction = new QAction("&重做", this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setStatusTip("重做上一个操作");

    m_deleteAction = new QAction("&删除", this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setStatusTip("删除选中项目");

    m_copyAction = new QAction("&复制", this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_copyAction->setStatusTip("复制选中项目");

    m_pasteAction = new QAction("&粘贴", this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setStatusTip("粘贴项目");

    m_duplicateAction = new QAction("&快速复制", this);
    m_duplicateAction->setShortcut(QKeySequence("Ctrl+D"));
    m_duplicateAction->setStatusTip("快速复制并粘贴选中项目");

    m_selectAllAction = new QAction("全选(&A)", this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_selectAllAction->setStatusTip("选择所有项目");

    m_deselectAllAction = new QAction("取消全选(&D)", this);
    m_deselectAllAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    m_deselectAllAction->setStatusTip("取消选择所有项目");

    // View actions
    m_zoomInAction = new QAction("放大(&I)", this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    m_zoomInAction->setStatusTip("放大");

    m_zoomOutAction = new QAction("缩小(&O)", this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    m_zoomOutAction->setStatusTip("缩小");

    m_resetZoomAction = new QAction("重置缩放(&R)", this);
    m_resetZoomAction->setShortcut(QKeySequence("Ctrl+0"));
    m_resetZoomAction->setStatusTip("重置缩放到100%");

    m_fitToWindowAction = new QAction("适应窗口(&F)", this);
    m_fitToWindowAction->setShortcut(QKeySequence("Ctrl+1"));
    m_fitToWindowAction->setStatusTip("使视图适应窗口");

    // Grid actions
    m_toggleGridAction = new QAction("显示网格(&G)", this);
    m_toggleGridAction->setCheckable(true);
    m_toggleGridAction->setShortcut(QKeySequence("G"));
    m_toggleGridAction->setStatusTip("显示或隐藏网格");
    m_toggleGridAction->setChecked(true); // 默认显示网格

    m_gridSizeAction = new QAction("网格大小...", this);
    m_gridSizeAction->setStatusTip("设置网格大小");

    m_gridColorAction = new QAction("网格颜色...", this);
    m_gridColorAction->setStatusTip("设置网格颜色");
    
    m_toggleGridAlignmentAction = new QAction("网格对齐(&A)", this);
    m_toggleGridAlignmentAction->setCheckable(true);
    m_toggleGridAlignmentAction->setShortcut(QKeySequence("Shift+G"));
    m_toggleGridAlignmentAction->setStatusTip("启用或禁用网格对齐");
    m_toggleGridAlignmentAction->setChecked(true); // 默认启用网格对齐
    
    // 清除所有参考线
    m_clearAllGuidesAction = new QAction("清除所有参考线(&G)", this);
    m_clearAllGuidesAction->setShortcut(QKeySequence("Ctrl+Shift+G"));
    m_clearAllGuidesAction->setStatusTip("清除所有参考线");
    
    m_groupAction = new QAction("组合(&G)", this);
    m_groupAction->setShortcut(QKeySequence("Ctrl+G"));
    m_groupAction->setStatusTip("将选中的项目组合成一个组");
    
    m_ungroupAction = new QAction("取消组合(&U)", this);
    m_ungroupAction->setShortcut(QKeySequence("Ctrl+Shift+G"));
    m_ungroupAction->setStatusTip("取消选中的组合");
    
    // Align actions
    m_alignLeftAction = new QAction("左对齐(&L)", this);
    m_alignLeftAction->setStatusTip("将选中的项目左对齐");
    
    m_alignCenterAction = new QAction("水平居中(&C)", this);
    m_alignCenterAction->setStatusTip("将选中的项目水平居中");
    
    m_alignRightAction = new QAction("右对齐(&R)", this);
    m_alignRightAction->setStatusTip("将选中的项目右对齐");
    
    m_alignTopAction = new QAction("顶部对齐(&T)", this);
    m_alignTopAction->setStatusTip("将选中的项目顶部对齐");
    
    m_alignMiddleAction = new QAction("垂直居中(&M)", this);
    m_alignMiddleAction->setStatusTip("将选中的项目垂直居中");
    
    m_alignBottomAction = new QAction("底部对齐(&B)", this);
    m_alignBottomAction->setStatusTip("将选中的项目底部对齐");
    
    // 分布动作
    m_distributeHorizontalAction = new QAction("水平分布(&H)", this);
    m_distributeHorizontalAction->setStatusTip("将选中的项目水平均匀分布");
    
    m_distributeVerticalAction = new QAction("垂直分布(&V)", this);
    m_distributeVerticalAction->setStatusTip("将选中的项目垂直均匀分布");

    // Tool actions
    m_toolGroup = new QActionGroup(this);

    m_outlinePreviewToolAction = new QAction("&选择工具", this);
    m_outlinePreviewToolAction->setCheckable(true);
    m_outlinePreviewToolAction->setShortcut(QKeySequence("V"));
    m_outlinePreviewToolAction->setStatusTip("选择和变换项目");
    m_outlinePreviewToolAction->setIcon(QIcon(":/icons/icons/select-tool-new.svg"));
    m_toolGroup->addAction(m_outlinePreviewToolAction);

    m_rectangleToolAction = new QAction("&矩形工具", this);
    m_rectangleToolAction->setCheckable(true);
    m_rectangleToolAction->setShortcut(QKeySequence("R"));
    m_rectangleToolAction->setStatusTip("绘制矩形");
    m_rectangleToolAction->setIcon(QIcon(":/icons/icons/rectangle-tool-new.svg"));
    m_toolGroup->addAction(m_rectangleToolAction);

    m_ellipseToolAction = new QAction("&椭圆工具", this);
    m_ellipseToolAction->setCheckable(true);
    m_ellipseToolAction->setShortcut(QKeySequence("E"));
    m_ellipseToolAction->setStatusTip("绘制椭圆");
    m_ellipseToolAction->setIcon(QIcon(":/icons/icons/ellipse-tool-new.svg"));
    m_toolGroup->addAction(m_ellipseToolAction);

    m_bezierToolAction = new QAction("&贝塞尔曲线工具", this);
    m_bezierToolAction->setCheckable(true);
    m_bezierToolAction->setShortcut(QKeySequence("B"));
    m_bezierToolAction->setStatusTip("绘制贝塞尔曲线");
    m_bezierToolAction->setIcon(QIcon(":/icons/icons/bezier-tool-new.svg"));
    m_toolGroup->addAction(m_bezierToolAction);
    
    
    
    m_nodeEditToolAction = new QAction("节&点编辑工具", this);
    m_nodeEditToolAction->setCheckable(true);
    m_nodeEditToolAction->setShortcut(QKeySequence("N"));
    m_nodeEditToolAction->setStatusTip("编辑图形节点和控制点");
    m_nodeEditToolAction->setIcon(QIcon(":/icons/icons/node-edit-tool.svg")); // 使用专用节点编辑图标
    m_toolGroup->addAction(m_nodeEditToolAction);

    m_polylineToolAction = new QAction("&折线工具", this);
    m_polylineToolAction->setCheckable(true);
    m_polylineToolAction->setShortcut(QKeySequence("Y"));
    m_polylineToolAction->setStatusTip("绘制折线");
    m_polylineToolAction->setIcon(QIcon(":/icons/icons/polyline-tool-new.svg"));
    m_toolGroup->addAction(m_polylineToolAction);

    m_polygonToolAction = new QAction("&多边形工具", this);
    m_polygonToolAction->setCheckable(true);
    m_polygonToolAction->setShortcut(QKeySequence("P"));
    m_polygonToolAction->setStatusTip("绘制多边形");
    m_polygonToolAction->setIcon(QIcon(":/icons/icons/polygon-tool-new.svg"));
    m_toolGroup->addAction(m_polygonToolAction);

    m_brushToolAction = new QAction("&画笔工具", this);
    m_brushToolAction->setCheckable(true);
    m_brushToolAction->setShortcut(QKeySequence("B"));
    m_brushToolAction->setStatusTip("自由绘制");
    m_brushToolAction->setIcon(QIcon(":/icons/icons/brush-tool-new.svg"));
    m_toolGroup->addAction(m_brushToolAction);

    m_fillToolAction = new QAction("&填充工具", this);
    m_fillToolAction->setCheckable(true);
    m_fillToolAction->setShortcut(QKeySequence("F"));
    m_fillToolAction->setStatusTip("填充区域");
    m_fillToolAction->setIcon(QIcon(":/icons/icons/fill-tool-new.svg"));
    m_toolGroup->addAction(m_fillToolAction);

    m_gradientFillToolAction = new QAction("&渐进填充工具", this);
    m_gradientFillToolAction->setCheckable(true);
    m_gradientFillToolAction->setShortcut(QKeySequence("G"));
    m_gradientFillToolAction->setStatusTip("渐进填充区域");
    m_gradientFillToolAction->setIcon(QIcon(":/icons/icons/gradient-fill-tool.svg"));
    m_toolGroup->addAction(m_gradientFillToolAction);

    m_penToolAction = new QAction("&钢笔工具", this);
    m_penToolAction->setCheckable(true);
    m_penToolAction->setShortcut(QKeySequence("P"));
    m_penToolAction->setStatusTip("绘制贝塞尔曲线路径");
    m_penToolAction->setIcon(QIcon(":/icons/icons/pen-tool.svg"));
    m_toolGroup->addAction(m_penToolAction);

    m_eraserToolAction = new QAction("&橡皮擦工具", this);
    m_eraserToolAction->setCheckable(true);
    m_eraserToolAction->setShortcut(QKeySequence("E"));
    m_eraserToolAction->setStatusTip("擦除图形或图形的部分区域");
    m_eraserToolAction->setIcon(QIcon(":/icons/icons/eraser-tool.svg"));
    m_toolGroup->addAction(m_eraserToolAction);

    m_lineToolAction = new QAction("&线条工具", this);
    m_lineToolAction->setCheckable(true);
    m_lineToolAction->setShortcut(QKeySequence("L"));
    m_lineToolAction->setStatusTip("绘制线条");
    m_lineToolAction->setIcon(QIcon(":/icons/icons/line-tool-new.svg"));
    m_toolGroup->addAction(m_lineToolAction);

    m_pathEditToolAction = new QAction("&路径编辑", this);
    m_pathEditToolAction->setCheckable(true);
    m_pathEditToolAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
    m_pathEditToolAction->setStatusTip("编辑选中路径的节点");
    m_pathEditToolAction->setIcon(QIcon(":/icons/icons/path-edit-tool-new.svg"));
    m_toolGroup->addAction(m_pathEditToolAction);

    // m_textToolAction = new QAction("&文本工具", this);  // Not implemented yet
    // m_textToolAction->setCheckable(true);
    // m_textToolAction->setShortcut(QKeySequence("T"));
    // m_textToolAction->setStatusTip("添加文本");
    // m_toolGroup->addAction(m_textToolAction);

    // Path boolean operations
    m_pathUnionAction = new QAction("联合(&U)", this);
    m_pathUnionAction->setStatusTip("将选中的图形联合成一个路径");
    m_pathUnionAction->setShortcut(QKeySequence("Ctrl+U"));
    
    m_pathSubtractAction = new QAction("减去(&S)", this);
    m_pathSubtractAction->setStatusTip("从第一个选中图形中减去其他图形");
    m_pathSubtractAction->setShortcut(QKeySequence("Ctrl+S"));
    
    m_pathIntersectAction = new QAction("相交(&I)", this);
    m_pathIntersectAction->setStatusTip("获取选中图形的相交部分");
    m_pathIntersectAction->setShortcut(QKeySequence("Ctrl+I"));
    
    m_pathXorAction = new QAction("异或(&X)", this);
    m_pathXorAction->setStatusTip("获取选中图形的异或部分");
    m_pathXorAction->setShortcut(QKeySequence("Ctrl+X"));

    // Help actions
    m_aboutAction = new QAction("&关于", this);
    m_aboutAction->setStatusTip("显示应用程序的关于对话框");
}

void MainWindow::connectActions()
{
    // File connections
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newFile);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportFile);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    // Edit connections
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::deleteSelected);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copySelected);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::paste);
    connect(m_duplicateAction, &QAction::triggered, this, &MainWindow::duplicate);
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::selectAll);
    connect(m_deselectAllAction, &QAction::triggered, this, &MainWindow::deselectAll);

    // View connections
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);
    connect(m_resetZoomAction, &QAction::triggered, this, &MainWindow::resetZoom);
    connect(m_fitToWindowAction, &QAction::triggered, this, &MainWindow::fitToWindow);

    // Grid connections
    connect(m_toggleGridAction, &QAction::triggered, this, &MainWindow::toggleGrid);
    connect(m_gridSizeAction, &QAction::triggered, this, &MainWindow::showGridSettings);
    connect(m_gridColorAction, &QAction::triggered, this, &MainWindow::showGridSettings);
    connect(m_toggleGridAlignmentAction, &QAction::triggered, this, &MainWindow::toggleGridAlignment);
    connect(m_clearAllGuidesAction, &QAction::triggered, this, &MainWindow::clearAllGuides);
    
    // Group connections
    connect(m_groupAction, &QAction::triggered, this, &MainWindow::groupSelected);
    connect(m_ungroupAction, &QAction::triggered, this, &MainWindow::ungroupSelected);
    
    // Align connections
    connect(m_alignLeftAction, &QAction::triggered, this, &MainWindow::alignLeft);
    connect(m_alignCenterAction, &QAction::triggered, this, &MainWindow::alignCenter);
    connect(m_alignRightAction, &QAction::triggered, this, &MainWindow::alignRight);
    connect(m_alignTopAction, &QAction::triggered, this, &MainWindow::alignTop);
    connect(m_alignMiddleAction, &QAction::triggered, this, &MainWindow::alignMiddle);
    connect(m_alignBottomAction, &QAction::triggered, this, &MainWindow::alignBottom);
    connect(m_distributeHorizontalAction, &QAction::triggered, this, &MainWindow::distributeHorizontal);
    connect(m_distributeVerticalAction, &QAction::triggered, this, &MainWindow::distributeVertical);

    // Tool connections
    
    connect(m_outlinePreviewToolAction, &QAction::triggered, this, &MainWindow::selectTool);
    connect(m_rectangleToolAction, &QAction::triggered, this, &MainWindow::rectangleTool);
    connect(m_ellipseToolAction, &QAction::triggered, this, &MainWindow::ellipseTool);
    connect(m_bezierToolAction, &QAction::triggered, this, &MainWindow::bezierTool);
    
    connect(m_nodeEditToolAction, &QAction::triggered, this, &MainWindow::nodeEditTool);
    connect(m_polylineToolAction, &QAction::triggered, this, &MainWindow::polylineTool);
    connect(m_polygonToolAction, &QAction::triggered, this, &MainWindow::polygonTool);
    connect(m_brushToolAction, &QAction::triggered, this, &MainWindow::brushTool);
    connect(m_fillToolAction, &QAction::triggered, this, &MainWindow::fillTool);
    connect(m_gradientFillToolAction, &QAction::triggered, this, &MainWindow::gradientFillTool);
    connect(m_penToolAction, &QAction::triggered, this, &MainWindow::penTool);
    connect(m_eraserToolAction, &QAction::triggered, this, &MainWindow::eraserTool);
    connect(m_lineToolAction, &QAction::triggered, this, &MainWindow::lineTool);
    connect(m_pathEditToolAction, &QAction::triggered, this, &MainWindow::pathEditTool);
    // connect(m_textToolAction, &QAction::triggered, this, &MainWindow::textTool);  // Not implemented yet
    
    // Path boolean operation connections
    connect(m_pathUnionAction, &QAction::triggered, this, &MainWindow::pathUnion);
    connect(m_pathSubtractAction, &QAction::triggered, this, &MainWindow::pathSubtract);
    connect(m_pathIntersectAction, &QAction::triggered, this, &MainWindow::pathIntersect);
    connect(m_pathXorAction, &QAction::triggered, this, &MainWindow::pathXor);

    // Help connections
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::about);
}

void MainWindow::setCurrentTool(ToolBase *tool)
{
    qDebug() << "setCurrentTool called with tool:" << tool;

    if (m_currentTool)
    {
        qDebug() << "Deactivating current tool:" << m_currentTool;
        m_currentTool->deactivate();
    }

    // 如果有正在进行的变换操作，先结束它
    if (m_scene) {
        qDebug() << "Calling endTransform() before tool switch";
        m_scene->endTransform();
    }

        m_currentTool = tool;

    if (m_currentTool)
    {
        qDebug() << "Activating new tool:" << m_currentTool;
        DrawingView *drawingView = qobject_cast<DrawingView *>(m_canvas->view());
        m_currentTool->activate(m_scene, drawingView);
        if (drawingView)
        {
            drawingView->setCurrentTool(m_currentTool);
        }
    }
    
    // 如果切换到非选择工具且不是节点编辑工具或轮廓预览工具，在工具激活后清除场景中的选择
    if (m_scene && tool != m_nodeEditTool && tool != m_outlinePreviewTool) {
        qDebug() << "Clearing selection after switching to non-select tool";
        qDebug() << "Selected items count before clearing:" << m_scene->selectedItems().count();
        
        // 在工具激活后清除场景中的选择
        if (m_scene) {
            qDebug() << "Clearing selection after switching to non-select tool";
            qDebug() << "Selected items count before clearing:" << m_scene->selectedItems().count();
            
            // 检查并取消鼠标抓取
            if (m_scene->mouseGrabberItem()) {
                qDebug() << "Found mouse grabber item, ungrabbing";
                m_scene->mouseGrabberItem()->ungrabMouse();
            }
            
            m_scene->clearSelection();
            
            // 强制清除所有项目的选中状态
            QList<QGraphicsItem*> allItems = m_scene->items(Qt::AscendingOrder);
            for (QGraphicsItem *item : allItems) {
                if (item && item->isSelected()) {
                    item->setSelected(false);
                }
            }
            
            // 隐藏所有路径的控制点连线
            QList<QGraphicsItem*> items = m_scene->items(Qt::AscendingOrder);
            for (QGraphicsItem *item : items) {
                // 检查item是否仍然有效且在场景中
                if (!item || item->scene() != m_scene) {
                    continue;
                }
                
                // 首先验证类型
                if (item->type() != DrawingShape::Path) {
                    continue;
                }
                
                // 使用更安全的类型转换，并验证对象状态
                DrawingPath *path = nullptr;
                try {
                    path = qgraphicsitem_cast<DrawingPath*>(item);
                    // 额外验证：检查对象是否已被删除或无效
                    if (path && path->type() == DrawingShape::Path) {
                        path->setShowControlPolygon(false);
                    }
                } catch (...) {
                    // 忽略异常，继续处理其他项目
                    continue;
                }
            }
            
            qDebug() << "Selected items count after clearing:" << m_scene->selectedItems().count();
        }
    }


    // Update tool actions
    if (tool == m_outlinePreviewTool)
    {
        m_outlinePreviewToolAction->setChecked(true);
    }
    else if (tool == m_rectangleTool)
    {
        m_rectangleToolAction->setChecked(true);
    }
    else if (tool == m_ellipseTool)
    {
        m_ellipseToolAction->setChecked(true);
    }
    else if (tool == m_bezierTool)
    {
        m_bezierToolAction->setChecked(true);
    }
    
    else if (tool == m_nodeEditTool)
    {
        m_nodeEditToolAction->setChecked(true);
    }

    m_statusLabel->setText(QString("工具已更改: %1")
    .arg(tool == m_outlinePreviewTool ? "选择" : tool == m_rectangleTool ? "矩形"
                                    : tool == m_ellipseTool     ? "椭圆"
                                    : tool == m_bezierTool      ? "贝塞尔"
                                    
                                    : tool == m_nodeEditTool    ? "节点编辑"
                                                                :
                                                            // tool == m_lineTool ? "线条" :       // Not implemented yet
                                        // tool == m_polygonTool ? "多边形" : // Not implemented yet
                                        tool ? "未知"
                                            : "未知")); // Simplified since TextTool is not implemented

    qDebug() << "Tool switch completed";
}



void MainWindow::newFile()
{
    if (m_isModified)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "VectorQt",
                                                                  "文档已修改，是否保存？",
                                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Save)
        {
            saveFile();
        }
        else if (reply == QMessageBox::Cancel)
        {
            return;
        }
    }

    m_scene->clearScene();
    m_currentFile.clear();
    m_isModified = false;
    updateUI();
    m_statusLabel->setText("新文档已创建");
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "打开文档", QDir::homePath(), "SVG Files (*.svg);;VectorQt Files (*.vfp)");

    if (!fileName.isEmpty())
    {
        QFileInfo fileInfo(fileName);
        if (fileInfo.suffix().toLower() == "svg")
        {
            // SVG导入
            if (SvgHandler::importFromSvg(m_scene, fileName)) {
                m_currentFile = fileName;
                m_isModified = false;
                updateUI(); // 更新窗口标题
                m_statusLabel->setText(QString("SVG文件已导入: %1").arg(fileInfo.fileName()));
                
                // 加载完成后重置视图到100%而不是自动适应
                if (m_canvas) {
                    m_canvas->resetZoom();
                    // 可选：将视图居中到内容
                    m_canvas->centerOnContent();
                }
            } else {
                QMessageBox::warning(this, "导入错误", "无法导入SVG文件");
            }
        }
        else
        {
            // QDP文件加载
            m_statusLabel->setText("QDP文件加载功能尚未实现");
        }
    }
}

void MainWindow::saveFile()
{
    if (m_currentFile.isEmpty())
    {
        saveFileAs();
    }
    else
    {
        // 保存为 SVG 文件
        if (SvgHandler::exportToSvg(m_scene, m_currentFile)) {
            m_isModified = false;
            m_statusLabel->setText(QString("文档已保存: %1").arg(QFileInfo(m_currentFile).fileName()));
        } else {
            QMessageBox::warning(this, "保存错误", "无法保存SVG文件");
        }
    }
}

void MainWindow::saveFileAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "保存文档", QDir::homePath(), "SVG Files (*.svg)");

    if (!fileName.isEmpty())
    {
        // 确保文件有 .svg 扩展名
        if (!fileName.endsWith(".svg", Qt::CaseInsensitive)) {
            fileName += ".svg";
        }
        
        m_currentFile = fileName;
        
        // 保存为 SVG 文件
        if (SvgHandler::exportToSvg(m_scene, m_currentFile)) {
            m_isModified = false;
            m_statusLabel->setText(QString("文档已保存: %1").arg(QFileInfo(m_currentFile).fileName()));
        } else {
            QMessageBox::warning(this, "保存错误", "无法保存SVG文件");
        }
    }
}

void MainWindow::exportFile()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "导出文档", QDir::homePath(), "SVG Files (*.svg)");

    if (!fileName.isEmpty())
    {
        if (SvgHandler::exportToSvg(m_scene, fileName)) {
            m_statusLabel->setText(QString("文档已导出到: %1").arg(QFileInfo(fileName).fileName()));
        } else {
            QMessageBox::warning(this, "导出错误", "无法导出SVG文件");
        }
    }
}

void MainWindow::undo()
{
    m_scene->undoStack()->undo();
}

void MainWindow::redo()
{
    m_scene->undoStack()->redo();
}

void MainWindow::selectTool()
{
    setCurrentTool(m_outlinePreviewTool);
}



void MainWindow::rectangleTool()
{
    setCurrentTool(m_rectangleTool);
}

void MainWindow::ellipseTool()
{
    setCurrentTool(m_ellipseTool);
}

void MainWindow::bezierTool()
{
    setCurrentTool(m_bezierTool);
}



void MainWindow::nodeEditTool()
{
    setCurrentTool(m_nodeEditTool);
}



void MainWindow::polylineTool()
{
    setCurrentTool(m_polylineTool);
}

void MainWindow::polygonTool()
{
    setCurrentTool(m_polygonTool);
}

void MainWindow::brushTool()
{
    setCurrentTool(m_brushTool);
}

void MainWindow::fillTool()
{
    setCurrentTool(m_fillTool);
}

void MainWindow::gradientFillTool()
{
    setCurrentTool(m_gradientFillTool);
}

void MainWindow::penTool()
{
    setCurrentTool(m_penTool);
}

void MainWindow::eraserTool()
{
    setCurrentTool(m_eraserTool);
}

void MainWindow::lineTool()
{
    setCurrentTool(m_lineTool);
}

void MainWindow::pathEditTool()
{
    // 直接切换到路径编辑工具，让它处理所有逻辑
    setCurrentTool(m_pathEditTool);
}

// void MainWindow::textTool()  // Not implemented yet
// {
//     setCurrentTool(m_textTool);
// }

// updateZoomLabel implementation is below

void MainWindow::deleteSelected()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty())
        return;

    // 先清除选择，避免在删除过程中出现问题
    m_scene->clearSelection();

    // 使用撤销栈来删除项目，而不是直接删除
    foreach (QGraphicsItem *item, selected)
    {
        if (item)
        {
            // 使用DrawingScene的RemoveItemCommand
            // 这里我们需要访问DrawingScene的撤销栈
            m_scene->removeItem(item);
            // 不要手动删除item，QGraphicsScene会自动管理内存
        }
    }
    m_scene->setModified(true);
}

void MainWindow::copySelected()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty())
        return;

    // 创建MIME数据来存储复制的项目
    QMimeData *mimeData = new QMimeData();
    
    // 使用JSON格式存储数据，更简单可靠
    QString jsonData = "[";
    
    bool first = true;
    for (QGraphicsItem *item : selected) {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            if (!first) {
                jsonData += ",";
            }
            first = false;
            
            jsonData += "{";
            jsonData += QString("\"type\":%1,").arg((int)shape->shapeType());
            jsonData += QString("\"x\":%1,").arg(shape->pos().x());
            jsonData += QString("\"y\":%1,").arg(shape->pos().y());
            
            // 写入样式数据
            QPen pen = shape->strokePen();
            QBrush brush = shape->fillBrush();
            jsonData += QString("\"stroke\":{\"color\":\"%1\",\"width\":%2,\"style\":%3},")
                        .arg(pen.color().name())
                        .arg(pen.width())
                        .arg((int)pen.style());
            jsonData += QString("\"fill\":{\"color\":\"%1\",\"style\":%2},")
                        .arg(brush.color().name())
                        .arg((int)brush.style());
            
            // 写入几何数据
            switch (shape->shapeType()) {
                case DrawingShape::Rectangle: {
                    DrawingRectangle *rect = static_cast<DrawingRectangle*>(shape);
                    jsonData += QString(",\"rect\":{\"x\":%1,\"y\":%2,\"w\":%3,\"h\":%4}")
                                .arg(rect->rectangle().x())
                                .arg(rect->rectangle().y())
                                .arg(rect->rectangle().width())
                                .arg(rect->rectangle().height());
                    break;
                }
                case DrawingShape::Ellipse: {
                    DrawingEllipse *ellipse = static_cast<DrawingEllipse*>(shape);
                    jsonData += QString(",\"ellipse\":{\"x\":%1,\"y\":%2,\"w\":%3,\"h\":%4}")
                                .arg(ellipse->ellipse().x())
                                .arg(ellipse->ellipse().y())
                                .arg(ellipse->ellipse().width())
                                .arg(ellipse->ellipse().height());
                    break;
                }
                case DrawingShape::Line: {
                    DrawingLine *line = static_cast<DrawingLine*>(shape);
                    jsonData += QString(",\"line\":{\"x1\":%1,\"y1\":%2,\"x2\":%3,\"y2\":%4}")
                                .arg(line->line().x1())
                                .arg(line->line().y1())
                                .arg(line->line().x2())
                                .arg(line->line().y2());
                    break;
                }
                default:
                    // 跳过未知类型
                    break;
            }
            
            jsonData += "}";
        }
    }
    
    jsonData += "]";
    
    mimeData->setData("application/vectorflow/shapes", jsonData.toUtf8());
    
    // 放到剪贴板 - 剪贴板会接管mimeData的所有权
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setMimeData(mimeData);
    } else {
        delete mimeData; // 如果剪贴板不可用，手动删除
    }
    
    m_statusLabel->setText(QString("已复制 %1 个项目").arg(selected.size()));
}

void MainWindow::paste()
{
    if (!m_scene)
        return;

    // 检查剪贴板是否可用
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard) {
        return;
    }
    
    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData) {
        return;
    }
    
    if (!mimeData->hasFormat("application/vectorflow/shapes")) {
        return;
    }

    QByteArray copyData = mimeData->data("application/vectorflow/shapes");
    QString jsonData = QString::fromUtf8(copyData);
    
    // 简单的JSON解析（基础实现）
    // 移除外层方括号
    if (jsonData.startsWith("[") && jsonData.endsWith("]")) {
        jsonData = jsonData.mid(1, jsonData.length() - 2);
    }
    
    // 分割对象（简单实现，不处理嵌套）
    QStringList objectStrings;
    int braceLevel = 0;
    int start = 0;
    
    for (int i = 0; i < jsonData.length(); ++i) {
        if (jsonData[i] == '{') {
            if (braceLevel == 0) {
                start = i;
            }
            braceLevel++;
        } else if (jsonData[i] == '}') {
            braceLevel--;
            if (braceLevel == 0) {
                objectStrings.append(jsonData.mid(start, i - start + 1));
            }
        }
    }
    
    if (objectStrings.isEmpty())
        return;

    // 清除当前选择
    m_scene->clearSelection();
    
    // 偏移量，避免完全重叠
    QPointF offset(20, 20);
    
    // 解析并创建图形
    for (const QString &objStr : objectStrings) {
        // 简单的键值对解析
        QMap<QString, QString> props;
        QString cleanObj = objStr.mid(1, objStr.length() - 2); // 移除 {}
        
        // 更智能的解析，处理嵌套对象
        int braceLevel = 0;
        int start = 0;
        
        for (int i = 0; i < cleanObj.length(); ++i) {
            if (cleanObj[i] == '{') {
                braceLevel++;
            } else if (cleanObj[i] == '}') {
                braceLevel--;
            } else if (cleanObj[i] == ',' && braceLevel == 0) {
                // 找到顶级分隔符
                QString pair = cleanObj.mid(start, i - start).trimmed();
                int colonPos = pair.indexOf(':');
                if (colonPos > 0) {
                    QString key = pair.left(colonPos).trimmed().replace("\"", "");
                    QString value = pair.mid(colonPos + 1).trimmed();
                    props[key] = value;
                }
                start = i + 1;
            }
        }
        
        // 处理最后一个键值对
        if (start < cleanObj.length()) {
            QString pair = cleanObj.mid(start).trimmed();
            int colonPos = pair.indexOf(':');
            if (colonPos > 0) {
                QString key = pair.left(colonPos).trimmed().replace("\"", "");
                QString value = pair.mid(colonPos + 1).trimmed();
                props[key] = value;
            }
        }

        
        bool ok;
        int shapeType = props["type"].toInt(&ok);
        if (!ok) continue;
        
        double x = props["x"].toDouble();
        double y = props["y"].toDouble();
        QPointF pos(x, y);
        
        
        
        DrawingShape *shape = nullptr;
        
        switch ((DrawingShape::ShapeType)shapeType) {
            case DrawingShape::Rectangle: {
                // 解析rect对象
                QString rectStr = props["rect"];
                rectStr = rectStr.mid(1, rectStr.length() - 2); // 移除 {}
                QStringList rectPairs = rectStr.split(',');
                
                if (rectPairs.size() >= 4) {
                    double rx = rectPairs[0].split(':')[1].toDouble();
                    double ry = rectPairs[1].split(':')[1].toDouble();
                    double rw = rectPairs[2].split(':')[1].toDouble();
                    double rh = rectPairs[3].split(':')[1].toDouble();
                    
                    DrawingRectangle *rectangle = new DrawingRectangle(nullptr);
                    rectangle->setRectangle(QRectF(rx, ry, rw, rh));
                    rectangle->setPos(pos + offset);
                    shape = rectangle;
                    
                }
                break;
            }
            case DrawingShape::Ellipse: {
                // 解析ellipse对象
                QString ellipseStr = props["ellipse"];
                ellipseStr = ellipseStr.mid(1, ellipseStr.length() - 2); // 移除 {}
                QStringList ellipsePairs = ellipseStr.split(',');
                
                if (ellipsePairs.size() >= 4) {
                    double ex = ellipsePairs[0].split(':')[1].toDouble();
                    double ey = ellipsePairs[1].split(':')[1].toDouble();
                    double ew = ellipsePairs[2].split(':')[1].toDouble();
                    double eh = ellipsePairs[3].split(':')[1].toDouble();
                    
                    DrawingEllipse *ellipseShape = new DrawingEllipse(nullptr);
                    ellipseShape->setEllipse(QRectF(ex, ey, ew, eh));
                    ellipseShape->setPos(pos + offset);
                    shape = ellipseShape;
                    
                }
                break;
            }
            case DrawingShape::Line: {
                // 解析line对象
                QString lineStr = props["line"];
                lineStr = lineStr.mid(1, lineStr.length() - 2); // 移除 {}
                QStringList linePairs = lineStr.split(',');
                
                if (linePairs.size() >= 4) {
                    double x1 = linePairs[0].split(':')[1].toDouble();
                    double y1 = linePairs[1].split(':')[1].toDouble();
                    double x2 = linePairs[2].split(':')[1].toDouble();
                    double y2 = linePairs[3].split(':')[1].toDouble();
                    
                    DrawingLine *lineShape = new DrawingLine(QLineF(x1, y1, x2, y2), nullptr);
                    lineShape->setPos(pos + offset);
                    shape = lineShape;
                    
                }
                break;
            }
            default:
                continue;
        }
        
        if (shape) {
            // 应用样式
            QString strokeStr = props["stroke"];
            QString fillStr = props["fill"];
            
            // 解析stroke样式
            if (!strokeStr.isEmpty()) {
                QString cleanStroke = strokeStr.mid(1, strokeStr.length() - 2); // 移除 {}
                QStringList strokePairs = cleanStroke.split(',');
                
                if (strokePairs.size() >= 3) {
                    QString colorStr = strokePairs[0].split(':')[1].replace("\"", "");
                    double width = strokePairs[1].split(':')[1].toDouble();
                    int style = strokePairs[2].split(':')[1].toInt();
                    
                    QPen pen((QColor(colorStr)));
                    pen.setWidth(width);
                    pen.setStyle((Qt::PenStyle)style);
                    shape->setStrokePen(pen);
                }
            }
            
            // 解析fill样式
            if (!fillStr.isEmpty()) {
                QString cleanFill = fillStr.mid(1, fillStr.length() - 2); // 移除 {}
                QStringList fillPairs = cleanFill.split(',');
                
                if (fillPairs.size() >= 2) {
                    QString colorStr = fillPairs[0].split(':')[1].replace("\"", "");
                    int style = fillPairs[1].split(':')[1].toInt();
                    
                    QBrush brush((QColor(colorStr)));
                    brush.setStyle((Qt::BrushStyle)style);
                    shape->setFillBrush(brush);
                }
            }
            
            m_scene->addItem(shape);
            shape->setSelected(true);
        }
    }
    
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已粘贴 %1 个项目").arg(objectStrings.size()));
}

void MainWindow::duplicate()
{
    // 快速复制粘贴：先复制，然后立即粘贴
    copySelected();
    paste();
}

void MainWindow::selectAll()
{
    foreach (QGraphicsItem *item, m_scene->items())
    {
        item->setSelected(true);
    }
}

void MainWindow::deselectAll()
{
    m_scene->clearSelection();
}

void MainWindow::zoomIn()
{
    m_canvas->zoomIn();
}

void MainWindow::zoomOut()
{
    m_canvas->zoomOut();
}

void MainWindow::resetZoom()
{
    m_canvas->resetZoom();
}

void MainWindow::fitToWindow()
{
    m_canvas->fitToWindow();
}

void MainWindow::toggleGrid()
{
    if (m_scene)
    {
        m_scene->setGridVisible(!m_scene->isGridVisible());
        m_toggleGridAction->setChecked(m_scene->isGridVisible());
    }
}

void MainWindow::toggleGridAlignment()
{
    if (m_scene)
    {
        bool enabled = !m_scene->isGridAlignmentEnabled();
        m_scene->setGridAlignmentEnabled(enabled);
        m_toggleGridAlignmentAction->setChecked(enabled);
        m_statusLabel->setText(enabled ? "网格对齐已启用" : "网格对齐已禁用");
    }
}

void MainWindow::groupSelected()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    // qDebug() << "groupSelected: selected items count:" << selected.size();
    
    if (selected.size() < 2) {
        // 如果没有选中足够多的项目，给出提示
        m_statusLabel->setText("需要至少选择2个项目才能组合");
        return;
    }
    
    // 计算所有选中项目的边界
    QRectF combinedBounds;
    QList<DrawingShape*> shapesToGroup;
    QList<QPointF> originalPositions;
    
    for (QGraphicsItem *item : selected) {
        if (item && item->parentItem() == nullptr) {  // 确保项目没有父项
            DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
            if (shape) {
                shapesToGroup.append(shape);
                originalPositions.append(shape->pos());
                
                QRectF itemBounds = shape->boundingRect();
                itemBounds.translate(shape->pos());
                
                if (combinedBounds.isEmpty()) {
                    combinedBounds = itemBounds;
                } else {
                    combinedBounds |= itemBounds;
                }
            }
        }
    }
    
    // 创建自定义的DrawingGroup
    DrawingGroup *group = new DrawingGroup();
    // qDebug() << "groupSelected: created DrawingGroup at" << group;
    
    // 设置组的标志，确保它可以被选中和移动
    group->setFlags(QGraphicsItem::ItemIsMovable | 
                    QGraphicsItem::ItemIsSelectable | 
                    QGraphicsItem::ItemSendsGeometryChanges);
    
    // 计算包围盒中心（关键！）
    QPointF center = combinedBounds.center();
    
    // 🌟 先设置组合对象的位置到中心点
    group->setPos(center);
    
    // qDebug() << "groupSelected: total shapes to group:" << shapesToGroup.size();
    // qDebug() << "groupSelected: group position set to center" << center;
    
    // 将组添加到场景中
    m_scene->addItem(group);
    
    // 将所有形状添加到组中
    for (DrawingShape *shape : shapesToGroup) {
        if (shape) {
            // 清除形状的选择状态
            shape->setSelected(false);
            // 将形状添加到组合中
            group->addItem(shape);
            // qDebug() << "groupSelected: added shape to group, shape type:" << shape->type();
        }
    }
    // qDebug() << "groupSelected: added group to scene, group type:" << group->type();
    
    // 清除之前的选择，并选中新的组合
    m_scene->clearSelection();
    group->setSelected(true);
    // qDebug() << "groupSelected: group selected, group isSelected:" << group->isSelected();
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    m_statusLabel->setText(QString("已组合 %1 个项目").arg(shapesToGroup.size()));
}

void MainWindow::ungroupSelected()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    // qDebug() << "ungroupSelected: selected items count:" << selected.size();
    
    if (selected.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    int ungroupedCount = 0;
    
    // 创建一个临时列表来存储要取消组合的组
    QList<DrawingGroup*> groupsToUngroup;
    for (QGraphicsItem *item : selected) {
        // qDebug() << "ungroupSelected: checking item:" << item << "type:" << (item ? item->type() : -1);
        // 使用类型检查而不是qgraphicsitem_cast
        if (item && item->type() == QGraphicsItem::UserType + 1) {
            DrawingShape *shape = static_cast<DrawingShape*>(item);
            if (shape && shape->shapeType() == DrawingShape::Group) {
                DrawingGroup *group = static_cast<DrawingGroup*>(item);
                groupsToUngroup.append(group);
                // qDebug() << "ungroupSelected: found DrawingGroup:" << group;
            }
        }
    }
    
    for (DrawingGroup *group : groupsToUngroup) {
        // 先取消选择组，避免选择层保留引用
        group->setSelected(false);
        
        // 取消组合
        QList<DrawingShape*> shapesToUngroup = group->items();
        
        // 将项目从组中移除并添加回场景
        for (DrawingShape *shape : shapesToUngroup) {
            // 保存组合的位置
            QPointF groupPos = group->pos();
            // 保存子项的相对位置
            QPointF childPos = shape->pos();
            
            group->removeItem(shape);
            
            // 计算子项的绝对位置（组合位置 + 相对位置）
            QPointF absolutePos = groupPos + childPos;
            shape->setPos(absolutePos);
            
            // 确保子项在场景中
            if (!shape->scene()) {
                m_scene->addItem(shape);
            }
        }
        
        // 从场景中移除组
        m_scene->removeItem(group);
        
        // 删除组对象
        delete group;
        
        ungroupedCount++;
    }
    
    if (ungroupedCount > 0) {
        // 清除选择，触发选择状态更新
        m_scene->clearSelection();
        
        // 确保选择层更新
        if (m_scene->selectionLayer()) {
            m_scene->selectionLayer()->updateSelectionBounds();
        }
        
        // 确保场景选择状态更新
        m_scene->update(); // 触发场景重绘
        
        m_scene->setModified(true);
        m_statusLabel->setText(QString("已取消组合 %1 个组").arg(ungroupedCount));
    } else {
        m_statusLabel->setText("没有选中的组合项目");
    }
}

void MainWindow::showGridSettings()
{
    // 这里可以创建一个设置对话框来配置网格
    // 暂时使用简单的消息框
    if (m_scene)
    {
        bool ok;
        int size = QInputDialog::getInt(this, "网格设置",
                                        "请输入网格大小 (像素):",
                                        m_scene->gridSize(),
                                        5, 100, 1, &ok);
        if (ok)
        {
            m_scene->setGridSize(size);
        }

        // 询问用户是否要更改网格颜色
        QColor color = QColorDialog::getColor(m_scene->gridColor(), this, "选择网格颜色");
        if (color.isValid())
        {
            m_scene->setGridColor(color);
        }
    }
}

void MainWindow::updateZoomLabel()
{
    if (m_horizontalRuler && m_verticalRuler && m_canvas)
    {
        double zoom = m_canvas->zoomLevel();
        m_horizontalRuler->setScale(zoom);
        m_verticalRuler->setScale(zoom);

        // 更新缩放标签
        m_zoomLabel->setText(QString("%1%").arg(qRound(zoom * 100)));
        
        // 触发重绘
        m_horizontalRuler->update();
        m_verticalRuler->update();
    }
}

void MainWindow::updateStatusBar(const QString &message)
{
    m_statusLabel->setText(message);
}

void MainWindow::about()
{
    QMessageBox::about(this, "关于 QDrawPro",
                       "VectorQt - 矢量绘图应用\n\n"
                       "一个基于Qt的矢量绘图应用程序，灵感来自Inkscape。\\n\\n"
                       "功能：\\n"
                       "• 基本绘图工具（矩形、椭圆）\\n"
                       "• 选择和变换\\n"
                       "• 撤销/重做支持\\n"
                       "• 属性面板\\n"
                       "• 现代Qt GraphicsView框架");
}

void MainWindow::onSelectionChanged()
{
    updateUI();
    if (m_propertyPanel)
    {
        m_propertyPanel->onSelectionChanged();
    }
    
    // 更新标尺显示
    updateRulerSelection();
}

void MainWindow::onApplyColorToSelection(const QColor &color, bool isFill)
{
    if (!m_scene) return;
    
    // 获取当前选中的所有图形
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) return;
    
    // 创建撤销命令
    class ColorChangeCommand : public QUndoCommand
    {
    public:
        ColorChangeCommand(DrawingScene *scene, const QList<DrawingShape*> &shapes, 
                           const QList<QColor> &oldFillColors, const QList<QColor> &oldStrokeColors,
                           const QColor &newColor, bool isFill, QUndoCommand *parent = nullptr)
            : QUndoCommand(isFill ? "修改填充色" : "修改边框色", parent)
            , m_scene(scene), m_shapes(shapes), m_oldFillColors(oldFillColors)
            , m_oldStrokeColors(oldStrokeColors), m_newColor(newColor), m_isFill(isFill)
        {}
        
        void undo() override {
            for (int i = 0; i < m_shapes.size() && i < m_oldFillColors.size(); ++i) {
                if (m_isFill) {
                    // 检查原始颜色是否为透明
                    if (m_oldFillColors[i] == Qt::transparent) {
                        m_shapes[i]->setFillBrush(QBrush(Qt::NoBrush));
                    } else {
                        m_shapes[i]->setFillBrush(QBrush(m_oldFillColors[i]));
                    }
                } else {
                    // 检查原始颜色是否为透明
                    if (m_oldStrokeColors[i] == Qt::transparent) {
                        m_shapes[i]->setStrokePen(QPen(Qt::NoPen));
                    } else {
                        m_shapes[i]->setStrokePen(QPen(m_oldStrokeColors[i]));
                    }
                }
            }
            if (m_scene) {
                m_scene->update();
                for (DrawingShape *shape : m_shapes) {
                    emit m_scene->objectStateChanged(shape);
                }
            }
        }
        
        void redo() override {
            for (DrawingShape *shape : m_shapes) {
                if (m_isFill) {
                    // 检查新颜色是否为透明
                    if (m_newColor == Qt::transparent) {
                        shape->setFillBrush(QBrush(Qt::NoBrush));
                    } else {
                        shape->setFillBrush(QBrush(m_newColor));
                    }
                } else {
                    // 检查新颜色是否为透明（无边框）
                    if (m_newColor == Qt::transparent) {
                        shape->setStrokePen(QPen(Qt::NoPen));
                    } else {
                        shape->setStrokePen(QPen(m_newColor));
                    }
                }
            }
            if (m_scene) {
                m_scene->update();
                for (DrawingShape *shape : m_shapes) {
                    emit m_scene->objectStateChanged(shape);
                }
            }
        }
        
    private:
        DrawingScene *m_scene;
        QList<DrawingShape*> m_shapes;
        QList<QColor> m_oldFillColors;
        QList<QColor> m_oldStrokeColors;
        QColor m_newColor;
        bool m_isFill;
    };
    
    // 收集选中的DrawingShape对象和原始颜色
    QList<DrawingShape*> shapes;
    QList<QColor> oldFillColors;
    QList<QColor> oldStrokeColors;
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            shapes.append(shape);
            // 检查当前填充样式
            if (shape->fillBrush() == Qt::NoBrush) {
                oldFillColors.append(Qt::transparent);
            } else {
                oldFillColors.append(shape->fillBrush().color());
            }
            // 检查当前边框样式
            if (shape->strokePen() == Qt::NoPen) {
                oldStrokeColors.append(Qt::transparent);
            } else {
                oldStrokeColors.append(shape->strokePen().color());
            }
        }
    }
    
    if (shapes.isEmpty()) return;
    
    // 立即应用颜色变化
    for (DrawingShape *shape : shapes) {
        if (isFill) {
            // 检查是否为透明色（无填充）
            if (color == Qt::transparent) {
                shape->setFillBrush(QBrush(Qt::NoBrush));
            } else {
                shape->setFillBrush(QBrush(color));
            }
        } else {
            // 检查是否为透明色（无边框）
            if (color == Qt::transparent) {
                shape->setStrokePen(QPen(Qt::NoPen));
            } else {
                shape->setStrokePen(QPen(color));
            }
        }
    }
    
    // 创建并推送撤销命令
    ColorChangeCommand *command = new ColorChangeCommand(m_scene, shapes, oldFillColors, oldStrokeColors, color, isFill);
    m_scene->undoStack()->push(command);
    
    // 更新场景
    m_scene->update();
}

void MainWindow::updateRulerSelection()
{
    // 🌟 更新标尺显示选中对象边界
    if (m_scene && m_horizontalRuler && m_verticalRuler) {
        QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
        if (!selectedItems.isEmpty()) {
            // 计算选中对象的联合边界
            QRectF combinedBounds;
            bool first = true;
            for (QGraphicsItem *item : selectedItems) {
                QRectF itemBounds = item->boundingRect();
                itemBounds.translate(item->pos());
                if (first) {
                    combinedBounds = itemBounds;
                    first = false;
                } else {
                    combinedBounds = combinedBounds.united(itemBounds);
                }
            }
            
            // 更新标尺显示
            m_horizontalRuler->setSelectedBounds(combinedBounds);
            m_verticalRuler->setSelectedBounds(combinedBounds);
            
            // 触发重绘
            m_horizontalRuler->update();
            m_verticalRuler->update();
        } else {
            // 清除标尺显示
            m_horizontalRuler->clearSelectedBounds();
            m_verticalRuler->clearSelectedBounds();
            
            // 触发重绘
            m_horizontalRuler->update();
            m_verticalRuler->update();
        }
    }
}

void MainWindow::onSceneChanged()
{
    m_isModified = true;
    updateUI(); // 更新窗口标题显示修改状态
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // 更新标尺显示鼠标位置
    if (m_canvas && m_canvas->view() && m_horizontalRuler && m_verticalRuler)
    {
        // 获取鼠标在视图中的位置
        QPoint viewPos = event->pos();
        m_horizontalRuler->setMousePos(QPointF(viewPos.x(), viewPos.y()));
        m_verticalRuler->setMousePos(QPointF(viewPos.x(), viewPos.y()));
        
        // 触发重绘
        m_horizontalRuler->update();
        m_verticalRuler->update();
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::updateUI()
{
    // Update window title
    QString title = "VectorQt - 矢量绘图应用";
    if (!m_currentFile.isEmpty())
    {
        title += " - " + QFileInfo(m_currentFile).fileName();
    }
    if (m_isModified)
    {
        title += " *";
    }
    setWindowTitle(title);

    // Update undo/redo actions
    if (m_scene && m_scene->undoStack())
    {
        m_undoAction->setEnabled(m_scene->undoStack()->canUndo());
        m_redoAction->setEnabled(m_scene->undoStack()->canRedo());
    }
    else
    {
        m_undoAction->setEnabled(false);
        m_redoAction->setEnabled(false);
    }

    // Update delete action
    bool hasSelection = m_scene && !m_scene->selectedItems().isEmpty();
    m_deleteAction->setEnabled(hasSelection);
    
    // Update group/ungroup actions
    if (m_scene) {
        QList<QGraphicsItem *> selected = m_scene->selectedItems();
        bool hasMultipleSelection = selected.size() > 1;
        bool hasGroupSelection = false;
        
        // 检查是否有选中的组
        for (QGraphicsItem *item : selected) {
            // qDebug() << "updateSelection: checking item" << item << "type:" << (item ? item->type() : -1);
            // 使用类型检查而不是qgraphicsitem_cast
        if (item && item->type() == QGraphicsItem::UserType + 1) {
            DrawingShape *shape = static_cast<DrawingShape*>(item);
            if (shape && shape->shapeType() == DrawingShape::Group) {
                // qDebug() << "updateSelection: found DrawingGroup:" << item;
                hasGroupSelection = true;
                break;
            }
        }
        }
        
        m_groupAction->setEnabled(hasMultipleSelection);
        m_ungroupAction->setEnabled(hasGroupSelection);
    } else {
        m_groupAction->setEnabled(false);
        m_ungroupAction->setEnabled(false);
    }

    // Update save action
    m_saveAction->setEnabled(m_isModified);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    
    // 更新标尺以适应新的窗口大小
    if (m_canvas && m_canvas->view() && m_horizontalRuler && m_verticalRuler) {
        // 更新标尺原点以反映当前视图位置
        QPoint origin = m_canvas->view()->mapFromScene(QPoint(0, 0));
        m_horizontalRuler->setOrigin(origin.x());
        m_verticalRuler->setOrigin(origin.y());
        
        // 更新缩放比例
        DrawingView *drawingView = qobject_cast<DrawingView *>(m_canvas->view());
        if (drawingView) {
            double zoom = drawingView->zoomLevel();
            m_horizontalRuler->setScale(zoom);
            m_verticalRuler->setScale(zoom);
        }
        
        // 触发重绘
        m_horizontalRuler->update();
        m_verticalRuler->update();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isModified)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "VectorQt",
                                                                  "文档已修改，是否保存？",
                                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Save)
        {
            saveFile();
        }
        else if (reply == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
    }

    // 清理剪贴板，避免在应用程序关闭后访问已释放的内存
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->clear();
    }

    event->accept();
}

void MainWindow::alignLeft()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    // 计算所有选中项目的最左边界
    qreal leftmost = std::numeric_limits<qreal>::max();
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        leftmost = qMin(leftmost, bounds.left());
    }
    
    // 将所有项目对齐到最左边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        qreal deltaX = leftmost - bounds.left();
        item->setPos(item->pos().x() + deltaX, item->pos().y());
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已左对齐 %1 个项目").arg(selectedItems.size()));
}

void MainWindow::alignCenter()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    // 计算所有选中项目的中心位置
    qreal leftmost = std::numeric_limits<qreal>::max();
    qreal rightmost = std::numeric_limits<qreal>::lowest();
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        leftmost = qMin(leftmost, bounds.left());
        rightmost = qMax(rightmost, bounds.right());
    }
    
    qreal centerX = (leftmost + rightmost) / 2.0;
    
    // 将所有项目水平居中对齐
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        qreal itemCenterX = (bounds.left() + bounds.right()) / 2.0;
        qreal deltaX = centerX - itemCenterX;
        item->setPos(item->pos().x() + deltaX, item->pos().y());
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已水平居中对齐 %1 个项目").arg(selectedItems.size()));
}

void MainWindow::alignRight()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    // 计算所有选中项目的最右边界
    qreal rightmost = std::numeric_limits<qreal>::lowest();
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        rightmost = qMax(rightmost, bounds.right());
    }
    
    // 将所有项目对齐到最右边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        qreal deltaX = rightmost - bounds.right();
        item->setPos(item->pos().x() + deltaX, item->pos().y());
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已右对齐 %1 个项目").arg(selectedItems.size()));
}

void MainWindow::alignTop()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    // 计算所有选中项目的最顶边界
    qreal topmost = std::numeric_limits<qreal>::max();
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        topmost = qMin(topmost, bounds.top());
    }
    
    // 将所有项目对齐到最顶边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        qreal deltaY = topmost - bounds.top();
        item->setPos(item->pos().x(), item->pos().y() + deltaY);
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已顶部对齐 %1 个项目").arg(selectedItems.size()));
}

void MainWindow::alignMiddle()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    // 计算所有选中项目的中心位置
    qreal topmost = std::numeric_limits<qreal>::max();
    qreal bottommost = std::numeric_limits<qreal>::lowest();
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        topmost = qMin(topmost, bounds.top());
        bottommost = qMax(bottommost, bounds.bottom());
    }
    
    qreal centerY = (topmost + bottommost) / 2.0;
    
    // 将所有项目垂直居中对齐
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        qreal itemCenterY = (bounds.top() + bounds.bottom()) / 2.0;
        qreal deltaY = centerY - itemCenterY;
        item->setPos(item->pos().x(), item->pos().y() + deltaY);
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已垂直居中对齐 %1 个项目").arg(selectedItems.size()));
}

void MainWindow::alignBottom()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    // 计算所有选中项目的最底边界
    qreal bottommost = std::numeric_limits<qreal>::lowest();
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        bottommost = qMax(bottommost, bounds.bottom());
    }
    
    // 将所有项目对齐到最底边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->boundingRect();
        bounds.translate(item->pos());
        qreal deltaY = bottommost - bounds.bottom();
        item->setPos(item->pos().x(), item->pos().y() + deltaY);
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已底部对齐 %1 个项目").arg(selectedItems.size()));
}

// 🌟 参考线创建槽函数
void MainWindow::onGuideRequested(const QPointF &position, Qt::Orientation orientation)
{
    // 提取参考线位置（只需要一个坐标）
    qreal guidePos = (orientation == Qt::Horizontal) ? position.y() : position.x();
    
    // 检查是否已存在相同位置的参考线
    QList<DrawingScene::Guide> existingGuides = m_scene->guides();
    for (const DrawingScene::Guide &guide : existingGuides) {
        if (guide.orientation == orientation && qAbs(guide.position - guidePos) < 2.0) {
            // 如果已存在，则删除该参考线
            m_scene->removeGuide(orientation, guide.position);
            m_statusLabel->setText(QString("删除参考线: %1 @ %2")
                .arg(orientation == Qt::Horizontal ? "水平" : "垂直")
                .arg(guidePos, 0, 'f', 1));
            return;
        }
    }
    
    // 添加新参考线
    m_scene->addGuide(orientation, guidePos);
    m_statusLabel->setText(QString("创建参考线: %1 @ %2")
        .arg(orientation == Qt::Horizontal ? "水平" : "垂直")
        .arg(guidePos, 0, 'f', 1));
}

void MainWindow::clearAllGuides()
{
    if (!m_scene) return;
    
    m_scene->clearGuides();
    m_statusLabel->setText("已清除所有参考线");
}

void MainWindow::distributeHorizontal()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 3) {
        m_statusLabel->setText("水平分布需要至少3个项目");
        return;
    }
    
    // 按X坐标排序
    std::sort(selectedItems.begin(), selectedItems.end(), [](QGraphicsItem* a, QGraphicsItem* b) {
        return a->pos().x() < b->pos().x();
    });
    
    // 计算总宽度和间距
    qreal totalWidth = 0;
    QList<qreal> widths;
    for (QGraphicsItem* item : selectedItems) {
        qreal w = item->boundingRect().width();
        widths.append(w);
        totalWidth += w;
    }
    
    qreal leftmost = selectedItems.first()->pos().x();
    qreal rightmost = selectedItems.last()->pos().x() + widths.last();
    qreal totalSpace = rightmost - leftmost - totalWidth;
    qreal spacing = totalSpace / (selectedItems.size() - 1);
    
    // 重新分布
    qreal currentX = leftmost;
    for (int i = 0; i < selectedItems.size(); ++i) {
        QGraphicsItem* item = selectedItems[i];
        item->setPos(currentX, item->pos().y());
        currentX += widths[i] + spacing;
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已水平分布 %1 个项目").arg(selectedItems.size()));
}

void MainWindow::distributeVertical()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 3) {
        m_statusLabel->setText("垂直分布需要至少3个项目");
        return;
    }
    
    // 按Y坐标排序
    std::sort(selectedItems.begin(), selectedItems.end(), [](QGraphicsItem* a, QGraphicsItem* b) {
        return a->pos().y() < b->pos().y();
    });
    
    // 计算总高度和间距
    qreal totalHeight = 0;
    QList<qreal> heights;
    for (QGraphicsItem* item : selectedItems) {
        qreal h = item->boundingRect().height();
        heights.append(h);
        totalHeight += h;
    }
    
    qreal topmost = selectedItems.first()->pos().y();
    qreal bottommost = selectedItems.last()->pos().y() + heights.last();
    qreal totalSpace = bottommost - topmost - totalHeight;
    qreal spacing = totalSpace / (selectedItems.size() - 1);
    
    // 重新分布
    qreal currentY = topmost;
    for (int i = 0; i < selectedItems.size(); ++i) {
        QGraphicsItem* item = selectedItems[i];
        item->setPos(item->pos().x(), currentY);
        currentY += heights[i] + spacing;
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已垂直分布 %1 个项目").arg(selectedItems.size()));
}

QColor MainWindow::getCurrentFillColor() const
{
    if (m_colorPalette) {
        return m_colorPalette->getCurrentFillColor();
    }
    return Qt::blue; // 默认颜色
}

// 路径布尔运算槽函数实现
void MainWindow::pathUnion()
{
    performPathBooleanOperation(0, "联合"); // PathEditor::Union = 0
}

void MainWindow::pathSubtract()
{
    performPathBooleanOperation(2, "减去"); // PathEditor::Subtraction = 2
}

void MainWindow::pathIntersect()
{
    performPathBooleanOperation(1, "相交"); // PathEditor::Intersection = 1
}

void MainWindow::pathXor()
{
    performPathBooleanOperation(3, "异或"); // PathEditor::Xor = 3
}

// 执行路径布尔运算的通用方法
void MainWindow::performPathBooleanOperation(int op, const QString &opName)
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 2) {
        m_statusLabel->setText(QString("%1操作需要至少选中2个图形").arg(opName));
        return;
    }
    
    // 收集所有选中图形的路径
    QList<QPainterPath> paths;
    QList<DrawingShape*> shapes;
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            // 获取图形的路径
            QPainterPath shapePath;
            
            // 根据图形类型获取路径
            if (shape->shapeType() == DrawingShape::Path) {
                DrawingPath *drawingPath = qgraphicsitem_cast<DrawingPath*>(shape);
                if (drawingPath) {
                    shapePath = drawingPath->path();
                }
            } else {
                // 对于其他图形，创建对应的路径
                QRectF bounds = shape->boundingRect();
                if (shape->shapeType() == DrawingShape::Rectangle) {
                    shapePath.addRect(bounds);
                } else if (shape->shapeType() == DrawingShape::Ellipse) {
                    shapePath.addEllipse(bounds);
                }
                // 可以添加更多图形类型的支持
            }
            
            if (!shapePath.isEmpty()) {
                paths.append(shapePath);
                shapes.append(shape);
            }
        }
    }
    
    if (paths.size() < 2) {
        m_statusLabel->setText(QString("没有找到可进行%1操作的图形").arg(opName));
        return;
    }
    
    // 执行布尔运算
    QPainterPath resultPath = paths[0];
    for (int i = 1; i < paths.size(); ++i) {
        resultPath = PathEditor::booleanOperation(resultPath, paths[i], static_cast<PathEditor::BooleanOperation>(op));
    }
    
    if (resultPath.isEmpty()) {
        m_statusLabel->setText(QString("%1操作结果为空").arg(opName));
        return;
    }
    
    // 创建新的路径图形
    DrawingPath *newPath = new DrawingPath();
    newPath->setPath(resultPath);
    
    // 设置新图形的位置和样式
    if (!shapes.isEmpty()) {
        // 使用第一个图形的位置
        newPath->setPos(shapes.first()->pos());
        
        // 使用第一个图形的样式
        newPath->setFillBrush(shapes.first()->fillBrush());
        newPath->setStrokePen(shapes.first()->strokePen());
    }
    
    // 添加到场景
    m_scene->addItem(newPath);
    
    // 删除原始图形
    for (DrawingShape *shape : shapes) {
        m_scene->removeItem(shape);
        delete shape;
    }
    
    // 选中新创建的图形
    newPath->setSelected(true);
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    m_statusLabel->setText(QString("%1操作完成").arg(opName));
}


