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

#include "../ui/effect-manager.h"
#include "../ui/snap-manager.h"
#include "../ui/path-operations-manager.h"
#include "../ui/selection-manager.h"
#include "../ui/command-manager.h"
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
      m_horizontalRuler(nullptr), m_verticalRuler(nullptr), m_cornerWidget(nullptr),
      m_uiUpdateTimer(nullptr), m_lastSelectedCount(0), m_toolStateManager(nullptr), m_toolManager(nullptr), m_shortcutManager(nullptr), m_effectManager(nullptr),
      m_isUntitled(true)
{
    // 初始化工具管理系统
    m_toolStateManager = new ToolStateManager(this);
    m_toolManager = new ToolManager(this);
    m_toolManager->setToolStateManager(m_toolStateManager);
    
    // 初始化快捷键管理器
    m_shortcutManager = new ShortcutManager(this);
    m_shortcutManager->setToolManager(m_toolManager);
    // 场景将在newFile()后设置
    
    
    
    // 初始化效果管理器
    m_effectManager = new EffectManager(this);
    
    // 连接EffectManager信号
    connect(m_effectManager, &EffectManager::effectApplied, this, &MainWindow::onEffectApplied);
    connect(m_effectManager, &EffectManager::effectCleared, this, &MainWindow::onEffectCleared);
    connect(m_effectManager, &EffectManager::statusMessageChanged, this, &MainWindow::onStatusMessageChanged);
    
    // 初始化吸附管理器（合并了网格管理器）
    m_snapManager = new SnapManager(m_scene, this);
    
    // 连接SnapManager信号
    connect(m_snapManager, &SnapManager::statusMessageChanged, this, &MainWindow::onStatusMessageChanged);
    // gridVisibilityChanged信号现在来自DrawingScene
    // 注意：这个连接会在setupCanvas中重新建立，确保Scene已创建
    connect(m_snapManager, &SnapManager::gridAlignmentChanged, this, [this](bool enabled) {
        if (m_toggleGridAlignmentAction) {
            m_toggleGridAlignmentAction->setChecked(enabled);
        }
    });
    
    // 初始化路径操作管理器
    m_pathOperationsManager = new PathOperationsManager(this);
    
    // 连接PathOperationsManager信号
    connect(m_pathOperationsManager, &PathOperationsManager::statusMessageChanged, this, &MainWindow::onStatusMessageChanged);
    
    // 初始化选择管理器
    m_selectionManager = new SelectionManager(this);
    
    // 连接SelectionManager信号
    connect(m_selectionManager, &SelectionManager::statusMessageChanged, this, &MainWindow::onStatusMessageChanged);
    connect(m_selectionManager, &SelectionManager::selectionChanged, this, [this](int count) {
        m_lastSelectedCount = count;
        updateUI();
    });
    
    // 初始化命令管理器并设置单例
    m_commandManager = new CommandManager(this);
    CommandManager::setInstance(m_commandManager);
    
    // 设置命令管理器到选择管理器
    m_selectionManager->setCommandManager(m_commandManager);
    
    // 设置EffectManager的CommandManager
    if (m_effectManager) {
        m_effectManager->setCommandManager(m_commandManager);
    }
    
    // 设置 CommandManager 的 Scene
    m_commandManager->setScene(m_scene);
    
    // 连接CommandManager的撤销栈信号到UI
    if (m_commandManager) {
        connect(m_commandManager, &CommandManager::canUndoChanged,
                this, [this](bool canUndo) { m_undoAction->setEnabled(canUndo); });
        connect(m_commandManager, &CommandManager::canRedoChanged,
                this, [this](bool canRedo) { m_redoAction->setEnabled(canRedo); });
    }
    
    // 调试输出：确认命令管理器初始化
    qDebug() << "CommandManager initialized and set to SelectionManager";
    
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
        
        // 检查当前工具并更新工具栏状态
        if (m_toolManager) {
            updateToolBarState(static_cast<int>(m_toolManager->currentToolType()));
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
    if (m_selectionManager) {
        delete m_selectionManager;
        m_selectionManager = nullptr;
    }
    if (m_snapManager) {
        delete m_snapManager;
        m_snapManager = nullptr;
    }
    if (m_effectManager) {
        delete m_effectManager;
        m_effectManager = nullptr;
    }
    if (m_commandManager) {
        // 先清理单例引用
        CommandManager::clearInstance();
        // 然后删除对象
        delete m_commandManager;
        m_commandManager = nullptr;
    }
    if (m_pathOperationsManager) {
        delete m_pathOperationsManager;
        m_pathOperationsManager = nullptr;
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
    
    // 设置网格图标的初始状态（确保与场景状态同步）
    if (m_toggleGridAction) {
        m_toggleGridAction->setChecked(m_scene->isGridVisible());
    }
    // 吸附设置现在通过SnapManager管理
    if (m_snapManager) {
        m_snapManager->setSnapEnabled(true); // 启用智能吸附
        m_snapManager->setObjectSnapEnabled(true); // 启用对象吸附
        m_snapManager->setSnapTolerance(3); // 设置吸附容差（降低灵敏度）
        m_snapManager->setObjectSnapTolerance(3); // 设置对象吸附容差（降低灵敏度）
    }
    
    // Create rulers
    m_horizontalRuler = new Ruler(Ruler::Horizontal, this);
    m_verticalRuler = new Ruler(Ruler::Vertical, this);

    // Create drawing canvas with grid functionality
    m_canvas = new DrawingCanvas(this);
    m_canvas->setScene(m_scene);

    
    
    // 设置效果管理器的scene
    if (m_effectManager) {
        m_effectManager->setScene(m_scene);
    }
    
    // 设置吸附管理器的scene（场景创建后需要重新设置）
    if (m_snapManager) {
        m_snapManager->setScene(m_scene);
        // 设置Scene的SnapManager引用
        m_scene->setSnapManager(m_snapManager);
        
        // 连接Scene的gridVisibilityChanged信号
        connect(m_scene, &DrawingScene::gridVisibilityChanged, this, [this](bool visible) {
            if (m_toggleGridAction) {
                m_toggleGridAction->setChecked(visible);
            }
        });
    }
    
    // 设置路径操作管理器的scene
    if (m_pathOperationsManager) {
        m_pathOperationsManager->setScene(m_scene);
    }
    
    // 设置选择管理器的scene
    if (m_selectionManager) {
        m_selectionManager->setScene(m_scene);
    }
    
    // 设置命令管理器的scene
    if (m_commandManager) {
        m_commandManager->setScene(m_scene);
        // 同时设置场景的CommandManager引用，供工具使用
        
    }

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
    
    // Connect undo stack signals to update menu states - 使用CommandManager
    if (m_commandManager) {
        // CommandManager的信号已在构造函数中连接
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
    m_undoView = new QUndoView(historyDock);
    historyDock->setWidget(m_undoView);
    addDockWidget(Qt::RightDockWidgetArea, historyDock);
    
    // 设置撤销视图（如果CommandManager已初始化）
    if (m_commandManager) {
        setupUndoView();
    }
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
    m_copyAction->setShortcutContext(Qt::ApplicationShortcut);
    m_copyAction->setStatusTip("复制选中项目");

    m_pasteAction = new QAction("&粘贴", this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setShortcutContext(Qt::ApplicationShortcut);
    m_pasteAction->setStatusTip("粘贴项目");

    m_duplicateAction = new QAction("&快速复制", this);
    m_duplicateAction->setShortcut(QKeySequence("Ctrl+D"));
    m_duplicateAction->setShortcutContext(Qt::ApplicationShortcut);
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
    // 初始状态将在GridManager初始化后设置

    m_gridSizeAction = new QAction("网格大小...", this);
    m_gridSizeAction->setStatusTip("设置网格大小");

    m_gridColorAction = new QAction("网格颜色...", this);
    m_gridColorAction->setStatusTip("设置网格颜色");
    
    m_toggleGridAlignmentAction = new QAction("网格对齐(&A)", this);
    m_toggleGridAlignmentAction->setCheckable(true);
    m_toggleGridAlignmentAction->setShortcut(QKeySequence("Shift+G"));
    m_toggleGridAlignmentAction->setStatusTip("启用或禁用网格对齐");
    // 初始状态将在GridManager初始化后设置
    
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
    
    // 设置SnapManager相关动作的初始状态（在所有动作创建后）
    // 注意：场景可能在createActions()之后才创建，所以这里先不设置网格图标状态
    // 网格图标状态将在createCanvas()之后设置
    if (m_snapManager) {
        if (m_toggleGridAlignmentAction) {
            m_toggleGridAlignmentAction->setChecked(m_snapManager->isGridAlignmentEnabled());
        }
    }
}

void MainWindow::connectActions()
{
    // File connections
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newFile);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_saveAction, &QAction::triggered, this, [this]() { saveFile(); });
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(m_exportAction, &QAction::triggered, this, [this]() { exportFile(); });
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    // Edit connections
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undo);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redo);
    connect(m_deleteAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->deleteSelected();
        }
    });
    connect(m_copyAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->copySelected();
        }
    });
    connect(m_pasteAction, &QAction::triggered, this, [this]() {
        qDebug() << "Paste action triggered";
        if (m_selectionManager) {
            m_selectionManager->paste();
        } else {
            qDebug() << "SelectionManager is null, cannot paste";
        }
    });
    connect(m_duplicateAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->duplicate();
        }
    });
    connect(m_blurEffectAction, &QAction::triggered, this, &MainWindow::applyBlurEffect);
    connect(m_dropShadowEffectAction, &QAction::triggered, this, &MainWindow::applyDropShadowEffect);
    connect(m_clearFilterAction, &QAction::triggered, this, &MainWindow::clearFilterEffect);
    connect(m_convertTextToPathAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->convertSelectedTextToPath();
        }
    });
    connect(m_selectAllAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->selectAll();
        }
    });
    connect(m_deselectAllAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->deselectAll();
        }
    });

    // View connections
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);
    connect(m_resetZoomAction, &QAction::triggered, this, &MainWindow::resetZoom);
    connect(m_fitToWindowAction, &QAction::triggered, this, &MainWindow::fitToWindow);

    // Grid connections
    connect(m_toggleGridAction, &QAction::triggered, this, [this]() {
        if (m_canvas && m_canvas->scene()) {
            DrawingScene* scene = static_cast<DrawingScene*>(m_canvas->scene());
            scene->setGridVisible(!scene->isGridVisible());
        }
    });
    connect(m_gridSizeAction, &QAction::triggered, this, [this]() {
        // 网格大小设置暂时简化
        if (m_canvas && m_canvas->scene()) {
            DrawingScene* scene = static_cast<DrawingScene*>(m_canvas->scene());
            scene->setGridSize(20); // 可以添加设置对话框
        }
    });
    connect(m_gridColorAction, &QAction::triggered, this, [this]() {
        // 网格颜色设置暂时简化
        if (m_canvas && m_canvas->scene()) {
            DrawingScene* scene = static_cast<DrawingScene*>(m_canvas->scene());
            scene->setGridColor(QColor(200, 200, 200, 100)); // 可以添加设置对话框
        }
    });
    connect(m_toggleGridAlignmentAction, &QAction::triggered, this, [this]() {
        if (m_snapManager) {
            m_snapManager->toggleGridAlignment();
        }
    });
    connect(m_clearAllGuidesAction, &QAction::triggered, this, &MainWindow::clearAllGuides);
    connect(m_togglePerformancePanelAction, &QAction::triggered, this, &MainWindow::togglePerformancePanel);
    
    // Group connections
    connect(m_groupAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->groupSelected();
        }
    });
    connect(m_ungroupAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->ungroupSelected();
        }
    });
    
    // Z-order connections
    connect(m_bringToFrontAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->bringToFront();
        }
    });
    connect(m_sendToBackAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->sendToBack();
        }
    });
    connect(m_bringForwardAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->bringForward();
        }
    });
    connect(m_sendBackwardAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->sendBackward();
        }
    });
    
    // Align connections
    connect(m_alignLeftAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->alignLeft();
        }
    });
    connect(m_alignCenterAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->alignCenter();
        }
    });
    connect(m_alignRightAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->alignRight();
        }
    });
    connect(m_alignTopAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->alignTop();
        }
    });
    connect(m_alignMiddleAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->alignMiddle();
        }
    });
    connect(m_alignBottomAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->alignBottom();
        }
    });
    connect(m_sameWidthAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->sameWidth();
        }
    });
    connect(m_sameHeightAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->sameHeight();
        }
    });
    connect(m_sameSizeAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->sameSize();
        }
    });
    connect(m_distributeHorizontalAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->distributeHorizontal();
        }
    });
    connect(m_distributeVerticalAction, &QAction::triggered, this, [this]() {
        if (m_selectionManager) {
            m_selectionManager->distributeVertical();
        }
    });

    // 添加复制粘贴动作到窗口以确保快捷键生效
    addAction(m_copyAction);
    addAction(m_pasteAction);
    addAction(m_duplicateAction);

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
    // 路径布尔运算连接
    connect(m_pathUnionAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->pathUnion();
        }
    });
    connect(m_pathSubtractAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->pathSubtract();
        }
    });
    connect(m_pathIntersectAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->pathIntersect();
        }
    });
    connect(m_pathXorAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->pathXor();
        }
    });
    
    // 路径编辑连接
    connect(m_pathSimplifyAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->pathSimplify();
        }
    });
    connect(m_pathSmoothAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->pathSmooth();
        }
    });
    connect(m_pathReverseAction, &QAction::triggered, this, [this]() {
        if (m_pathOperationsManager) {
            m_pathOperationsManager->pathReverse();
        }
    });
    connect(m_generateShapeAction, &QAction::triggered, this, [this]() {
            // m_pathOperationsManager->generateShape(); // 方法暂时不存在
        });

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
    if (!m_canvas) return;
    
    if (maybeSave()) {
        if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene())) {
            drawingScene->clearScene();
        }
        m_currentFilePath.clear();
        m_isUntitled = true;
        updateWindowTitle();
        emit onStatusMessageChanged("新文档已创建");
    }
}

