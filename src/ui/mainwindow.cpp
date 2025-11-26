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
#include <QUndoCommand>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QColorDialog>
#include <QScrollBar>
#include <QIcon>
#include <QElapsedTimer>
#include <QDateTime>
#include <algorithm>
#include "../ui/mainwindow.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-canvas.h"
#include "../core/toolbase.h"
#include "../ui/propertypanel.h"
#include "../ui/tabbed-property-panel.h"
#include "../ui/tools-panel.h"
#include "../tools/drawing-tool-brush.h"
#include "../tools/drawing-tool-pen.h"

#include "../tools/drawing-tool-eraser.h"
#include "../core/patheditor.h"
#include "../core/layer-manager.h"
#include "../core/drawing-layer.h"
#include "../tools/drawing-tool-fill.h"
#include "../core/layer-manager.h"
#include "../ui/layer-panel.h"
#include "../tools/drawing-tool-bezier.h"
// #include "drawing-tool-bezier-edit.h" // 已移除 - 待重新实现
#include "../tools/drawing-tool-node-edit.h"
#include "../tools/drawing-tool-polyline.h"
#include "../tools/drawing-tool-polygon.h"
#include "../tools/drawing-tool-fill.h"
#include "../tools/drawing-tool-gradient-fill.h"
#include "../tools/drawing-tool-line.h"
#include "../tools/drawing-tool-path-edit.h"
#include "../tools/drawing-tool-outline-preview.h"
#include "../core/patheditor.h"
#include "../ui/ruler.h"
#include "../ui/scrollable-toolbar.h"
#include "../core/svghandler.h"
#include "../core/drawing-shape.h"
#include "../ui/colorpalette.h"
#include "../core/drawing-group.h"
#include "../tools/tool-state-manager.h"
#include "../tools/tool-manager.h"
// #include "../core/event-bus.h"  // 暂时不使用，保留为未来扩展
#include "../ui/shortcut-manager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_scene(nullptr), m_canvas(nullptr), m_propertyPanel(nullptr), m_tabbedPropertyPanel(nullptr), m_undoView(nullptr), m_layerManager(nullptr), m_currentTool(nullptr),
      m_colorPalette(nullptr), m_scrollableToolBar(nullptr),
      m_horizontalRuler(nullptr), m_verticalRuler(nullptr), m_cornerWidget(nullptr), m_isModified(false), m_lastOpenDir(QDir::homePath()), m_lastSaveDir(QDir::homePath()),
      m_uiUpdateTimer(nullptr), m_lastSelectedCount(0), m_toolStateManager(nullptr), m_toolManager(nullptr), m_shortcutManager(nullptr)
{
    // 初始化工具管理系统
    m_toolStateManager = new ToolStateManager(this);
    m_toolManager = new ToolManager(this);
    m_toolManager->setToolStateManager(m_toolStateManager);
    
    // 初始化快捷键管理器
    m_shortcutManager = new ShortcutManager(this);
    m_shortcutManager->setToolManager(m_toolManager);
    // 场景将在newFile()后设置
    
    // Event-Bus暂时不使用，保留为未来扩展
    // 当前主要使用Qt信号槽进行组件间通信
    // EventBus使用策略详见：docs/hybrid-communication-strategy.md
    
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

    // 创建UI更新定时器
    m_uiUpdateTimer = new QTimer(this);
    connect(m_uiUpdateTimer, &QTimer::timeout, this, [this]() {
        if (m_scene) {
            int currentCount = m_scene->selectedItems().count();
            if (currentCount != m_lastSelectedCount) {
                m_lastSelectedCount = currentCount;
                updateUI();
            }
        }
    });
    m_uiUpdateTimer->start(100); // 每100ms检查一次

    // 不设置默认工具，让用户手动选择

    resize(1200, 800);
    setWindowTitle(tr("VectorQt - 专业矢量绘图工具"));
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
    // 手动清理ToolManager，确保所有工具在scene删除前被清理
    if (m_toolManager) {
        delete m_toolManager;
        m_toolManager = nullptr;
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

    // 设置工具管理器的场景和视图
    if (m_toolManager && m_canvas->view()) {
        m_toolManager->setScene(m_scene);
        m_toolManager->setView(qobject_cast<DrawingView*>(m_canvas->view()));
    }

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

    // 使用工具管理器创建和管理所有工具
    if (m_toolManager) {
        m_toolManager->createAndRegisterTool<OutlinePreviewTransformTool>(ToolType::Select, this);
        m_toolManager->createAndRegisterTool<LegacyRectangleTool>(ToolType::Rectangle, this);
        m_toolManager->createAndRegisterTool<LegacyEllipseTool>(ToolType::Ellipse, this);
        m_toolManager->createAndRegisterTool<DrawingBezierTool>(ToolType::Bezier, this);
        m_toolManager->createAndRegisterTool<DrawingNodeEditTool>(ToolType::NodeEdit, this);
        m_toolManager->createAndRegisterTool<DrawingToolPolyline>(ToolType::Polyline, this);
        m_toolManager->createAndRegisterTool<DrawingToolPolygon>(ToolType::Polygon, this);
        m_toolManager->createAndRegisterTool<DrawingToolBrush>(ToolType::Brush, this);
        m_toolManager->createAndRegisterTool<DrawingToolFill>(ToolType::Fill, this);
        m_toolManager->createAndRegisterTool<DrawingToolGradientFill>(ToolType::GradientFill, this);
        m_toolManager->createAndRegisterTool<DrawingToolPen>(ToolType::Pen, this);
        m_toolManager->createAndRegisterTool<DrawingToolEraser>(ToolType::Eraser, this);
        m_toolManager->createAndRegisterTool<DrawingToolLine>(ToolType::Line, this);
        m_toolManager->createAndRegisterTool<DrawingToolPathEdit>(ToolType::PathEdit, this);
        
        // 连接状态栏更新信号（针对选择工具）
        auto selectTool = m_toolManager->getToolAs<OutlinePreviewTransformTool>(ToolType::Select);
        if (selectTool) {
            connect(selectTool, SIGNAL(statusMessageChanged(const QString&)),
                    this, SLOT(updateStatusBar(const QString&)));
        }
    }
    
    // 性能监控现在集成在属性面板的tab页中，不需要单独的浮动窗口
    
    // 创建一个空的停靠widget用于菜单控制（保留兼容性）
    m_performanceDock = new QDockWidget("性能监控", this);
    m_performanceDock->setVisible(false);  // 隐藏停靠widget
    
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
    connect(m_scene, &DrawingScene::objectStateChanged,
            this, &MainWindow::onObjectStateChanged);
    connect(m_scene, &DrawingScene::contextMenuRequested,
            this, &MainWindow::showContextMenu);
    connect(m_scene, &DrawingScene::toolSwitchRequested,
            this, &MainWindow::onToolSwitchRequested);
    // 连接DrawingCanvas的缩放信号
    connect(m_canvas, &DrawingCanvas::zoomChanged,
            this, &MainWindow::updateZoomLabel);
            
    // Connect color palette signals to tools (通过ToolManager获取工具)
    if (m_colorPalette && m_toolManager) {
        auto fillTool = m_toolManager->getToolAs<DrawingToolFill>(ToolType::Fill);
        if (fillTool) {
            connect(m_colorPalette, &ColorPalette::fillColorChanged,
                    fillTool, &DrawingToolFill::onFillColorChanged);
        }
        
        auto gradientFillTool = m_toolManager->getToolAs<DrawingToolGradientFill>(ToolType::GradientFill);
        if (gradientFillTool) {
            connect(m_colorPalette, &ColorPalette::fillColorChanged,
                    gradientFillTool, &DrawingToolGradientFill::onFillColorChanged);
        }
        
        auto penTool = m_toolManager->getToolAs<DrawingToolPen>(ToolType::Pen);
        if (penTool) {
            connect(m_colorPalette, &ColorPalette::strokeColorChanged,
                    penTool, &DrawingToolPen::onStrokeColorChanged);
            connect(m_colorPalette, &ColorPalette::fillColorChanged,
                    penTool, &DrawingToolPen::onFillColorChanged);
        }
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
    
    // 注册工具快捷键到快捷键管理器
    if (m_shortcutManager) {
        // 注册永久快捷键
        m_shortcutManager->registerShortcut(ToolType::Select, QKeySequence("V"), m_outlinePreviewToolAction);
        m_shortcutManager->registerShortcut(ToolType::Rectangle, QKeySequence("R"), m_rectangleToolAction);
        m_shortcutManager->registerShortcut(ToolType::Ellipse, QKeySequence("E"), m_ellipseToolAction);
        m_shortcutManager->registerShortcut(ToolType::Bezier, QKeySequence("B"), m_bezierToolAction);
        m_shortcutManager->registerShortcut(ToolType::Polyline, QKeySequence("Y"), m_polylineToolAction);
        m_shortcutManager->registerShortcut(ToolType::Polygon, QKeySequence("Shift+P"), m_polygonToolAction);
        m_shortcutManager->registerShortcut(ToolType::Brush, QKeySequence("Shift+B"), m_brushToolAction);
        m_shortcutManager->registerShortcut(ToolType::Fill, QKeySequence("F"), m_fillToolAction);
        m_shortcutManager->registerShortcut(ToolType::GradientFill, QKeySequence("Shift+G"), m_gradientFillToolAction);
        m_shortcutManager->registerShortcut(ToolType::Pen, QKeySequence("P"), m_penToolAction);
        m_shortcutManager->registerShortcut(ToolType::Eraser, QKeySequence("Shift+E"), m_eraserToolAction);
        m_shortcutManager->registerShortcut(ToolType::Line, QKeySequence("L"), m_lineToolAction);
        m_shortcutManager->registerShortcut(ToolType::PathEdit, QKeySequence("Ctrl+Shift+P"), m_pathEditToolAction);
        
        // 注册临时快捷键（按住临时切换，松开恢复）
        // 使用不和永久快捷键冲突的按键
        m_shortcutManager->registerTemporaryShortcut(ToolType::Brush, QKeySequence("X"));
        m_shortcutManager->registerTemporaryShortcut(ToolType::Eraser, QKeySequence("C"));
        m_shortcutManager->registerTemporaryShortcut(ToolType::Fill, QKeySequence("D"));
        m_shortcutManager->registerTemporaryShortcut(ToolType::Select, QKeySequence("Space"));
        m_shortcutManager->registerTemporaryShortcut(ToolType::Pen, QKeySequence("Q"));
        m_shortcutManager->registerTemporaryShortcut(ToolType::Rectangle, QKeySequence("W"));
        m_shortcutManager->registerTemporaryShortcut(ToolType::Ellipse, QKeySequence("A"));
        m_shortcutManager->registerTemporaryShortcut(ToolType::Line, QKeySequence("S"));
        
        // 注册上下文相关的快捷键
        // 节点编辑工具的快捷键只在有选中对象时有效
        m_shortcutManager->registerShortcut(ToolType::NodeEdit, QKeySequence("N"), m_nodeEditToolAction, 
                                          ShortcutContext::HasSelection);
    }
    
    // 设置默认工具为选择工具（在所有工具创建完成后）
    if (m_toolManager) {
        // 先设置当前工具指针
        m_currentTool = m_toolManager->getTool(ToolType::Select);
        // 然后切换到选择工具
        if (m_currentTool) {
            m_toolManager->switchTool(ToolType::Select);
            // 确保选择工具的动作被选中
            if (m_outlinePreviewToolAction) {
                m_outlinePreviewToolAction->setChecked(true);
            }
        }
    }
    
    // 初始化UI更新定时器
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
    editMenu->addAction(m_bringToFrontAction);
    editMenu->addAction(m_sendToBackAction);
    editMenu->addAction(m_bringForwardAction);
    editMenu->addAction(m_sendBackwardAction);
    editMenu->addSeparator();
    editMenu->addAction(m_alignLeftAction);
    editMenu->addAction(m_alignCenterAction);
    editMenu->addAction(m_alignRightAction);
    editMenu->addSeparator();
    editMenu->addAction(m_alignTopAction);
    editMenu->addAction(m_alignMiddleAction);
    editMenu->addAction(m_alignBottomAction);
    editMenu->addSeparator();
    editMenu->addAction(m_sameWidthAction);
    editMenu->addAction(m_sameHeightAction);
    editMenu->addAction(m_sameSizeAction);
    editMenu->addSeparator();
    editMenu->addAction(m_distributeHorizontalAction);
    editMenu->addAction(m_distributeVerticalAction);

    // Filter menu
    QMenu *filterMenu = menuBar()->addMenu("滤镜(&T)");
    filterMenu->addAction(m_blurEffectAction);
    filterMenu->addAction(m_dropShadowEffectAction);
    filterMenu->addSeparator();
    filterMenu->addAction(m_clearFilterAction);

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
    viewMenu->addAction(m_togglePerformancePanelAction);
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
    toolsMenu->addAction(m_textToolAction);

    // Path menu - 路径编辑功能已集成到选择工具中
    QMenu *pathMenu = menuBar()->addMenu("&路径");
    
    // 添加路径布尔运算操作
    pathMenu->addSeparator();
    m_pathUnionAction = new QAction("联合(&U)", this);
    m_pathUnionAction->setStatusTip(tr("将选中的图形联合成一个路径"));
    pathMenu->addAction(m_pathUnionAction);
    
    m_pathSubtractAction = new QAction("减去(&S)", this);
    m_pathSubtractAction->setStatusTip(tr("从第一个选中图形中减去其他图形"));
    pathMenu->addAction(m_pathSubtractAction);
    
    m_pathIntersectAction = new QAction("相交(&I)", this);
    m_pathIntersectAction->setStatusTip(tr("获取选中图形的相交部分"));
    pathMenu->addAction(m_pathIntersectAction);
    
    m_pathXorAction = new QAction("异或(&X)", this);
    m_pathXorAction->setStatusTip(tr("获取选中图形的异或部分"));
    pathMenu->addAction(m_pathXorAction);
    
    pathMenu->addSeparator();
    
    // 路径编辑操作
    m_pathSimplifyAction = new QAction("简化路径(&M)", this);
    m_pathSimplifyAction->setStatusTip(tr("简化选中路径，减少节点数量"));
    pathMenu->addAction(m_pathSimplifyAction);
    
    m_pathSmoothAction = new QAction("平滑路径(&S)", this);
    m_pathSmoothAction->setStatusTip(tr("平滑选中路径的曲线"));
    pathMenu->addAction(m_pathSmoothAction);
    
    m_pathReverseAction = new QAction("反转路径(&R)", this);
    m_pathReverseAction->setStatusTip(tr("反转选中路径的方向"));
    pathMenu->addAction(m_pathReverseAction);
    
    pathMenu->addSeparator();
    
    m_generateShapeAction = new QAction("生成图形(&G)", this);
    m_generateShapeAction->setStatusTip(tr("从选中路径生成标准图形"));
    pathMenu->addAction(m_generateShapeAction);

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
    m_scrollableToolBar->setIconSize(QSize(24, 24)); // 统一图标大小
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
    m_scrollableToolBar->addAction(m_textToolAction);

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
    viewToolBar->addAction(m_sameWidthAction);
    viewToolBar->addAction(m_sameHeightAction);
    viewToolBar->addAction(m_sameSizeAction);
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
    // 使用单例模式获取图层管理器
    m_layerManager = LayerManager::instance();
    
    // Create TabBar property panel
    m_tabbedPropertyPanel = new TabbedPropertyPanel(this);
    m_tabbedPropertyPanel->setScene(m_scene);
    if (m_canvas && m_canvas->view()) {
        m_tabbedPropertyPanel->setView(m_canvas->view());
    }
    
    // 连接图层管理器
    if (m_layerManager) {
        
        LayerPanel *layerPanel = m_tabbedPropertyPanel->getLayersPanel();
        if (layerPanel) {
            
            layerPanel->setScene(m_scene);
            m_layerManager->setLayerPanel(layerPanel);
        } else {
            
        }
        m_layerManager->setScene(m_scene);
        
        // 重要：设置TabbedPropertyPanel的LayerManager引用
        m_tabbedPropertyPanel->setLayerManager(m_layerManager);
    } else {
        
    }
    
    // 连接工具面板信号到槽函数
    ToolsPanel *toolsPanel = m_tabbedPropertyPanel->getToolsPanel();
    if (toolsPanel) {
        connect(toolsPanel, &ToolsPanel::brushSizeChanged, this, &MainWindow::onBrushSizeChanged);
        connect(toolsPanel, &ToolsPanel::brushOpacityChanged, this, &MainWindow::onBrushOpacityChanged);
        connect(toolsPanel, &ToolsPanel::brushSmoothingChanged, this, &MainWindow::onBrushSmoothingChanged);
        connect(toolsPanel, &ToolsPanel::eraserSizeChanged, this, &MainWindow::onEraserSizeChanged);
        connect(toolsPanel, &ToolsPanel::fillToleranceChanged, this, &MainWindow::onFillToleranceChanged);
        connect(toolsPanel, &ToolsPanel::strokeWidthChanged, this, &MainWindow::onStrokeWidthChanged);
        connect(toolsPanel, &ToolsPanel::antialiasingChanged, this, &MainWindow::onAntialiasingChanged);
        connect(toolsPanel, &ToolsPanel::snapToGridChanged, this, &MainWindow::onSnapToGridChanged);
        connect(toolsPanel, &ToolsPanel::penPressureSupportChanged, this, &MainWindow::onPenPressureSupportChanged);
        connect(toolsPanel, &ToolsPanel::penJoinStyleChanged, this, &MainWindow::onPenJoinStyleChanged);
        connect(toolsPanel, &ToolsPanel::penCapStyleChanged, this, &MainWindow::onPenCapStyleChanged);
    }
    
    
    
    // Properties dock with TabBar layout
    QDockWidget *propertiesDock = new QDockWidget(tr("面板"), this);
    propertiesDock->setWidget(m_tabbedPropertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);
    
    // Keep reference to the old property panel for compatibility
    m_propertyPanel = m_tabbedPropertyPanel->getPropertiesPanel();

    // Undo history dock - now integrated into TabBar panel
    QDockWidget *historyDock = new QDockWidget(tr("历史记录"), this);
    m_undoView = new QUndoView(m_scene->undoStack(), historyDock);
    historyDock->setWidget(m_undoView);
    addDockWidget(Qt::RightDockWidgetArea, historyDock);
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
    m_newAction->setStatusTip(tr("创建新文档"));

    m_openAction = new QAction("&打开...", this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip(tr("打开现有文档"));

    m_saveAction = new QAction("&保存", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip(tr("保存文档到磁盘"));

    m_saveAsAction = new QAction("另存为...", this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip(tr("以新名称保存文档"));

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

    // 滤镜Actions
    m_blurEffectAction = new QAction("高斯模糊(&B)", this);
    m_blurEffectAction->setStatusTip("为选中对象添加高斯模糊效果");

    m_dropShadowEffectAction = new QAction("阴影(&S)", this);
    m_dropShadowEffectAction->setStatusTip("为选中对象添加阴影效果");

    m_clearFilterAction = new QAction("清除滤镜(&C)", this);
    m_clearFilterAction->setStatusTip("清除选中对象的所有滤镜效果");

    m_convertTextToPathAction = new QAction("文本转路径(&T)", this);
    m_convertTextToPathAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    m_convertTextToPathAction->setStatusTip("将选中的文本转换为可编辑的路径");

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
    
    // Z-order actions
    m_bringToFrontAction = new QAction("置于顶层(&F)", this);
    m_bringToFrontAction->setShortcut(QKeySequence("Ctrl+Shift+]"));
    m_bringToFrontAction->setStatusTip("将选中项目置于顶层");
    
    m_sendToBackAction = new QAction("置于底层(&B)", this);
    m_sendToBackAction->setShortcut(QKeySequence("Ctrl+Shift+["));
    m_sendToBackAction->setStatusTip("将选中项目置于底层");
    
    m_bringForwardAction = new QAction("上移一层(&R)", this);
    m_bringForwardAction->setShortcut(QKeySequence("Ctrl+]"));
    m_bringForwardAction->setStatusTip("将选中项目上移一层");
    
    m_sendBackwardAction = new QAction("下移一层(&S)", this);
    m_sendBackwardAction->setShortcut(QKeySequence("Ctrl+["));
    m_sendBackwardAction->setStatusTip("将选中项目下移一层");
    
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
    
    m_sameWidthAction = new QAction("同宽(&W)", this);
    m_sameWidthAction->setStatusTip("将选中的项目设置为相同宽度");
    
    m_sameHeightAction = new QAction("同高(&H)", this);
    m_sameHeightAction->setStatusTip("将选中的项目设置为相同高度");
    
    m_sameSizeAction = new QAction("同大小(&S)", this);
    m_sameSizeAction->setStatusTip("将选中的项目设置为相同大小");
    
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
    m_outlinePreviewToolAction->setIcon(QIcon(":/icons/icons/tool-pointer.svg"));
    m_toolGroup->addAction(m_outlinePreviewToolAction);

    m_rectangleToolAction = new QAction("&矩形工具", this);
    m_rectangleToolAction->setCheckable(true);
    m_rectangleToolAction->setShortcut(QKeySequence("R"));
    m_rectangleToolAction->setStatusTip("绘制矩形");
    m_rectangleToolAction->setIcon(QIcon(":/icons/icons/draw-rectangle.svg"));
    m_toolGroup->addAction(m_rectangleToolAction);

    m_ellipseToolAction = new QAction("&椭圆工具", this);
    m_ellipseToolAction->setCheckable(true);
    m_ellipseToolAction->setShortcut(QKeySequence("E"));
    m_ellipseToolAction->setStatusTip("绘制椭圆");
    m_ellipseToolAction->setIcon(QIcon(":/icons/icons/draw-ellipse.svg"));
    m_toolGroup->addAction(m_ellipseToolAction);

    m_bezierToolAction = new QAction("&贝塞尔曲线工具", this);
    m_bezierToolAction->setCheckable(true);
    m_bezierToolAction->setShortcut(QKeySequence("B"));
    m_bezierToolAction->setStatusTip("绘制贝塞尔曲线");
    m_bezierToolAction->setIcon(QIcon(":/icons/icons/draw-path.svg"));
    m_toolGroup->addAction(m_bezierToolAction);
    
    
    
    m_nodeEditToolAction = new QAction("节&点编辑工具", this);
    m_nodeEditToolAction->setCheckable(true);
    m_nodeEditToolAction->setShortcut(QKeySequence("N"));
    m_nodeEditToolAction->setStatusTip("编辑图形节点和控制点");
    m_nodeEditToolAction->setIcon(QIcon(":/icons/icons/tool-node-editor.svg")); // 使用专用节点编辑图标
    m_toolGroup->addAction(m_nodeEditToolAction);

    m_polylineToolAction = new QAction("&折线工具", this);
    m_polylineToolAction->setCheckable(true);
    m_polylineToolAction->setShortcut(QKeySequence("Y"));
    m_polylineToolAction->setStatusTip("绘制折线");
    m_polylineToolAction->setIcon(QIcon(":/icons/icons/polyline-tool-new.svg"));
    m_toolGroup->addAction(m_polylineToolAction);

    m_polygonToolAction = new QAction("&多边形工具", this);
    m_polygonToolAction->setCheckable(true);
    m_polygonToolAction->setShortcut(QKeySequence("Shift+P"));
    m_polygonToolAction->setStatusTip("绘制多边形");
    m_polygonToolAction->setIcon(QIcon(":/icons/icons/draw-polygon.svg"));
    m_toolGroup->addAction(m_polygonToolAction);

    m_brushToolAction = new QAction("&画笔工具", this);
    m_brushToolAction->setCheckable(true);
    m_brushToolAction->setShortcut(QKeySequence("Shift+B"));
    m_brushToolAction->setStatusTip("自由绘制");
    m_brushToolAction->setIcon(QIcon(":/icons/icons/draw-freehand.svg"));
    m_toolGroup->addAction(m_brushToolAction);

    m_fillToolAction = new QAction("&填充工具", this);
    m_fillToolAction->setCheckable(true);
    m_fillToolAction->setShortcut(QKeySequence("F"));
    m_fillToolAction->setStatusTip("填充区域");
    m_fillToolAction->setIcon(QIcon(":/icons/icons/color-fill.svg"));
    m_toolGroup->addAction(m_fillToolAction);

    m_gradientFillToolAction = new QAction("&渐进填充工具", this);
    m_gradientFillToolAction->setCheckable(true);
    m_gradientFillToolAction->setShortcut(QKeySequence("Shift+G"));
    m_gradientFillToolAction->setStatusTip("渐进填充区域");
    m_gradientFillToolAction->setIcon(QIcon(":/icons/icons/color-gradient.svg"));
    m_toolGroup->addAction(m_gradientFillToolAction);

    m_penToolAction = new QAction("&钢笔工具", this);
    m_penToolAction->setCheckable(true);
    m_penToolAction->setShortcut(QKeySequence("P"));
    m_penToolAction->setStatusTip("绘制贝塞尔曲线路径");
    m_penToolAction->setIcon(QIcon(":/icons/icons/draw-calligraphic.svg"));
    m_toolGroup->addAction(m_penToolAction);

    m_eraserToolAction = new QAction("&橡皮擦工具", this);
    m_eraserToolAction->setCheckable(true);
    m_eraserToolAction->setShortcut(QKeySequence("Shift+E"));
    m_eraserToolAction->setStatusTip("擦除图形或图形的部分区域");
    m_eraserToolAction->setIcon(QIcon(":/icons/icons/draw-eraser.svg"));
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

    m_textToolAction = new QAction("&文本工具", this);
    m_textToolAction->setCheckable(true);
    m_textToolAction->setShortcut(QKeySequence("T"));
    m_textToolAction->setStatusTip("添加文本");
    m_textToolAction->setIcon(QIcon(":/icons/icons/draw-text.svg"));
    m_toolGroup->addAction(m_textToolAction);

    // Performance panel action
    m_togglePerformancePanelAction = new QAction("性能监控面板", this);
    m_togglePerformancePanelAction->setCheckable(true);
    m_togglePerformancePanelAction->setChecked(true);
    m_togglePerformancePanelAction->setShortcut(QKeySequence("F12"));
    m_togglePerformancePanelAction->setStatusTip("显示或隐藏性能监控面板");
    m_togglePerformancePanelAction->setIcon(QIcon(":/icons/icons/modern/preview.html"));  // 临时图标

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
    connect(m_blurEffectAction, &QAction::triggered, this, &MainWindow::applyBlurEffect);
    connect(m_dropShadowEffectAction, &QAction::triggered, this, &MainWindow::applyDropShadowEffect);
    connect(m_clearFilterAction, &QAction::triggered, this, &MainWindow::clearFilterEffect);
    connect(m_convertTextToPathAction, &QAction::triggered, this, &MainWindow::convertTextToPath);
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
    connect(m_togglePerformancePanelAction, &QAction::triggered, this, &MainWindow::togglePerformancePanel);
    
    // Group connections
    connect(m_groupAction, &QAction::triggered, this, &MainWindow::groupSelected);
    connect(m_ungroupAction, &QAction::triggered, this, &MainWindow::ungroupSelected);
    
    // Z-order connections
    connect(m_bringToFrontAction, &QAction::triggered, this, &MainWindow::bringToFront);
    connect(m_sendToBackAction, &QAction::triggered, this, &MainWindow::sendToBack);
    connect(m_bringForwardAction, &QAction::triggered, this, &MainWindow::bringForward);
    connect(m_sendBackwardAction, &QAction::triggered, this, &MainWindow::sendBackward);
    
    // Align connections
    connect(m_alignLeftAction, &QAction::triggered, this, &MainWindow::alignLeft);
    connect(m_alignCenterAction, &QAction::triggered, this, &MainWindow::alignCenter);
    connect(m_alignRightAction, &QAction::triggered, this, &MainWindow::alignRight);
    connect(m_alignTopAction, &QAction::triggered, this, &MainWindow::alignTop);
    connect(m_alignMiddleAction, &QAction::triggered, this, &MainWindow::alignMiddle);
    connect(m_alignBottomAction, &QAction::triggered, this, &MainWindow::alignBottom);
    connect(m_sameWidthAction, &QAction::triggered, this, &MainWindow::sameWidth);
    connect(m_sameHeightAction, &QAction::triggered, this, &MainWindow::sameHeight);
    connect(m_sameSizeAction, &QAction::triggered, this, &MainWindow::sameSize);
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
    connect(m_textToolAction, &QAction::triggered, this, &MainWindow::textTool);
    
    // Path boolean operation connections
    connect(m_pathUnionAction, &QAction::triggered, this, &MainWindow::pathUnion);
    connect(m_pathSubtractAction, &QAction::triggered, this, &MainWindow::pathSubtract);
    connect(m_pathIntersectAction, &QAction::triggered, this, &MainWindow::pathIntersect);
    connect(m_pathXorAction, &QAction::triggered, this, &MainWindow::pathXor);
    
    // 路径编辑连接
    connect(m_pathSimplifyAction, &QAction::triggered, this, &MainWindow::pathSimplify);
    connect(m_pathSmoothAction, &QAction::triggered, this, &MainWindow::pathSmooth);
    connect(m_pathReverseAction, &QAction::triggered, this, &MainWindow::pathReverse);
    connect(m_generateShapeAction, &QAction::triggered, this, &MainWindow::generateShape);

    // Help connections
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::about);
}

void MainWindow::setCurrentTool(ToolBase *tool)
{
    if (!m_toolManager) {
        qWarning() << "MainWindow: Tool manager not initialized";
        return;
    }
    
    // 使用工具管理器进行切换
    if (!m_toolManager->switchTool(tool)) {
        qWarning() << "MainWindow: Failed to switch tool";
        return;
    }
    
    // 更新当前工具引用
    m_currentTool = m_toolManager->currentTool();
    
    // 更新工具面板显示当前工具设置
    if (m_tabbedPropertyPanel && m_tabbedPropertyPanel->getToolsPanel()) {
        m_tabbedPropertyPanel->getToolsPanel()->setCurrentTool(m_currentTool);
    }
    
    // 优化选择清除逻辑：只在切换到非选择类工具时清除选择
    if (m_scene && tool && m_toolManager) {
        auto nodeEditTool = m_toolManager->getTool(ToolType::NodeEdit);
        auto selectTool = m_toolManager->getTool(ToolType::Select);
        if (tool != nodeEditTool && tool != selectTool) {
        // 检查并取消鼠标抓取
        if (m_scene->mouseGrabberItem()) {
            m_scene->mouseGrabberItem()->ungrabMouse();
        }
            
            m_scene->clearSelection();
        
        // 优化：只遍历一次项目列表，同时处理选中状态和控制点
        const QList<QGraphicsItem*> allItems = m_scene->items(Qt::AscendingOrder);
        for (QGraphicsItem *item : allItems) {
            if (!item || item->scene() != m_scene) {
                continue;
            }
            
            // 清除选中状态
            if (item->isSelected()) {
                item->setSelected(false);
            }
            
            // 隐藏路径控制点（优化：只处理路径类型）
            if (item->type() == DrawingShape::Path) {
                DrawingPath *path = qgraphicsitem_cast<DrawingPath*>(item);
                if (path) {
                    path->setShowControlPolygon(false);
                }
            }
        }
    }

    // Update tool actions - 通过ToolManager比较工具类型
    if (tool && m_toolManager) {
        if (tool == m_toolManager->getTool(ToolType::Select)) {
            m_outlinePreviewToolAction->setChecked(true);
        }
        else if (tool == m_toolManager->getTool(ToolType::Rectangle)) {
            m_rectangleToolAction->setChecked(true);
        }
        else if (tool == m_toolManager->getTool(ToolType::Ellipse)) {
            m_ellipseToolAction->setChecked(true);
        }
        else if (tool == m_toolManager->getTool(ToolType::Bezier)) {
            m_bezierToolAction->setChecked(true);
        }
        else if (tool == m_toolManager->getTool(ToolType::NodeEdit)) {
            m_nodeEditToolAction->setChecked(true);
        }
    }

    // 更新状态栏（如果存在）
    if (m_statusLabel && m_toolManager) {
        QString toolName = "未知";
        if (tool == m_toolManager->getTool(ToolType::Select)) toolName = "选择";
        else if (tool == m_toolManager->getTool(ToolType::Rectangle)) toolName = "矩形";
        else if (tool == m_toolManager->getTool(ToolType::Ellipse)) toolName = "椭圆";
        else if (tool == m_toolManager->getTool(ToolType::Bezier)) toolName = "贝塞尔";
        else if (tool == m_toolManager->getTool(ToolType::NodeEdit)) toolName = "节点编辑";
        
        m_statusLabel->setText(QString("工具已更改: %1").arg(toolName));
    }
    
    // 初始化UI状态
    updateUI();
}
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
    
    // 设置快捷键管理器的场景引用
    if (m_shortcutManager) {
        m_shortcutManager->setScene(m_scene);
    }
    
    // 清除所有图层
    if (m_layerManager) {
        
        m_layerManager->clearAllLayers();
        // 确保SVG导入标志被重置
        m_layerManager->setSvgImporting(false);
        
        // 重新设置场景以创建默认背景图层
        
        m_layerManager->setScene(m_scene);
        
    }
    updateUI();
    m_statusLabel->setText("新文档已创建");
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "打开文档", m_lastOpenDir, "SVG Files (*.svg);;VectorQt Files (*.vfp)");

    if (!fileName.isEmpty())
    {
        QFileInfo fileInfo(fileName);
        m_lastOpenDir = fileInfo.absolutePath(); // 更新记住的目录
        
        // 清除现有内容
        m_scene->clearScene();
        if (m_layerManager) {
            m_layerManager->clearAllLayers();
            m_layerManager->setScene(m_scene);
        }
        m_currentFile.clear();
        m_isModified = false;
        
        // 重置选择状态
        if (m_scene) {
            m_scene->clearSelection();
        }
        
        // 重置当前工具
        if (m_toolManager) {
            m_toolManager->switchTool(ToolType::Select);
        }
        
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
                                                    "保存文档", m_lastSaveDir, "SVG Files (*.svg)");

    if (!fileName.isEmpty())
    {
        QFileInfo fileInfo(fileName);
        m_lastSaveDir = fileInfo.absolutePath(); // 更新记住的保存目录
        
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
    QString fileName = QFileDialog::getSaveFileName(this, tr("导出文档"), m_lastSaveDir, "SVG Files (*.svg)");

    if (!fileName.isEmpty())
    {
        QFileInfo fileInfo(fileName);
        m_lastSaveDir = fileInfo.absolutePath(); // 更新记住的保存目录
        if (SvgHandler::exportToSvg(m_scene, fileName)) {
            statusBar()->showMessage(tr("文档已导出"), 2000);
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法导出文档"));
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
    if (m_toolManager) m_toolManager->switchTool(ToolType::Select);
}

void MainWindow::rectangleTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Rectangle);
}

void MainWindow::ellipseTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Ellipse);
}

void MainWindow::bezierTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Bezier);
}

