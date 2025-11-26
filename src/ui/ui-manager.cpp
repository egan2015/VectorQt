#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QGraphicsView>
#include "ui-manager.h"
#include "drawingscene.h"
#include "../core/drawing-canvas.h"
#include "../ui/drawingview.h"
#include "../ui/colorpalette.h"
#include "../ui/scrollable-toolbar.h"
#include "../ui/ruler.h"

UIManager::UIManager(QMainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_scene(nullptr)
    , m_canvas(nullptr)
    , m_view(nullptr)
    , m_colorPalette(nullptr)
    , m_scrollableToolBar(nullptr)
    , m_horizontalRuler(nullptr)
    , m_verticalRuler(nullptr)
    , m_cornerWidget(nullptr)
    , m_statusLabel(nullptr)
    , m_zoomLabel(nullptr)
    , m_positionLabel(nullptr)
    , m_performanceDock(nullptr)
{
}

UIManager::~UIManager()
{
}

void UIManager::setupUI()
{
    createCentralWidget();
    createColorPalette();
    setupRulers();
    connectUIComponents();
    
    emit uiInitialized();
}

void UIManager::setupMenus()
{
    // 创建菜单栏
    QMenuBar *menuBar = m_mainWindow->menuBar();
    
    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");
    // TODO: 从ActionManager获取动作并添加到菜单
    
    // 编辑菜单
    QMenu *editMenu = menuBar->addMenu("编辑(&E)");
    // TODO: 从ActionManager获取动作并添加到菜单
    
    // 视图菜单
    QMenu *viewMenu = menuBar->addMenu("视图(&V)");
    // TODO: 从ActionManager获取动作并添加到菜单
    
    // 工具菜单
    QMenu *toolsMenu = menuBar->addMenu("工具(&T)");
    // TODO: 从ActionManager获取动作并添加到菜单
    
    // 效果菜单
    QMenu *effectsMenu = menuBar->addMenu("效果(&E)");
    // TODO: 从ActionManager获取动作并添加到菜单
    
    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");
    // TODO: 从ActionManager获取动作并添加到菜单
}

void UIManager::setupToolbars()
{
    // 主工具栏
    QToolBar *mainToolBar = m_mainWindow->addToolBar("主工具栏");
    mainToolBar->setObjectName("MainToolBar");
    // TODO: 从ActionManager获取动作并添加到工具栏
    
    // 绘图工具栏
    QToolBar *drawingToolBar = m_mainWindow->addToolBar("绘图工具");
    drawingToolBar->setObjectName("DrawingToolBar");
    // TODO: 从ActionManager获取动作并添加到工具栏
    
    // 属性工具栏
    QToolBar *propertyToolBar = m_mainWindow->addToolBar("属性");
    propertyToolBar->setObjectName("PropertyToolBar");
    // TODO: 从ActionManager获取动作并添加到工具栏
}

void UIManager::setupDocks()
{
    // 创建性能面板（保留用于菜单兼容性）
    m_performanceDock = new QDockWidget("性能", m_mainWindow);
    m_performanceDock->setObjectName("PerformanceDock");
    m_performanceDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    // TODO: 创建性能面板内容
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_performanceDock);
}

void UIManager::setupStatusBar()
{
    QStatusBar *statusBar = m_mainWindow->statusBar();
    
    // 状态标签
    m_statusLabel = new QLabel("就绪");
    statusBar->addWidget(m_statusLabel);
    
    // 添加弹性空间
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusBar->addWidget(spacer);
    
    // 缩放标签
    m_zoomLabel = new QLabel("100%");
    statusBar->addWidget(m_zoomLabel);
    
    // 位置标签
    m_positionLabel = new QLabel("0, 0");
    statusBar->addWidget(m_positionLabel);
}

void UIManager::updateStatusBar(const QString &message)
{
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
    emit statusMessageChanged(message);
}

void UIManager::updateZoomLabel(const QString &zoomText)
{
    if (m_zoomLabel) {
        m_zoomLabel->setText(zoomText);
    }
}

void UIManager::updatePositionLabel(const QString &positionText)
{
    if (m_positionLabel) {
        m_positionLabel->setText(positionText);
    }
}