void MainWindow::openFile()
{
    if (!m_canvas) return;
    
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
            tr("打开文件"), "",
            tr("SVG 文件 (*.svg);;所有文件 (*.*)"));
        
        if (!fileName.isEmpty()) {
            loadFile(fileName);
        }
    }
}

void MainWindow::saveFile()
{
    if (!m_canvas) return;
    
    if (m_isUntitled) {
        saveFileAs();
    } else {
        saveFile(m_currentFilePath);
    }
}

void MainWindow::saveFileAs()
{
    if (!m_canvas) return;
    
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("保存文件"), m_currentFilePath,
        tr("SVG 文件 (*.svg);;所有文件 (*.*)"));
    
    if (!fileName.isEmpty()) {
        saveFile(fileName);
    }
}

void MainWindow::exportFile()
{
    if (!m_canvas) return;
    
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("导出文件"), m_currentFilePath,
        tr("SVG 文件 (*.svg);;所有文件 (*.*)"));
    
    if (!fileName.isEmpty()) {
        exportFile(fileName);
    }
}

void MainWindow::undo()
{
    if (m_commandManager) {
        m_commandManager->undo();
    }
}

// FileManager信号处理函数


void MainWindow::onStatusMessageChanged(const QString &message)
{
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
}

void MainWindow::onWindowTitleChanged(const QString &title)
{
    setWindowTitle(title);
}