void MainWindow::nodeEditTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::NodeEdit);
}

void MainWindow::polylineTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Polyline);
}

void MainWindow::polygonTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Polygon);
}

void MainWindow::brushTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Brush);
}

void MainWindow::fillTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Fill);
}

void MainWindow::gradientFillTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::GradientFill);
}

void MainWindow::penTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Pen);
}

void MainWindow::eraserTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Eraser);
}

void MainWindow::lineTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::Line);
}

void MainWindow::pathEditTool()
{
    if (m_toolManager) m_toolManager->switchTool(ToolType::PathEdit);
}

void MainWindow::textTool()
{
    // 暂时使用选择工具作为文本工具的基础
    selectTool();
    m_statusLabel->setText("文本工具已激活 - 选择文本对象后右键选择'文本转路径'");
}

// updateZoomLabel implementation is below

// 工具面板槽函数实现
void MainWindow::onBrushSizeChanged(int size)
{
    if (m_toolManager) {
        auto brushTool = m_toolManager->getToolAs<DrawingToolBrush>(ToolType::Brush);
        if (brushTool) {
            brushTool->setBrushWidth(size);
        }
    }
}

void MainWindow::onBrushOpacityChanged(int opacity)
{
    // TODO: 实现画笔不透明度设置
    // 目前画笔工具还没有不透明度参数
}