void UIManager::createCentralWidget()
{
    // Create scene
    m_scene = new DrawingScene(m_mainWindow);
    m_scene->setSceneRect(0, 0, 1000, 800);
    m_scene->setGridVisible(true);
    m_scene->setGridAlignmentEnabled(true);
    m_scene->setSnapEnabled(true);
    m_scene->setObjectSnapEnabled(true);
    m_scene->setSnapTolerance(3);
    m_scene->setObjectSnapTolerance(3);
    
    // Create rulers
    m_horizontalRuler = new Ruler(Ruler::Horizontal, m_mainWindow);
    m_verticalRuler = new Ruler(Ruler::Vertical, m_mainWindow);

    // Create drawing canvas
    m_canvas = new DrawingCanvas(m_mainWindow);
    m_canvas->setScene(m_scene);
    
    // Get the view
    m_view = qobject_cast<DrawingView*>(m_canvas->view());
    
    // 确保视图正确初始化
    if (m_view) {
        m_view->centerOn(0, 0);
        m_view->ensureVisible(m_scene->sceneRect());
        m_view->update();
        m_scene->update();
    }

    // 初始化标尺
    if (m_view) {
        m_horizontalRuler->setView(m_view);
        m_verticalRuler->setView(m_view);

        QPoint origin = m_view->mapFromScene(QPoint(0, 0));
        m_horizontalRuler->setOrigin(origin.x());
        m_verticalRuler->setOrigin(origin.y());
        m_horizontalRuler->setScale(m_view->zoomLevel());
        m_verticalRuler->setScale(m_view->zoomLevel());
    }
    
    // 设置中心部件
    QWidget *centralWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // 创建标尺容器
    QWidget *rulerContainer = new QWidget();
    QHBoxLayout *rulerLayout = new QHBoxLayout(rulerContainer);
    rulerLayout->setContentsMargins(0, 0, 0, 0);
    rulerLayout->setSpacing(0);
    
    // 左上角角落部件
    m_cornerWidget = new QWidget(m_mainWindow);
    m_cornerWidget->setFixedSize(20, 20);
    m_cornerWidget->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc;");
    
    rulerLayout->addWidget(m_cornerWidget);
    rulerLayout->addWidget(m_horizontalRuler);
    
    // 主要内容区域
    QWidget *mainContent = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(mainContent);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    mainLayout->addWidget(m_verticalRuler);
    mainLayout->addWidget(m_canvas);
    
    layout->addWidget(rulerContainer);
    layout->addWidget(mainContent);
    
    m_mainWindow->setCentralWidget(centralWidget);
}

void UIManager::createColorPalette()
{
    // Create color palette
    m_colorPalette = new ColorPalette(m_mainWindow);
    m_colorPalette->setScene(m_scene);
    
    // Create a dock widget to contain the color palette
    QDockWidget *colorPaletteDock = new QDockWidget("", nullptr);
    colorPaletteDock->setWidget(m_colorPalette);
    colorPaletteDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    colorPaletteDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    colorPaletteDock->setTitleBarWidget(new QWidget());
    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, colorPaletteDock);
}

void UIManager::setupRulers()
{
    // 标尺已在createCentralWidget中初始化
    // 这里可以添加额外的标尺设置
}

void UIManager::connectUIComponents()
{
    // 连接画布信号
    if (m_canvas) {
        connect(m_canvas, &DrawingCanvas::zoomChanged, 
                this, [this](double zoom) {
                    updateZoomLabel(QString("%1%").arg(qRound(zoom * 100)));
                });
        
        connect(m_canvas, &DrawingCanvas::mousePositionChanged,
                this, [this](const QPointF &pos) {
                    updatePositionLabel(QString("%1, %2").arg(qRound(pos.x())).arg(qRound(pos.y())));
                });
        
        connect(m_canvas, &DrawingCanvas::viewportChanged,
                this, &UIManager::updateRulers);
    }
    
    // 连接场景信号
    if (m_scene) {
        connect(m_scene, &DrawingScene::selectionChanged,
                this, [this]() {
                    updateRulers();
                });
    }
}

void UIManager::updateRulers()
{
    if (m_horizontalRuler && m_verticalRuler && m_view) {
        QPoint origin = m_view->mapFromScene(QPoint(0, 0));
        m_horizontalRuler->setOrigin(origin.x());
        m_verticalRuler->setOrigin(origin.y());
        m_horizontalRuler->setScale(m_view->zoomLevel());
        m_verticalRuler->setScale(m_view->zoomLevel());
    }
}