// EffectManager信号处理函数
void MainWindow::onEffectApplied(const QString &effectName)
{
    Q_UNUSED(effectName)
    // 效果应用完成，无需特殊处理
}

void MainWindow::onEffectCleared()
{
    // 效果清除完成，无需特殊处理
}

void MainWindow::redo()
{
    if (m_commandManager) {
        m_commandManager->redo();
    }
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
    if (m_commandManager) {
        m_commandManager->pushCommand(command);
    }
    
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
    // 现在由DrawingCanvas内部管理修改状态
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
    // Update window title - 已合并到MainWindow中
    updateWindowTitle();

    // Update undo/redo actions - 使用CommandManager
    if (m_commandManager)
    {
        m_undoAction->setEnabled(m_commandManager->canUndo());
        m_redoAction->setEnabled(m_commandManager->canRedo());
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

    // Update save action - 现在由DrawingScene管理修改状态
    if (DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas ? m_canvas->scene() : nullptr)) {
        m_saveAction->setEnabled(drawingScene->isModified());
    } else {
        m_saveAction->setEnabled(false);
    }
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
    DrawingScene* drawingScene = m_canvas ? qobject_cast<DrawingScene*>(m_canvas->scene()) : nullptr;
    if (drawingScene && drawingScene->isModified())
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "VectorQt",
                                                                  "文档已修改，是否保存？",
                                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Save)
        {
            if (!saveFile(m_currentFilePath)) {
                event->ignore();
                return;
            }
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

QColor MainWindow::getCurrentFillColor() const
{
    if (m_colorPalette) {
        return m_colorPalette->getCurrentFillColor();
    }
    return Qt::blue; // 默认颜色
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

// 在指定位置创建形状

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
        default:
            break;
    }
}