void MainWindow::onBrushSmoothingChanged(int smoothing)
{
    if (m_toolManager) {
        auto brushTool = m_toolManager->getToolAs<DrawingToolBrush>(ToolType::Brush);
        if (brushTool) {
            brushTool->setSmoothness(smoothing / 100.0); // 转换为0-1范围
        }
    }
}

void MainWindow::onEraserSizeChanged(int size)
{
    if (m_toolManager) {
        auto eraserTool = m_toolManager->getToolAs<DrawingToolEraser>(ToolType::Eraser);
        if (eraserTool) {
            eraserTool->setEraserSize(size);
        }
    }
}

void MainWindow::onFillToleranceChanged(int tolerance)
{
    if (m_toolManager) {
        auto fillTool = m_toolManager->getToolAs<DrawingToolFill>(ToolType::Fill);
        if (fillTool) {
            fillTool->setTolerance(tolerance);
        }
    }
}

void MainWindow::onStrokeWidthChanged(double width)
{
    // 应用到钢笔工具
    if (m_toolManager) {
        auto penTool = m_toolManager->getToolAs<DrawingToolPen>(ToolType::Pen);
        if (penTool) {
            penTool->setBrushWidth(width);
        }
    }
    
    // TODO: 也可以应用到其他支持线宽的工具
}

void MainWindow::onAntialiasingChanged(bool enabled)
{
    // TODO: 实现抗锯齿设置
    // 可能需要应用到视图渲染
}

void MainWindow::onSnapToGridChanged(bool enabled)
{
    if (m_scene) {
        m_scene->setGridAlignmentEnabled(enabled);
    }
}

void MainWindow::onPenPressureSupportChanged(bool enabled)
{
    if (m_toolManager) {
        auto penTool = m_toolManager->getToolAs<DrawingToolPen>(ToolType::Pen);
        if (penTool) {
            penTool->togglePressureSupport(enabled);
        }
    }
}

void MainWindow::onPenJoinStyleChanged(int style)
{
    // TODO: 实现钢笔连接样式设置
    // 需要在钢笔工具中添加相应的方法
}

void MainWindow::onPenCapStyleChanged(int style)
{
    // TODO: 实现钢笔端点样式设置
    // 需要在钢笔工具中添加相应的方法
}

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
            
            // 写入变换矩阵
            QTransform transform = shape->transform();
            if (!transform.isIdentity()) {
                jsonData += QString("\"transform\":{\"m11\":%1,\"m12\":%2,\"m21\":%3,\"m22\":%4,\"dx\":%5,\"dy\":%6},")
                            .arg(transform.m11())
                            .arg(transform.m12())
                            .arg(transform.m21())
                            .arg(transform.m22())
                            .arg(transform.dx())
                            .arg(transform.dy());
            } else {
                jsonData += "\"transform\":null,";
            }
            
            // 写入样式数据
            QPen pen = shape->strokePen();
            QBrush brush = shape->fillBrush();
            jsonData += QString("\"stroke\":{\"color\":\"%1\",\"width\":%2,\"style\":%3},")
                        .arg(pen.color().name())
                        .arg(pen.width())
                        .arg((int)pen.style());
            jsonData += QString("\"fill\":{\"color\":\"%1\",\"style\":%2}")
                        .arg(brush.color().name())
                        .arg((int)brush.style());
            
            // 写入滤镜数据
            bool hasFilter = false;
            if (shape->blurEffect()) {
                QGraphicsBlurEffect *blur = shape->blurEffect();
                jsonData += QString(",\"blur\":{\"radius\":%1}").arg(blur->blurRadius());
                hasFilter = true;
            } else if (shape->dropShadowEffect()) {
                QGraphicsDropShadowEffect *shadow = shape->dropShadowEffect();
                jsonData += QString(",\"shadow\":{\"color\":\"%1\",\"blurRadius\":%2,\"offsetX\":%3,\"offsetY\":%4}")
                            .arg(shadow->color().name())
                            .arg(shadow->blurRadius())
                            .arg(shadow->offset().x())
                            .arg(shadow->offset().y());
                hasFilter = true;
            }
            
            // 写入几何数据
            bool hasGeometry = false;
            switch (shape->shapeType()) {
                case DrawingShape::Rectangle: {
                    DrawingRectangle *rect = static_cast<DrawingRectangle*>(shape);
                    jsonData += QString(",\"rect\":{\"x\":%1,\"y\":%2,\"w\":%3,\"h\":%4}")
                                .arg(rect->rectangle().x())
                                .arg(rect->rectangle().y())
                                .arg(rect->rectangle().width())
                                .arg(rect->rectangle().height());
                    hasGeometry = true;
                    break;
                }
                case DrawingShape::Ellipse: {
                    DrawingEllipse *ellipse = static_cast<DrawingEllipse*>(shape);
                    jsonData += QString(",\"ellipse\":{\"x\":%1,\"y\":%2,\"w\":%3,\"h\":%4}")
                                .arg(ellipse->ellipse().x())
                                .arg(ellipse->ellipse().y())
                                .arg(ellipse->ellipse().width())
                                .arg(ellipse->ellipse().height());
                    hasGeometry = true;
                    break;
                }
                case DrawingShape::Line: {
                    DrawingLine *line = static_cast<DrawingLine*>(shape);
                    jsonData += QString(",\"line\":{\"x1\":%1,\"y1\":%2,\"x2\":%3,\"y2\":%4}")
                                .arg(line->line().x1())
                                .arg(line->line().y1())
                                .arg(line->line().x2())
                                .arg(line->line().y2());
                    hasGeometry = true;
                    break;
                }
                case DrawingShape::Path: {
                    DrawingPath *pathShape = dynamic_cast<DrawingPath*>(shape);
                    if (pathShape) {
                        // 使用变换后的路径
                        QPainterPath painterPath = pathShape->transformedShape();
                        QString pathStr;
                        
                        for (int i = 0; i < painterPath.elementCount(); ++i) {
                            QPainterPath::Element element = painterPath.elementAt(i);
                            if (i > 0) pathStr += ";";
                            pathStr += QString("%1,%2,%3").arg(element.type).arg(element.x).arg(element.y);
                        }
                        
                        if (pathStr.isEmpty()) {
                            pathStr = "0,0,0"; // 默认一个MoveTo元素
                        }
                        
                        jsonData += QString(",\"path\":\"%1\"").arg(pathStr);
                        hasGeometry = true;
                    }
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
    
    
    
    // 使用Base64编码确保数据完整性
    QByteArray encodedData = jsonData.toUtf8().toBase64();
    mimeData->setData("application/vectorflow/shapes", encodedData);
    
    // 放到剪贴板 - 剪贴板会接管mimeData的所有权
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setMimeData(mimeData);
    } else {
        delete mimeData; // 如果剪贴板不可用，手动删除
    }
    
    m_statusLabel->setText(QString("已复制 %1 个项目").arg(selected.size()));
}

// 粘贴撤销命令类
class PasteCommand : public QUndoCommand
{
public:
    PasteCommand(DrawingScene *scene, const QList<QGraphicsItem*>& items, QUndoCommand *parent = nullptr)
        : QUndoCommand("粘贴", parent), m_scene(scene), m_items(items)
    {
    }
    
    ~PasteCommand() override
    {
        // 析构时删除所有对象，防止内存泄漏
        for (QGraphicsItem *item : m_items) {
            if (item) {
                delete item;
            }
        }
    }

    void undo() override
    {
        for (QGraphicsItem *item : m_items) {
            if (item && item->scene()) {
                m_scene->removeItem(item);
                // 不要删除对象，只是从场景中移除
            }
        }
        m_scene->setModified(true);
    }

    void redo() override
    {
        for (QGraphicsItem *item : m_items) {
            if (item && !item->scene()) {
                m_scene->addItem(item);
            }
        }
        m_scene->setModified(true);
    }

private:
    DrawingScene *m_scene;
    QList<QGraphicsItem*> m_items;
};

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
    
    // 解码Base64数据
    QByteArray decodedData = QByteArray::fromBase64(copyData);
    QString jsonData = QString::fromUtf8(decodedData);
    
    
    
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
    
    // 存储创建的项目用于撤销命令
    QList<QGraphicsItem*> pastedItems;
    
    // 解析并创建图形
    for (const QString &objStr : objectStrings) {
        // 简单的键值对解析
        QMap<QString, QString> props;
        QString cleanObj = objStr.mid(1, objStr.length() - 2); // 移除 {}
        
        // 更智能的解析，处理嵌套对象和字符串值
        int braceLevel = 0;
        bool inString = false;
        int start = 0;
        
        for (int i = 0; i < cleanObj.length(); ++i) {
            if (cleanObj[i] == '"' && (i == 0 || cleanObj[i-1] != '\\')) {
                inString = !inString;
            } else if (cleanObj[i] == '{') {
                if (!inString) braceLevel++;
            } else if (cleanObj[i] == '}') {
                if (!inString) braceLevel--;
            } else if (cleanObj[i] == ',' && braceLevel == 0 && !inString) {
                // 找到顶级分隔符（不在字符串内）
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
            case DrawingShape::Path: {
                // 解析路径数据
                QString pathStr = props["path"];
                pathStr = pathStr.replace("\"", ""); // 移除引号
                
                if (!pathStr.isEmpty()) {
                    DrawingPath *pathShape = new DrawingPath(nullptr);
                    QPainterPath painterPath;
                    
                    // 解析路径元素
                    QStringList elements = pathStr.split(';');
                    for (const QString &elemStr : elements) {
                        QStringList parts = elemStr.split(',');
                        if (parts.size() >= 3) {
                            int type = parts[0].toInt();
                            double x = parts[1].toDouble();
                            double y = parts[2].toDouble();
                            
                            switch (type) {
                                case 0: // MoveTo
                                    painterPath.moveTo(x, y);
                                    break;
                                case 1: // LineTo
                                    painterPath.lineTo(x, y);
                                    break;
                                case 2: // CurveTo (简化处理)
                                    if (parts.size() >= 5) {
                                        double x2 = parts[3].toDouble();
                                        double y2 = parts[4].toDouble();
                                        painterPath.quadTo(x, y, x2, y2);
                                    }
                                    break;
                                case 3: // CurveToData
                                    // 跳过，由前面的CurveTo处理
                                    break;
                            }
                        }
                    }
                    
                    pathShape->setPath(painterPath);
                    pathShape->setPos(pos + offset);
                    shape = pathShape;
                    } else {
                    // 路径数据为空，跳过创建
                }
                break;
            }
            default:
                continue;
        }
        
        if (shape) {
            // 应用变换矩阵
            QString transformStr = props["transform"];
            if (!transformStr.isEmpty() && transformStr != "null") {
                QString cleanTransform = transformStr.mid(1, transformStr.length() - 2); // 移除 {}
                QStringList transformPairs = cleanTransform.split(',');
                
                if (transformPairs.size() >= 6) {
                    double m11 = transformPairs[0].split(':')[1].toDouble();
                    double m12 = transformPairs[1].split(':')[1].toDouble();
                    double m21 = transformPairs[2].split(':')[1].toDouble();
                    double m22 = transformPairs[3].split(':')[1].toDouble();
                    double dx = transformPairs[4].split(':')[1].toDouble();
                    double dy = transformPairs[5].split(':')[1].toDouble();
                    
                    QTransform transform(m11, m12, m21, m22, dx, dy);
                    shape->applyTransform(transform);
                }
            }
            
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
            
            // 解析滤镜 - 使用直接字符串查找避免解析器问题
            if (cleanObj.contains("\"blur\":")) {
                int blurIndex = cleanObj.indexOf("\"blur\":");
                if (blurIndex >= 0) {
                    int valueStart = cleanObj.indexOf(":", blurIndex) + 1;
                    while (valueStart < cleanObj.length() && cleanObj[valueStart].isSpace()) {
                        valueStart++;
                    }
                    if (valueStart < cleanObj.length() && cleanObj[valueStart] == '{') {
                        valueStart++;
                        int valueEnd = valueStart;
                        while (valueEnd < cleanObj.length() && cleanObj[valueEnd] != '}') {
                            valueEnd++;
                        }
                        QString blurValue = cleanObj.mid(valueStart, valueEnd - valueStart);
                        QStringList blurPairs = blurValue.split(',');
                        if (blurPairs.size() >= 1) {
                            double radius = blurPairs[0].split(':')[1].toDouble();
                            shape->setBlurEffect(radius);
                        }
                    }
                }
            }
            
            if (cleanObj.contains("\"shadow\":")) {
                int shadowIndex = cleanObj.indexOf("\"shadow\":");
                if (shadowIndex >= 0) {
                    int valueStart = cleanObj.indexOf(":", shadowIndex) + 1;
                    while (valueStart < cleanObj.length() && cleanObj[valueStart].isSpace()) {
                        valueStart++;
                    }
                    if (valueStart < cleanObj.length() && cleanObj[valueStart] == '{') {
                        valueStart++;
                        int valueEnd = valueStart;
                        while (valueEnd < cleanObj.length() && cleanObj[valueEnd] != '}') {
                            valueEnd++;
                        }
                        QString shadowValue = cleanObj.mid(valueStart, valueEnd - valueStart);
                        QStringList shadowPairs = shadowValue.split(',');
                        if (shadowPairs.size() >= 4) {
                            QString colorStr = shadowPairs[0].split(':')[1].replace("\"", "");
                            double blurRadius = shadowPairs[1].split(':')[1].toDouble();
                            double offsetX = shadowPairs[2].split(':')[1].toDouble();
                            double offsetY = shadowPairs[3].split(':')[1].toDouble();
                            
                            shape->setDropShadowEffect(QColor(colorStr), blurRadius, QPointF(offsetX, offsetY));
                        }
                    }
                }
            }
            
            // 添加到粘贴项目列表，而不是直接添加到场景
            if (shape) {
                pastedItems.append(shape);
                shape->setSelected(true);
            }
        }
    }
    
    // 创建撤销命令并执行
    if (!pastedItems.isEmpty()) {
        PasteCommand *command = new PasteCommand(m_scene, pastedItems);
        m_scene->undoStack()->push(command);
        m_statusLabel->setText(QString("已粘贴 %1 个项目").arg(pastedItems.size()));
    }
}