// 委托给ZoomManager的方法
void MainWindow::zoomIn()
{
    if (m_canvas) {
        m_canvas->zoomIn();
    }
}

void MainWindow::zoomOut()
{
    if (m_canvas) {
        m_canvas->zoomOut();
    }
}

void MainWindow::resetZoom()
{
    if (m_canvas) {
        m_canvas->resetZoom();
    }
}

void MainWindow::fitToWindow()
{
    if (m_canvas) {
        m_canvas->fitToWindow();
    }
}

// 委托给EffectManager的方法
void MainWindow::applyBlurEffect()
{
    if (m_effectManager) {
        m_effectManager->applyBlurEffect();
    }
}

void MainWindow::applyDropShadowEffect()
{
    if (m_effectManager) {
        m_effectManager->applyDropShadowEffect();
    }
}

void MainWindow::clearFilterEffect()
{
    if (m_effectManager) {
        m_effectManager->clearFilterEffect();
    }
}

// 委托给SnapManager的方法
void MainWindow::showGridSettings()
{
    // 网格设置暂时简化，可以直接在这里添加简单的设置逻辑
    if (m_snapManager) {
        // 可以添加简单的设置对话框或者使用状态栏消息
        emit onStatusMessageChanged("网格设置功能待实现");
    }
}