void MainWindow::duplicate()
{
    // 快速复制粘贴：先复制，然后立即粘贴
    copySelected();
    paste();
}

void MainWindow::applyBlurEffect()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty())
        return;

    // 使用默认模糊半径
    qreal radius = 5.0;

    // 创建撤销命令
    class FilterChangeCommand : public QUndoCommand {
    public:
        FilterChangeCommand(DrawingScene *scene, const QList<QGraphicsItem*>& items, 
                          qreal radius, QUndoCommand *parent = nullptr)
            : QUndoCommand("应用高斯模糊", parent), m_scene(scene), m_items(items), m_radius(radius)
        {
            // 保存原始状态
            for (QGraphicsItem *item : items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    QGraphicsEffect *effect = shape->graphicsEffect();
                    if (QGraphicsBlurEffect *blur = qobject_cast<QGraphicsBlurEffect*>(effect)) {
                        QGraphicsBlurEffect *newBlur = new QGraphicsBlurEffect();
                        newBlur->setBlurRadius(blur->blurRadius());
                        m_originalEffects[item] = newBlur;
                    } else if (QGraphicsDropShadowEffect *shadow = qobject_cast<QGraphicsDropShadowEffect*>(effect)) {
                        QGraphicsDropShadowEffect *newShadow = new QGraphicsDropShadowEffect();
                        newShadow->setColor(shadow->color());
                        newShadow->setBlurRadius(shadow->blurRadius());
                        newShadow->setOffset(shadow->offset());
                        m_originalEffects[item] = newShadow;
                    } else {
                        m_originalEffects[item] = nullptr;
                    }
                }
            }
        }

        void undo() override {
            for (QGraphicsItem *item : m_items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape && m_originalEffects.contains(item)) {
                    shape->setGraphicsEffect(m_originalEffects[item]);
                }
            }
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            for (QGraphicsItem *item : m_items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    shape->setBlurEffect(m_radius);
                }
            }
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        QList<QGraphicsItem*> m_items;
        qreal m_radius;
        QHash<QGraphicsItem*, QGraphicsEffect*> m_originalEffects;
    };

    FilterChangeCommand *command = new FilterChangeCommand(m_scene, selected, radius);
    m_scene->undoStack()->push(command);
    m_statusLabel->setText(QString("已应用高斯模糊到 %1 个对象").arg(selected.size()));
}

void MainWindow::applyDropShadowEffect()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty())
        return;

    // 使用默认阴影参数
    QColor color = Qt::black;
    qreal blurRadius = 3.0;
    qreal offsetX = 3.0;
    qreal offsetY = 3.0;

    // 创建撤销命令
    class ShadowFilterCommand : public QUndoCommand {
    public:
        ShadowFilterCommand(DrawingScene *scene, const QList<QGraphicsItem*>& items, 
                           const QColor &color, qreal blurRadius, const QPointF &offset,
                           QUndoCommand *parent = nullptr)
            : QUndoCommand("应用阴影效果", parent), m_scene(scene), m_items(items), 
              m_color(color), m_blurRadius(blurRadius), m_offset(offset)
        {
            // 保存原始状态
            for (QGraphicsItem *item : items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    QGraphicsEffect *effect = shape->graphicsEffect();
                    if (QGraphicsBlurEffect *blur = qobject_cast<QGraphicsBlurEffect*>(effect)) {
                        QGraphicsBlurEffect *newBlur = new QGraphicsBlurEffect();
                        newBlur->setBlurRadius(blur->blurRadius());
                        m_originalEffects[item] = newBlur;
                    } else if (QGraphicsDropShadowEffect *shadow = qobject_cast<QGraphicsDropShadowEffect*>(effect)) {
                        QGraphicsDropShadowEffect *newShadow = new QGraphicsDropShadowEffect();
                        newShadow->setColor(shadow->color());
                        newShadow->setBlurRadius(shadow->blurRadius());
                        newShadow->setOffset(shadow->offset());
                        m_originalEffects[item] = newShadow;
                    } else {
                        m_originalEffects[item] = nullptr;
                    }
                }
            }
        }

        void undo() override {
            for (QGraphicsItem *item : m_items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape && m_originalEffects.contains(item)) {
                    shape->setGraphicsEffect(m_originalEffects[item]);
                }
            }
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            for (QGraphicsItem *item : m_items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    shape->setDropShadowEffect(m_color, m_blurRadius, m_offset);
                }
            }
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        QList<QGraphicsItem*> m_items;
        QColor m_color;
        qreal m_blurRadius;
        QPointF m_offset;
        QHash<QGraphicsItem*, QGraphicsEffect*> m_originalEffects;
    };

    ShadowFilterCommand *command = new ShadowFilterCommand(m_scene, selected, color, blurRadius, QPointF(offsetX, offsetY));
    m_scene->undoStack()->push(command);
    m_statusLabel->setText(QString("已应用阴影效果到 %1 个对象").arg(selected.size()));
}

void MainWindow::clearFilterEffect()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty())
        return;

    // 创建撤销命令
    class ClearFilterCommand : public QUndoCommand {
    public:
        ClearFilterCommand(DrawingScene *scene, const QList<QGraphicsItem*>& items, QUndoCommand *parent = nullptr)
            : QUndoCommand("清除滤镜", parent), m_scene(scene), m_items(items)
        {
            // 保存原始状态
            for (QGraphicsItem *item : items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    QGraphicsEffect *effect = shape->graphicsEffect();
                    if (QGraphicsBlurEffect *blur = qobject_cast<QGraphicsBlurEffect*>(effect)) {
                        QGraphicsBlurEffect *newBlur = new QGraphicsBlurEffect();
                        newBlur->setBlurRadius(blur->blurRadius());
                        m_originalEffects[item] = newBlur;
                    } else if (QGraphicsDropShadowEffect *shadow = qobject_cast<QGraphicsDropShadowEffect*>(effect)) {
                        QGraphicsDropShadowEffect *newShadow = new QGraphicsDropShadowEffect();
                        newShadow->setColor(shadow->color());
                        newShadow->setBlurRadius(shadow->blurRadius());
                        newShadow->setOffset(shadow->offset());
                        m_originalEffects[item] = newShadow;
                    } else {
                        m_originalEffects[item] = nullptr;
                    }
                }
            }
        }

        void undo() override {
            for (QGraphicsItem *item : m_items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape && m_originalEffects.contains(item)) {
                    shape->setGraphicsEffect(m_originalEffects[item]);
                }
            }
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            for (QGraphicsItem *item : m_items) {
                DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    shape->clearFilter();
                }
            }
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        QList<QGraphicsItem*> m_items;
        QHash<QGraphicsItem*, QGraphicsEffect*> m_originalEffects;
    };

    ClearFilterCommand *command = new ClearFilterCommand(m_scene, selected);
    m_scene->undoStack()->push(command);
    m_statusLabel->setText(QString("已清除 %1 个对象的滤镜效果").arg(selected.size()));
}

void MainWindow::convertTextToPath()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty())
        return;

    QList<DrawingShape*> convertedShapes;
    
    foreach (QGraphicsItem *item, selected) {
        DrawingText *textShape = qgraphicsitem_cast<DrawingText*>(item);
        if (textShape) {
            // 将文本转换为路径
            DrawingPath *pathShape = textShape->convertToPath();
            if (pathShape) {
                // 添加到场景
                m_scene->addItem(pathShape);
                pathShape->setSelected(true);
                convertedShapes.append(pathShape);
                
                // 移除原始文本
                m_scene->removeItem(textShape);
                delete textShape;
            }
        }
    }
    
    if (!convertedShapes.isEmpty()) {
        m_scene->setModified(true);
        m_statusLabel->setText(QString("已将 %1 个文本转换为路径").arg(convertedShapes.size()));
    } else {
        m_statusLabel->setText("没有选中的文本对象");
    }
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
    
    if (selected.size() < 2) {
        // 如果没有选中足够多的项目，给出提示
        m_statusLabel->setText("需要至少选择2个项目才能组合");
        return;
    }
    
    // 使用场景的组合方法，这会创建撤销命令
    m_scene->groupSelectedItems();
    
    // 更新状态标签
    m_statusLabel->setText(QString("已组合 %1 个项目").arg(selected.size()));
}

void MainWindow::ungroupSelected()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    
    if (selected.isEmpty()) {
        m_statusLabel->setText("没有选中的项目");
        return;
    }
    
    // 计算要取消组合的数量
    int groupCount = 0;
    for (QGraphicsItem *item : selected) {
        if (item && item->type() == QGraphicsItem::UserType + 1) {
            DrawingShape *shape = static_cast<DrawingShape*>(item);
            if (shape && shape->shapeType() == DrawingShape::Group) {
                groupCount++;
            }
        }
    }
    
    if (groupCount == 0) {
        m_statusLabel->setText("没有选中的组合项目");
        return;
    }
    
    // 使用场景的取消组合方法，这会创建撤销命令
    m_scene->ungroupSelectedItems();
    
    // 更新状态标签
    m_statusLabel->setText(QString("已取消组合 %1 个组").arg(groupCount));
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
    if (m_scene) {
        int selectedCount = m_scene->selectedItems().count();
        
        
        // 如果有选中的图形，提示用户可以使用空格键切换到选择工具
        if (selectedCount > 0) {
            updateStatusBar("选中图形 - 按空格键切换到选择工具");
        } else {
            updateStatusBar("就绪");
        }
    }
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

void MainWindow::onObjectStateChanged(DrawingShape* shape)
{
    // 当图形对象状态发生变化时，更新标尺显示
    updateRulerSelection();
}

void MainWindow::onSceneChanged()
{
    m_isModified = true;
    updateUI(); // 更新窗口标题显示修改状态
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // 可以在这里处理鼠标移动事件
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 让快捷键管理器处理所有快捷键
    if (m_shortcutManager) {
        QKeySequence key(event->key() | (event->modifiers() & Qt::KeyboardModifierMask));
        if (m_shortcutManager->handleKeyPress(key)) {
            return; // 快捷键管理器已处理
        }
    }
    
    // 如果快捷键管理器没有处理，传递给基类
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // 让快捷键管理器处理按键释放
    if (m_shortcutManager) {
        QKeySequence key(event->key() | (event->modifiers() & Qt::KeyboardModifierMask));
        if (m_shortcutManager->handleKeyRelease(key)) {
            return; // 快捷键管理器已处理
        }
    }
    
    QMainWindow::keyReleaseEvent(event);
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

void MainWindow::bringToFront()
{
    if (m_scene) {
        m_scene->bringToFront();
        m_statusLabel->setText("置于顶层");
    }
}

void MainWindow::sendToBack()
{
    if (m_scene) {
        m_scene->sendToBack();
        m_statusLabel->setText("置于底层");
    }
}

void MainWindow::bringForward()
{
    if (m_scene) {
        m_scene->bringForward();
        m_statusLabel->setText("上移一层");
    }
}

void MainWindow::sendBackward()
{
    if (m_scene) {
        m_scene->sendBackward();
        m_statusLabel->setText("下移一层");
    }
}

void MainWindow::showContextMenu(const QPointF &pos)
{
    QMenu contextMenu(this);
    
    // 获取选中项数量用于条件判断
    int selectedCount = m_scene ? m_scene->selectedItems().size() : 0;
    
    // 添加基本操作
    contextMenu.addAction(m_copyAction);
    contextMenu.addAction(m_pasteAction);
    contextMenu.addAction(m_duplicateAction);
    contextMenu.addSeparator();
    
    // 添加滤镜子菜单
    QMenu *filterContextMenu = contextMenu.addMenu("滤镜");
    filterContextMenu->addAction(m_blurEffectAction);
    filterContextMenu->addAction(m_dropShadowEffectAction);
    filterContextMenu->addSeparator();
    filterContextMenu->addAction(m_clearFilterAction);
    contextMenu.addSeparator();
    
    contextMenu.addAction(m_deleteAction);
    contextMenu.addSeparator();
    
    // 添加Z序控制子菜单
    QMenu *zOrderMenu = contextMenu.addMenu("Z序");
    zOrderMenu->addAction(m_bringToFrontAction);
    zOrderMenu->addAction(m_sendToBackAction);
    zOrderMenu->addSeparator();
    zOrderMenu->addAction(m_bringForwardAction);
    zOrderMenu->addAction(m_sendBackwardAction);
    
    // 添加对齐子菜单
    QMenu *alignMenu = contextMenu.addMenu("对齐");
    alignMenu->addAction(m_alignLeftAction);
    alignMenu->addAction(m_alignCenterAction);
    alignMenu->addAction(m_alignRightAction);
    alignMenu->addSeparator();
    alignMenu->addAction(m_alignTopAction);
    alignMenu->addAction(m_alignMiddleAction);
    alignMenu->addAction(m_alignBottomAction);
    alignMenu->addSeparator();
    alignMenu->addAction(m_sameWidthAction);
    alignMenu->addAction(m_sameHeightAction);
    alignMenu->addAction(m_sameSizeAction);
    alignMenu->addSeparator();
    alignMenu->addAction(m_distributeHorizontalAction);
    alignMenu->addAction(m_distributeVerticalAction);
    
    // 根据选中数量启用/禁用对齐功能
    bool canAlign = selectedCount >= 1;
    bool canDistribute = selectedCount >= 3;
    m_alignLeftAction->setEnabled(canAlign);
    m_alignCenterAction->setEnabled(canAlign);
    m_alignRightAction->setEnabled(canAlign);
    m_alignTopAction->setEnabled(canAlign);
    m_alignMiddleAction->setEnabled(canAlign);
    m_alignBottomAction->setEnabled(canAlign);
    m_distributeHorizontalAction->setEnabled(canDistribute);
    m_distributeVerticalAction->setEnabled(canDistribute);
    
    // 添加组合操作
    contextMenu.addAction(m_groupAction);
    contextMenu.addAction(m_ungroupAction);
    m_groupAction->setEnabled(selectedCount >= 2);  // 需要至少2个对象才能组合
    
    contextMenu.addSeparator();
    
    // 添加路径编辑菜单
    QMenu *pathMenu = contextMenu.addMenu("路径编辑");
    
    // 路径编辑功能需要至少1个选中对象
    bool canEditPath = selectedCount >= 1;
    
    // 布尔运算需要至少2个选中对象
    bool canBoolean = selectedCount >= 2;
    
    // 布尔运算菜单
    QMenu *booleanMenu = pathMenu->addMenu("布尔运算");
    
    QAction *unionAction = booleanMenu->addAction("合并");
    unionAction->setEnabled(canBoolean);
    QAction *intersectAction = booleanMenu->addAction("相交");
    intersectAction->setEnabled(canBoolean);
    QAction *subtractAction = booleanMenu->addAction("减去");
    subtractAction->setEnabled(canBoolean);
    QAction *xorAction = booleanMenu->addAction("异或");
    xorAction->setEnabled(canBoolean);
    
    // 路径操作菜单
    QMenu *pathOpsMenu = pathMenu->addMenu("路径操作");
    
    QAction *simplifyAction = pathOpsMenu->addAction("简化路径");
    simplifyAction->setEnabled(canEditPath);
    QAction *smoothAction = pathOpsMenu->addAction("平滑路径");
    smoothAction->setEnabled(canEditPath);
    QAction *curveAction = pathOpsMenu->addAction("转换为曲线");
    curveAction->setEnabled(canEditPath);
    QAction *offsetAction = pathOpsMenu->addAction("偏移路径");
    offsetAction->setEnabled(canEditPath);
    QAction *clipAction = pathOpsMenu->addAction("裁剪路径");
    clipAction->setEnabled(canEditPath);
    
    // 创建形状菜单
    QMenu *shapeMenu = pathMenu->addMenu("创建形状");
    
    QAction *arrowAction = shapeMenu->addAction("箭头");
    QAction *starAction = shapeMenu->addAction("星形");
    QAction *gearAction = shapeMenu->addAction("齿轮");
    
    // 检查是否有选中的文本对象
    bool hasTextSelection = false;
    if (m_scene) {
        QList<QGraphicsItem *> selected = m_scene->selectedItems();
        for (QGraphicsItem *item : selected) {
            DrawingText *textShape = qgraphicsitem_cast<DrawingText*>(item);
            if (textShape) {
                hasTextSelection = true;
                break;
            }
        }
    }
    
    // 如果有选中的文本，添加文本转路径选项
    QAction *convertTextToPathAction = nullptr;
    if (hasTextSelection) {
        pathMenu->addSeparator();
        convertTextToPathAction = pathMenu->addAction("文本转路径");
    }
    
    // 显示菜单
    QAction *selectedAction = contextMenu.exec(m_canvas->view()->mapToGlobal(m_canvas->view()->mapFromScene(pos)));
    
    // 处理菜单选择
    if (selectedAction == unionAction) {
        executeBooleanOperation(static_cast<int>(PathEditor::Union));
    } else if (selectedAction == intersectAction) {
        executeBooleanOperation(static_cast<int>(PathEditor::Intersection));
    } else if (selectedAction == subtractAction) {
        executeBooleanOperation(static_cast<int>(PathEditor::Subtraction));
    } else if (selectedAction == xorAction) {
        executeBooleanOperation(static_cast<int>(PathEditor::Xor));
    } else if (selectedAction == simplifyAction) {
        executePathOperation("simplify");
    } else if (selectedAction == smoothAction) {
        executePathOperation("smooth");
    } else if (selectedAction == curveAction) {
        executePathOperation("curve");
    } else if (selectedAction == offsetAction) {
        executePathOperation("offset");
    } else if (selectedAction == clipAction) {
        executePathOperation("clip");
    } else if (selectedAction == arrowAction) {
        createShapeAtPosition("arrow", pos);
    } else if (selectedAction == starAction) {
        createShapeAtPosition("star", pos);
    } else if (selectedAction == gearAction) {
        createShapeAtPosition("gear", pos);
    } else if (selectedAction == convertTextToPathAction) {
        convertSelectedTextToPath();
    }
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
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        leftmost = qMin(leftmost, bounds.left());
    }
    
    // 将所有项目对齐到最左边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
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
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        leftmost = qMin(leftmost, bounds.left());
        rightmost = qMax(rightmost, bounds.right());
    }
    
    qreal centerX = (leftmost + rightmost) / 2.0;
    
    // 将所有项目水平居中对齐
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
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
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        rightmost = qMax(rightmost, bounds.right());
    }
    
    // 将所有项目对齐到最右边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
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
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        topmost = qMin(topmost, bounds.top());
    }
    
    // 将所有项目对齐到最顶边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
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
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        topmost = qMin(topmost, bounds.top());
        bottommost = qMax(bottommost, bounds.bottom());
    }
    
    qreal centerY = (topmost + bottommost) / 2.0;
    
    // 将所有项目垂直居中对齐
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
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
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        bottommost = qMax(bottommost, bounds.bottom());
    }
    
    // 将所有项目对齐到最底边界
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        qreal deltaY = bottommost - bounds.bottom();
        item->setPos(item->pos().x(), item->pos().y() + deltaY);
    }
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已底部对齐 %1 个项目").arg(selectedItems.size()));
}

void MainWindow::sameWidth()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 2) {
        m_statusLabel->setText("需要至少选择2个项目");
        return;
    }
    
    // 找到第一个选中项目的宽度作为目标宽度
    QGraphicsItem* firstItem = selectedItems.first();
    QRectF firstBounds = firstItem->mapToScene(firstItem->boundingRect()).boundingRect();
    qreal targetWidth = firstBounds.width();
    
    // 创建撤销命令
    class SameWidthCommand : public QUndoCommand
    {
    public:
        SameWidthCommand(DrawingScene *scene, const QList<QGraphicsItem*>& items, qreal width, QUndoCommand *parent = nullptr)
            : QUndoCommand("同宽", parent), m_scene(scene), m_items(items), m_targetWidth(width)
        {
            // 保存原始变换
            for (QGraphicsItem* item : m_items) {
                m_originalTransforms.append(item->transform());
            }
        }
        
        void undo() override {
            for (int i = 0; i < m_items.size(); ++i) {
                m_items[i]->setTransform(m_originalTransforms[i]);
            }
            m_scene->update();
            m_scene->setModified(true);
        }
        
        void redo() override {
            for (QGraphicsItem* item : m_items) {
                QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
                qreal currentWidth = bounds.width();
                qreal scaleX = m_targetWidth / currentWidth;
                
                // 检查是否是 DrawingShape，使用专门的方法
                DrawingShape* shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    // 重置变换然后应用新的缩放
                    shape->setTransform(QTransform());
                    shape->scaleAroundAnchor(scaleX, 1.0, shape->boundingRect().center());
                } else {
                    // 对于其他类型的项目，使用通用方法
                    QTransform transform;
                    transform.scale(scaleX, 1.0);
                    item->setTransform(transform);
                }
            }
            m_scene->update();
            m_scene->setModified(true);
        }
        
    private:
        DrawingScene *m_scene;
        QList<QGraphicsItem*> m_items;
        qreal m_targetWidth;
        QVector<QTransform> m_originalTransforms;
    };
    
    // 应用同宽操作
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        qreal currentWidth = bounds.width();
        qreal scaleX = targetWidth / currentWidth;
        
        // 检查是否是 DrawingShape，使用专门的方法
        DrawingShape* shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            // 重置变换然后应用新的缩放
            shape->setTransform(QTransform());
            shape->scaleAroundAnchor(scaleX, 1.0, shape->boundingRect().center());
        } else {
            // 对于其他类型的项目，使用通用方法
            QTransform transform;
            transform.scale(scaleX, 1.0);
            item->setTransform(transform);
        }
    }
    
    // 创建并推送撤销命令
    SameWidthCommand *command = new SameWidthCommand(m_scene, selectedItems, targetWidth);
    m_scene->undoStack()->push(command);
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已设置 %1 个项目为相同宽度").arg(selectedItems.size()));
}