void MainWindow::clearAllGuides()
{
    if (m_scene) {
        m_scene->clearGuides();
    }
}

// 委托给SelectionManager的方法
void MainWindow::showContextMenu(const QPointF &pos)
{
    QMenu contextMenu(this);
    
    // 添加基本编辑操作
    if (m_scene && !m_scene->selectedItems().isEmpty()) {
        contextMenu.addAction(m_copyAction);
        contextMenu.addAction(m_pasteAction);
        contextMenu.addAction(m_duplicateAction);
        contextMenu.addSeparator();
        contextMenu.addAction(m_deleteAction);
        contextMenu.addSeparator();
        
        // 添加对齐操作
        QMenu *alignMenu = contextMenu.addMenu("对齐");
        alignMenu->addAction(m_alignLeftAction);
        alignMenu->addAction(m_alignCenterAction);
        alignMenu->addAction(m_alignRightAction);
        alignMenu->addSeparator();
        alignMenu->addAction(m_alignTopAction);
        alignMenu->addAction(m_alignMiddleAction);
        alignMenu->addAction(m_alignBottomAction);
        
        // 添加路径操作
        if (m_scene->selectedItems().count() > 1) {
            QMenu *pathMenu = contextMenu.addMenu("路径操作");
            pathMenu->addAction(m_pathUnionAction);
            pathMenu->addAction(m_pathSubtractAction);
            pathMenu->addAction(m_pathIntersectAction);
            pathMenu->addAction(m_pathXorAction);
            pathMenu->addSeparator();
            pathMenu->addAction(m_pathSimplifyAction);
            pathMenu->addAction(m_pathSmoothAction);
            pathMenu->addAction(m_pathReverseAction);
        }
        
        // 添加滤镜操作
        QMenu *filterMenu = contextMenu.addMenu("滤镜");
        filterMenu->addAction(m_blurEffectAction);
        filterMenu->addAction(m_dropShadowEffectAction);
        filterMenu->addSeparator();
        filterMenu->addAction(m_clearFilterAction);
        
        // 添加组合操作
        if (m_scene->selectedItems().count() > 1) {
            contextMenu.addSeparator();
            contextMenu.addAction(m_groupAction);
        }
        
        // 添加取消组合操作（检查是否有选中的组合对象）
        bool hasGroup = false;
        for (QGraphicsItem *item : m_scene->selectedItems()) {
            DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
            if (shape && shape->shapeType() == DrawingShape::Group) {
                hasGroup = true;
                break;
            }
        }
        if (hasGroup) {
            if (m_scene->selectedItems().count() == 1) {
                contextMenu.addAction(m_ungroupAction);
            } else {
                // 如果有多个选中项且包含组合对象，添加分隔符和取消组合
                contextMenu.addSeparator();
                contextMenu.addAction(m_ungroupAction);
            }
        } else if (m_scene->selectedItems().count() > 1) {
            // 如果没有组合对象但有多于一个选中项，添加分隔符
            contextMenu.addSeparator();
        }
    } else {
        contextMenu.addAction(m_pasteAction);
    }
    
    contextMenu.exec(m_canvas->view()->mapToGlobal(m_canvas->view()->mapFromScene(pos)));
}