void MainWindow::sameHeight()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 2) {
        m_statusLabel->setText("需要至少选择2个项目");
        return;
    }
    
    // 找到第一个选中项目的高度作为目标高度
    QGraphicsItem* firstItem = selectedItems.first();
    QRectF firstBounds = firstItem->mapToScene(firstItem->boundingRect()).boundingRect();
    qreal targetHeight = firstBounds.height();
    
    // 创建撤销命令
    class SameHeightCommand : public QUndoCommand
    {
    public:
        SameHeightCommand(DrawingScene *scene, const QList<QGraphicsItem*>& items, qreal height, QUndoCommand *parent = nullptr)
            : QUndoCommand("同高", parent), m_scene(scene), m_items(items), m_targetHeight(height)
        {
            // 保存原始变换
            for (QGraphicsItem* item : m_items) {
                m_originalTransforms.append(item->transform());
            }
        }
        
        void undo() override {
            for (int i = 0; i < m_items.size(); ++i) {
                m_items[i]->setTransform(m_originalTransforms[i]);
            }
            m_scene->update();
            m_scene->setModified(true);
        }
        
        void redo() override {
            for (QGraphicsItem* item : m_items) {
                QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
                qreal currentHeight = bounds.height();
                qreal scaleY = m_targetHeight / currentHeight;
                
                // 检查是否是 DrawingShape，使用专门的方法
                DrawingShape* shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    // 重置变换然后应用新的缩放
                    shape->setTransform(QTransform());
                    shape->scaleAroundAnchor(1.0, scaleY, shape->boundingRect().center());
                } else {
                    // 对于其他类型的项目，使用通用方法
                    QTransform transform;
                    transform.scale(1.0, scaleY);
                    item->setTransform(transform);
                }
            }
            m_scene->update();
            m_scene->setModified(true);
        }
        
    private:
        DrawingScene *m_scene;
        QList<QGraphicsItem*> m_items;
        qreal m_targetHeight;
        QVector<QTransform> m_originalTransforms;
    };
    
    // 应用同高操作
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        qreal currentHeight = bounds.height();
        qreal scaleY = targetHeight / currentHeight;
        
        // 检查是否是 DrawingShape，使用专门的方法
        DrawingShape* shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            // 重置变换然后应用新的缩放
            shape->setTransform(QTransform());
            shape->scaleAroundAnchor(1.0, scaleY, shape->boundingRect().center());
        } else {
            // 对于其他类型的项目，使用通用方法
            QTransform transform;
            transform.scale(1.0, scaleY);
            item->setTransform(transform);
        }
    }
    
    // 创建并推送撤销命令
    SameHeightCommand *command = new SameHeightCommand(m_scene, selectedItems, targetHeight);
    m_scene->undoStack()->push(command);
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已设置 %1 个项目为相同高度").arg(selectedItems.size()));
}

void MainWindow::sameSize()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 2) {
        m_statusLabel->setText("需要至少选择2个项目");
        return;
    }
    
    // 找到第一个选中项目的尺寸作为目标尺寸
    QGraphicsItem* firstItem = selectedItems.first();
    QRectF firstBounds = firstItem->mapToScene(firstItem->boundingRect()).boundingRect();
    qreal targetWidth = firstBounds.width();
    qreal targetHeight = firstBounds.height();
    
    // 创建撤销命令
    class SameSizeCommand : public QUndoCommand
    {
    public:
        SameSizeCommand(DrawingScene *scene, const QList<QGraphicsItem*>& items, qreal width, qreal height, QUndoCommand *parent = nullptr)
            : QUndoCommand("同大小", parent), m_scene(scene), m_items(items), m_targetWidth(width), m_targetHeight(height)
        {
            // 保存原始变换
            for (QGraphicsItem* item : m_items) {
                m_originalTransforms.append(item->transform());
            }
        }
        
        void undo() override {
            for (int i = 0; i < m_items.size(); ++i) {
                m_items[i]->setTransform(m_originalTransforms[i]);
            }
            m_scene->update();
            m_scene->setModified(true);
        }
        
        void redo() override {
            for (QGraphicsItem* item : m_items) {
                QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
                qreal currentWidth = bounds.width();
                qreal currentHeight = bounds.height();
                qreal scaleX = m_targetWidth / currentWidth;
                qreal scaleY = m_targetHeight / currentHeight;
                
                // 检查是否是 DrawingShape，使用专门的方法
                DrawingShape* shape = qgraphicsitem_cast<DrawingShape*>(item);
                if (shape) {
                    // 重置变换然后应用新的缩放
                    shape->setTransform(QTransform());
                    shape->scaleAroundAnchor(scaleX, scaleY, shape->boundingRect().center());
                } else {
                    // 对于其他类型的项目，使用通用方法
                    QTransform transform;
                    transform.scale(scaleX, scaleY);
                    item->setTransform(transform);
                }
            }
            m_scene->update();
            m_scene->setModified(true);
        }
        
    private:
        DrawingScene *m_scene;
        QList<QGraphicsItem*> m_items;
        qreal m_targetWidth;
        qreal m_targetHeight;
        QVector<QTransform> m_originalTransforms;
    };
    
    // 应用同大小操作
    for (QGraphicsItem* item : selectedItems) {
        QRectF bounds = item->mapToScene(item->boundingRect()).boundingRect();
        qreal currentWidth = bounds.width();
        qreal currentHeight = bounds.height();
        qreal scaleX = targetWidth / currentWidth;
        qreal scaleY = targetHeight / currentHeight;
        
        // 检查是否是 DrawingShape，使用专门的方法
        DrawingShape* shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            // 重置变换然后应用新的缩放
            shape->setTransform(QTransform());
            shape->scaleAroundAnchor(scaleX, scaleY, shape->boundingRect().center());
        } else {
            // 对于其他类型的项目，使用通用方法
            QTransform transform;
            transform.scale(scaleX, scaleY);
            item->setTransform(transform);
        }
    }
    
    // 创建并推送撤销命令
    SameSizeCommand *command = new SameSizeCommand(m_scene, selectedItems, targetWidth, targetHeight);
    m_scene->undoStack()->push(command);
    
    m_scene->update();
    m_scene->setModified(true);
    m_statusLabel->setText(QString("已设置 %1 个项目为相同大小").arg(selectedItems.size()));
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

// 路径编辑槽函数
void MainWindow::pathSimplify()
{
    executePathOperation("simplify");
}

void MainWindow::pathSmooth()
{
    executePathOperation("smooth");
}

void MainWindow::pathReverse()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("请先选择要反转的路径");
        return;
    }
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape && shape->shapeType() == DrawingShape::Path) {
            DrawingPath *drawingPath = dynamic_cast<DrawingPath*>(shape);
            if (drawingPath) {
                QPainterPath originalPath = drawingPath->path();
                QPainterPath reversedPath;
                
                // 反转路径：创建一个新路径，按相反顺序添加元素
                for (int i = originalPath.elementCount() - 1; i >= 0; --i) {
                    QPainterPath::Element element = originalPath.elementAt(i);
                    if (element.isMoveTo()) {
                        reversedPath.moveTo(element.x, element.y);
                    } else if (element.isLineTo()) {
                        reversedPath.lineTo(element.x, element.y);
                    } else if (element.isCurveTo()) {
                        reversedPath.cubicTo(element.x, element.y, element.x, element.y, element.x, element.y);
                    } else if (element.type == QPainterPath::CurveToDataElement) {
                        reversedPath.cubicTo(element.x, element.y, element.x, element.y, element.x, element.y);
                    }
                }
                
                drawingPath->setPath(reversedPath);
                m_statusLabel->setText("路径已反转");
            }
        }
    }
    
    m_scene->update();
    m_scene->setModified(true);
}

void MainWindow::generateShape()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("请先选择要生成图形的路径");
        return;
    }
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape && shape->shapeType() == DrawingShape::Path) {
            DrawingPath *drawingPath = dynamic_cast<DrawingPath*>(shape);
            if (drawingPath) {
                QPainterPath originalPath = drawingPath->path();
                
                // 尝试生成标准图形：星形
                QPointF center = originalPath.boundingRect().center();
                qreal radius = qMax(originalPath.boundingRect().width(), originalPath.boundingRect().height()) / 2;
                QPainterPath starPath = PathEditor::createStar(center, radius, 5);
                
                drawingPath->setPath(starPath);
                m_statusLabel->setText("已生成星形");
            }
        }
    }
    
    m_scene->update();
    m_scene->setModified(true);
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
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape) {
            // 获取图形的路径
            QPainterPath shapePath;
            
            // 根据图形类型获取路径
            if (shape->shapeType() == DrawingShape::Path) {
                DrawingPath *drawingPath = dynamic_cast<DrawingPath*>(shape);
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

// 执行布尔运算
void MainWindow::executeBooleanOperation(int op)
{
    if (!m_scene) return;
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.size() < 2) {
        m_statusLabel->setText("需要选择至少两个路径进行布尔运算");
        return;
    }
    
    // 获取前两个选中的形状
    DrawingShape *shape1 = qgraphicsitem_cast<DrawingShape*>(selected[0]);
    DrawingShape *shape2 = qgraphicsitem_cast<DrawingShape*>(selected[1]);
    
    if (!shape1 || !shape2) {
        m_statusLabel->setText("选择的对象不是有效的路径");
        return;
    }
    
    // 执行布尔运算
    QPainterPath result;
    try {
        // 获取形状的实际位置
        QPointF pos1 = shape1->pos();
        QPointF pos2 = shape2->pos();
        
        // 获取基础路径（不包含位置）
        QPainterPath path1Base = shape1->transformedShape();
        QPainterPath path2Base = shape2->transformedShape();
        
        // 创建包含位置信息的路径
        QPainterPath path1WithPos;
        QPainterPath path2WithPos;
        
        // 将路径平移到正确的位置
        QTransform transform1;
        transform1.translate(pos1.x(), pos1.y());
        path1WithPos = transform1.map(path1Base);
        
        QTransform transform2;
        transform2.translate(pos2.x(), pos2.y());
        path2WithPos = transform2.map(path2Base);
        
        result = PathEditor::booleanOperation(
            path1WithPos, 
            path2WithPos, 
            static_cast<PathEditor::BooleanOperation>(op)
        );
    } catch (...) {
        m_statusLabel->setText("布尔运算异常");
        return;
    }
    
    // 检查结果是否为空
    if (result.isEmpty()) {
        m_statusLabel->setText("布尔运算结果为空");
        return;
    }
    
    // 创建新的路径对象
    DrawingPath *newPath = new DrawingPath();
    
    // 计算结果路径的边界框
    QRectF resultBounds = result.boundingRect();
    
    // 布尔运算的结果已经包含了正确的位置信息
    // 我们需要将结果路径转换为相对于图形原点的路径
    // 计算结果路径的实际位置（使用第一个路径的位置作为参考）
    QPointF resultPos = shape1->pos();
    
    // 创建一个变换，将路径移动到相对于图形原点的位置
    QTransform offsetTransform;
    offsetTransform.translate(-resultBounds.left(), -resultBounds.top());
    QPainterPath adjustedPath = offsetTransform.map(result);
    
    // 设置调整后的路径
    newPath->setPath(adjustedPath);
    
    // 设置新路径的位置为第一个形状的位置
    newPath->setPos(shape1->pos());
    
    // 复制样式
    newPath->setStrokePen(shape1->strokePen());
    newPath->setFillBrush(shape1->fillBrush());
    
    // 从场景中移除原始形状，并从图层中移除
    // 获取形状所属的图层
    DrawingLayer *layer1 = nullptr;
    DrawingLayer *layer2 = nullptr;
    
    // 查找形状所属的图层
    LayerManager *layerManager = LayerManager::instance();
    for (int i = 0; i < layerManager->layerCount(); ++i) {
        DrawingLayer *layer = layerManager->layer(i);
        if (layer->shapes().contains(shape1)) {
            layer1 = layer;
        }
        if (layer->shapes().contains(shape2)) {
            layer2 = layer;
        }
    }
    
    // 从场景中移除原始形状
    m_scene->removeItem(shape1);
    m_scene->removeItem(shape2);
    
    // 从图层中移除原始形状
    if (layer1) {
        layer1->removeShape(shape1);
    }
    if (layer2 && layer2 != layer1) {
        layer2->removeShape(shape2);
    }
    
    // 添加新路径到场景
    m_scene->addItem(newPath);
    newPath->setSelected(true);
    
    // 将新路径添加到第一个形状的图层中
    if (layer1) {
        layer1->addShape(newPath);
    }
    
    // 更新图层面板显示
    LayerManager::instance()->updateLayerPanel();
    
    // 删除原始形状
    delete shape1;
    delete shape2;
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    QString opName;
    switch (static_cast<PathEditor::BooleanOperation>(op)) {
        case PathEditor::Union: opName = "合并"; break;
        case PathEditor::Intersection: opName = "相交"; break;
        case PathEditor::Subtraction: opName = "减去"; break;
        case PathEditor::Xor: opName = "异或"; break;
        default: opName = "布尔运算"; break;
    }
    
    m_statusLabel->setText(QString("%1操作完成").arg(opName));
}

// 路径操作撤销命令
class PathOperationCommand : public QUndoCommand
{
public:
    PathOperationCommand(DrawingScene *scene, DrawingShape *originalShape, DrawingPath *newPath, const QString &operationText, QUndoCommand *parent = nullptr)
        : QUndoCommand(operationText, parent)
        , m_scene(scene)
        , m_originalShape(originalShape)
        , m_newPath(newPath)
        , m_originalShapeInScene(false)
        , m_newPathInScene(false)
        , m_originalLayer(nullptr)
        , m_targetLayer(nullptr)
    {
        // 记录对象初始状态
        if (m_originalShape) {
            m_originalShapeInScene = (m_originalShape->scene() != nullptr);
        }
        if (m_newPath) {
            m_newPathInScene = (m_newPath->scene() != nullptr);
        }
        
        // 记录图形所属的图层
        LayerManager *layerManager = LayerManager::instance();
        if (layerManager) {
            m_originalLayer = layerManager->findLayerForShape(m_originalShape);
            // 新路径应该添加到活动图层
            m_targetLayer = layerManager->activeLayer();
        }
    }
    
    ~PathOperationCommand()
    {
        // 安全删除对象，只在确定不需要时删除
        if (m_newPath && !m_newPath->scene()) {
            delete m_newPath;
        }
        // 注意：不在这里删除原始形状，因为它们可能被其他命令引用
    }
    
    void undo() override
    {
        if (!m_scene) return;
        
        LayerManager *layerManager = LayerManager::instance();
        
        // 移除新创建的路径
        if (m_newPath && m_newPath->scene()) {
            m_scene->removeItem(m_newPath);
            m_newPath->setSelected(false);
            // 从图层中移除
            if (layerManager && m_targetLayer) {
                m_targetLayer->removeShape(m_newPath);
            }
        }
        
        // 恢复原始形状，只有在它们不在场景中时才添加
        if (m_originalShape && !m_originalShape->scene()) {
            m_scene->addItem(m_originalShape);
            m_originalShape->setSelected(true);
            // 添加到原始图层
            if (layerManager && m_originalLayer) {
                m_originalLayer->addShape(m_originalShape);
            }
        }
        
        m_scene->update();
        
        // 刷新对象树模型
        if (layerManager) {
            if (m_originalLayer) emit layerManager->layerContentChanged(m_originalLayer);
            if (m_targetLayer) emit layerManager->layerContentChanged(m_targetLayer);
        }
    }
    
    void redo() override
    {
        if (!m_scene) return;
        
        LayerManager *layerManager = LayerManager::instance();
        
        // 移除原始形状，只有在它们在场景中时才移除
        if (m_originalShape && m_originalShape->scene()) {
            m_scene->removeItem(m_originalShape);
            m_originalShape->setSelected(false);
            // 从图层中移除
            if (layerManager && m_originalLayer) {
                m_originalLayer->removeShape(m_originalShape);
            }
        }
        
        // 添加新路径，只有在它不在场景中时才添加
        if (m_newPath && !m_newPath->scene()) {
            m_scene->addItem(m_newPath);
            m_newPath->setSelected(true);
            // 添加到目标图层
            if (layerManager && m_targetLayer) {
                m_targetLayer->addShape(m_newPath);
            }
        }
        
        m_scene->update();
        m_scene->setModified(true);
        
        // 刷新对象树模型
        if (layerManager) {
            emit layerManager->layerContentChanged(m_targetLayer);
        }
    }
    
private:
    DrawingScene *m_scene;
    DrawingShape *m_originalShape;
    DrawingPath *m_newPath;
    bool m_originalShapeInScene;
    bool m_newPathInScene;
    DrawingLayer *m_originalLayer;
    DrawingLayer *m_targetLayer;
};

// 执行路径操作
void MainWindow::executePathOperation(const QString &operation)
{
    if (!m_scene) return;
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        m_statusLabel->setText("需要选择一个路径进行操作");
        return;
    }
    
    DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(selected[0]);
    if (!shape) {
        m_statusLabel->setText("选择的对象不是有效的路径");
        return;
    }
    
    // 使用变换后的路径
    QPainterPath transformedPath = shape->transformedShape();
    QPainterPath resultPath;
    
    if (operation == "simplify") {
        resultPath = PathEditor::simplifyPath(transformedPath, 0.5);
    } else if (operation == "smooth") {
        resultPath = PathEditor::smoothPath(transformedPath, 0.5);
    } else if (operation == "curve") {
        resultPath = PathEditor::convertToCurve(transformedPath);
    } else if (operation == "offset") {
        resultPath = PathEditor::offsetPath(transformedPath, 5);
    } else if (operation == "clip") {
        // 使用图形的边界框作为裁剪区域
        QRectF bounds = transformedPath.boundingRect();
        QRectF clipRect = bounds.adjusted(10, 10, -10, -10); // 稍微缩小边界框
        resultPath = PathEditor::clipPath(transformedPath, clipRect);
    }
    
    if (resultPath.isEmpty()) {
        m_statusLabel->setText("路径操作失败");
        return;
    }
    
    // 创建一个新的DrawingPath来存储结果
    DrawingPath *newPath = new DrawingPath();
    
    // 重置路径的位置信息
    QRectF bounds = resultPath.boundingRect();
    QTransform offsetTransform;
    offsetTransform.translate(-bounds.left(), -bounds.top());
    QPainterPath adjustedPath = offsetTransform.map(resultPath);
    newPath->setPath(adjustedPath);
    newPath->setPos(shape->pos() + bounds.topLeft());
    newPath->setStrokePen(shape->strokePen());
    newPath->setFillBrush(shape->fillBrush());
    
    // 创建撤销命令 - 在修改场景之前创建
    QString operationText;
    if (operation == "simplify") operationText = "简化路径";
    else if (operation == "smooth") operationText = "平滑路径";
    else if (operation == "curve") operationText = "转换为曲线";
    else if (operation == "offset") operationText = "偏移路径";
    else if (operation == "clip") operationText = "裁剪路径";
    else operationText = "路径操作";
    
    PathOperationCommand *command = new PathOperationCommand(m_scene, shape, newPath, operationText);
    
    // 推送撤销命令，让命令处理场景和图层的同步
    m_scene->undoStack()->push(command);
    
    QString opName;
    if (operation == "simplify") opName = "简化";
    else if (operation == "smooth") opName = "平滑";
    else if (operation == "curve") opName = "转换为曲线";
    else if (operation == "offset") opName = "偏移";
    else if (operation == "clip") opName = "裁剪";
    else opName = "路径操作";
    
    m_statusLabel->setText(QString("%1操作完成").arg(opName));
}

// 在指定位置创建形状
void MainWindow::createShapeAtPosition(const QString &shapeType, const QPointF &pos)
{
    if (!m_scene) return;
    
    QPainterPath shape;
    
    if (shapeType == "arrow") {
        shape = PathEditor::createArrow(
            QPointF(pos.x() - 50, pos.y()),
            QPointF(pos.x() + 50, pos.y())
        );
    } else if (shapeType == "star") {
        shape = PathEditor::createStar(pos, 50, 5);
    } else if (shapeType == "gear") {
        shape = PathEditor::createGear(pos, 50, 8);
    }
    
    if (shape.isEmpty()) {
        m_statusLabel->setText("创建形状失败");
        return;
    }
    
    DrawingPath *newPath = new DrawingPath();
    newPath->setPath(shape);
    newPath->setPos(0, 0);
    newPath->setStrokePen(QPen(Qt::black, 2));
    
    if (shapeType == "star") {
        newPath->setFillBrush(QBrush(Qt::yellow));
    } else if (shapeType == "gear") {
        newPath->setFillBrush(QBrush(Qt::gray));
    } else {
        newPath->setFillBrush(Qt::NoBrush);
    }
    
    m_scene->addItem(newPath);
    newPath->setSelected(true);
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    QString shapeName;
    if (shapeType == "arrow") shapeName = "箭头";
    else if (shapeType == "star") shapeName = "星形";
    else if (shapeType == "gear") shapeName = "齿轮";
    else shapeName = "形状";
    
    m_statusLabel->setText(QString("创建%1完成").arg(shapeName));
}

// 将选中的文本转换为路径
void MainWindow::convertSelectedTextToPath()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    QList<DrawingShape*> convertedShapes;
    
    for (QGraphicsItem *item : selected) {
        DrawingText *textShape = qgraphicsitem_cast<DrawingText*>(item);
        if (textShape) {
            // 将文本转换为路径
            DrawingPath *pathShape = textShape->convertToPath();
            if (pathShape) {
                // 添加到场景
                m_scene->addItem(pathShape);
                pathShape->setSelected(true);
                convertedShapes.append(pathShape);
                
                // 获取文本所属的图层
                DrawingLayer *layer = nullptr;
                LayerManager *layerManager = LayerManager::instance();
                for (int i = 0; i < layerManager->layerCount(); ++i) {
                    DrawingLayer *checkLayer = layerManager->layer(i);
                    if (checkLayer->shapes().contains(textShape)) {
                        layer = checkLayer;
                        break;
                    }
                }
                
                // 安全地移除原始文本
                textShape->setSelected(false);
                m_scene->removeItem(textShape);
                
                // 从图层中移除原始文本
                if (layer) {
                    layer->removeShape(textShape);
                }
                
                // 将新路径添加到原文本的图层中
                if (layer) {
                    layer->addShape(pathShape);
                }
                
                delete textShape;
            }
        }
    }
    
    if (!convertedShapes.isEmpty()) {
        m_scene->setModified(true);
        m_statusLabel->setText(QString("已将 %1 个文本转换为路径").arg(convertedShapes.size()));
        
        // 更新图层面板显示
        LayerManager::instance()->updateLayerPanel();
    }
}

void MainWindow::togglePerformancePanel()
{
    // 性能监控现在集成在属性面板的tab页中
    // 切换到性能监控tab页
    if (m_tabbedPropertyPanel) {
        // 找到性能监控tab的索引
        for (int i = 0; i < m_tabbedPropertyPanel->count(); ++i) {
            if (m_tabbedPropertyPanel->tabText(i) == "性能监控") {
                m_tabbedPropertyPanel->setCurrentIndex(i);
                break;
            }
        }
    }
    
    // 更新菜单状态
    if (m_togglePerformancePanelAction) {
        m_togglePerformancePanelAction->setChecked(true);
    }
}

void MainWindow::onToolSwitchRequested(int toolType)
{
    if (m_toolManager) {
        // 切换到指定工具
        m_toolManager->switchTool(static_cast<ToolType>(toolType));
        
        // 更新场景中的当前工具状态
        if (m_scene) {
            m_scene->setCurrentTool(toolType);
        }
    }
}

void MainWindow::updateToolBarState(int currentTool)
{
    // 根据当前工具更新工具栏按钮的选中状态
    switch (static_cast<ToolType>(currentTool)) {
        case ToolType::Select:
            if (m_outlinePreviewToolAction) m_outlinePreviewToolAction->setChecked(true);
            break;
        case ToolType::Rectangle:
            if (m_rectangleToolAction) m_rectangleToolAction->setChecked(true);
            break;
        case ToolType::Ellipse:
            if (m_ellipseToolAction) m_ellipseToolAction->setChecked(true);
            break;
        case ToolType::Line:
            if (m_lineToolAction) m_lineToolAction->setChecked(true);
            break;
        case ToolType::Bezier:
            if (m_bezierToolAction) m_bezierToolAction->setChecked(true);
            break;
        case ToolType::NodeEdit:
            if (m_nodeEditToolAction) m_nodeEditToolAction->setChecked(true);
            break;
        case ToolType::Polygon:
            if (m_polygonToolAction) m_polygonToolAction->setChecked(true);
            break;
        case ToolType::Polyline:
            if (m_polylineToolAction) m_polylineToolAction->setChecked(true);
            break;
        case ToolType::Brush:
            if (m_brushToolAction) m_brushToolAction->setChecked(true);
            break;
        case ToolType::Pen:
            if (m_penToolAction) m_penToolAction->setChecked(true);
            break;
        case ToolType::Fill:
            if (m_fillToolAction) m_fillToolAction->setChecked(true);
            break;
        case ToolType::GradientFill:
            if (m_gradientFillToolAction) m_gradientFillToolAction->setChecked(true);
            break;
        case ToolType::Eraser:
            if (m_eraserToolAction) m_eraserToolAction->setChecked(true);
            break;
        case ToolType::Text:
            if (m_textToolAction) m_textToolAction->setChecked(true);
            break;
        case ToolType::PathEdit:
            if (m_pathEditToolAction) m_pathEditToolAction->setChecked(true);
            break;
        default:
            break;
    }
}