// 工具切换方法
void MainWindow::onToolSwitchRequested(int toolType)
{
    if (m_toolManager) {
        m_toolManager->switchTool(static_cast<ToolType>(toolType));
    }
}

// 委托给其他管理器的方法
void MainWindow::togglePerformancePanel()
{
    // 性能面板现在集成在属性面板的tab中
    // 简单切换到性能面板（假设是最后一个tab）
    if (m_tabbedPropertyPanel) {
        int count = m_tabbedPropertyPanel->count();
        if (count > 0) {
            m_tabbedPropertyPanel->setCurrentIndex(count - 1);
        }
    }
}

// 文件操作辅助方法实现（从FileManager合并而来）
bool MainWindow::maybeSave()
{
    DrawingScene* drawingScene = m_canvas ? qobject_cast<DrawingScene*>(m_canvas->scene()) : nullptr;
    if (drawingScene && drawingScene->isModified()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "VectorQt",
                                                                  "文档已修改，是否保存？",
                                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (reply == QMessageBox::Save) {
            return saveFile(m_isUntitled ? QString() : m_currentFilePath);
        } else if (reply == QMessageBox::Cancel) {
            return false;
        }
    }
    return true;
}

void MainWindow::loadFile(const QString &filePath)
{
    if (!m_canvas) return;
    
    DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene());
    if (!drawingScene) return;
    
    if (SvgHandler::importFromSvg(drawingScene, filePath)) {
        setCurrentFile(filePath);
        emit onStatusMessageChanged(tr("文件已加载: %1").arg(filePath));
    } else {
        QMessageBox::warning(this, "VectorQt", tr("无法加载文件 %1").arg(filePath));
    }
}

bool MainWindow::saveFile(const QString &filePath)
{
    if (!m_canvas) return false;
    
    DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene());
    if (!drawingScene) return false;
    
    if (SvgHandler::exportToSvg(drawingScene, filePath)) {
        setCurrentFile(filePath);
        drawingScene->setModified(false);
        emit onStatusMessageChanged(tr("文件已保存: %1").arg(filePath));
        return true;
    } else {
        QMessageBox::warning(this, "VectorQt", tr("无法保存文件 %1").arg(filePath));
        return false;
    }
}

bool MainWindow::exportFile(const QString &filePath)
{
    if (!m_canvas) return false;
    
    DrawingScene* drawingScene = qobject_cast<DrawingScene*>(m_canvas->scene());
    if (!drawingScene) return false;
    
    if (SvgHandler::exportToSvg(drawingScene, filePath)) {
        emit onStatusMessageChanged(tr("文件已导出: %1").arg(filePath));
        return true;
    } else {
        QMessageBox::warning(this, "VectorQt", tr("无法导出文件 %1").arg(filePath));
        return false;
    }
}

void MainWindow::setCurrentFile(const QString &filePath)
{
    m_currentFilePath = filePath;
    m_isUntitled = filePath.isEmpty();
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title = "VectorQt";
    if (!m_isUntitled) {
        title += " - " + QFileInfo(m_currentFilePath).fileName();
    } else {
        title += " - 未命名";
    }
    
    DrawingScene* drawingScene = m_canvas ? qobject_cast<DrawingScene*>(m_canvas->scene()) : nullptr;
    if (drawingScene && drawingScene->isModified()) {
        title += "*";
    }
    
    setWindowTitle(title);
}

// 🌟 参考线相关槽函数
void MainWindow::onGuideRequested(const QPointF &position, Qt::Orientation orientation)
{
    if (!m_scene) return;
    
    // 创建参考线
    qreal pos = orientation == Qt::Horizontal ? position.y() : position.x();
    m_scene->addGuide(orientation, pos);
    
    // 更新状态消息
    QString message = orientation == Qt::Horizontal ? "添加水平参考线" : "添加垂直参考线";
    updateStatusBar(message);
}

void MainWindow::setupUndoView()
{
    if (m_undoView && m_commandManager) {
        m_undoView->setStack(m_commandManager->undoStack());
        qDebug() << "QUndoView set to CommandManager undoStack";
    } else {
        qDebug() << "Failed to set QUndoView - m_undoView:" << m_undoView << "m_commandManager:" << m_commandManager;
    }
}
