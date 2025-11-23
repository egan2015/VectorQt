#include <QTabWidget>
#include <QVBoxLayout>
#include <QIcon>
#include "../ui/tabbed-property-panel.h"
#include "../ui/propertypanel.h"
#include "../ui/layer-panel.h"
#include "../core/layer-manager.h"
#include "../ui/tools-panel.h"
#include "../ui/page-settings-panel.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../ui/object-tree-view.h"

TabbedPropertyPanel::TabbedPropertyPanel(QWidget *parent)
    : QTabWidget(parent)
    , m_propertiesPanel(nullptr)
    , m_layersPanel(nullptr)
    , m_toolsPanel(nullptr)
    , m_objectTreeView(nullptr)
    , m_pageSettingsPanel(nullptr)
    , m_scene(nullptr)
    , m_view(nullptr)
{
    // 设置TabBar样式
    setTabPosition(QTabWidget::North);
    setMovable(false);
    setUsesScrollButtons(false);
    
    // 添加所有面板
    addPropertiesPanel();
    addLayersPanel();
    addToolsPanel();
    addPageSettingsPanel();
    
    // 连接标签切换信号
    connect(this, &QTabWidget::currentChanged, this, &TabbedPropertyPanel::currentPanelChanged);
}

void TabbedPropertyPanel::addPropertiesPanel()
{
    if (!m_propertiesPanel) {
        m_propertiesPanel = new PropertyPanel(this);
    }
    
    // 设置场景引用
    if (m_scene) {
        m_propertiesPanel->setScene(m_scene);
    }
    
    // 添加为第一个标签
    addTab(m_propertiesPanel, tr("属性"));
}

void TabbedPropertyPanel::addLayersPanel()
{
    if (!m_layersPanel) {
        m_layersPanel = new LayerPanel(this);
    }
    
    // 添加为第二个标签
    addTab(m_layersPanel, tr("图层与对象"));
    
    // 注意：LayerManager将在setLayerManager中设置场景引用
}

void TabbedPropertyPanel::addToolsPanel()
{
    if (!m_toolsPanel) {
        m_toolsPanel = new ToolsPanel(this);
    }
    
    // 设置场景引用
    if (m_scene) {
        m_toolsPanel->setScene(m_scene);
    }
    
    // 添加为第三个标签
    addTab(m_toolsPanel, tr("工具"));
}

void TabbedPropertyPanel::addObjectTreePanel()
{
    // 对象标签页已移除，对象树现在集成在图层与对象标签页中
    // 此方法保留以保持兼容性，但不执行任何操作
}

void TabbedPropertyPanel::addPageSettingsPanel()
{
    if (!m_pageSettingsPanel) {
        m_pageSettingsPanel = new PageSettingsPanel(this);
    }
    
    // 设置场景和视图引用
    if (m_scene) {
        m_pageSettingsPanel->setScene(m_scene);
    }
    if (m_view) {
        m_pageSettingsPanel->setView(m_view);
    }
    
    // 添加为第四个标签
    addTab(m_pageSettingsPanel, tr("页面"));
}

QWidget* TabbedPropertyPanel::getCurrentPanel() const
{
    return currentWidget();
}

void TabbedPropertyPanel::switchToPropertiesPanel()
{
    if (m_propertiesPanel) {
        setCurrentWidget(m_propertiesPanel);
    }
}

void TabbedPropertyPanel::switchToLayersPanel()
{
    if (m_layersPanel) {
        setCurrentWidget(m_layersPanel);
    }
}

void TabbedPropertyPanel::switchToToolsPanel()
{
    if (m_toolsPanel) {
        setCurrentWidget(m_toolsPanel);
    }
}

void TabbedPropertyPanel::switchToObjectTreePanel()
{
    if (m_objectTreeView) {
        setCurrentWidget(m_objectTreeView);
    }
}

void TabbedPropertyPanel::switchToPageSettingsPanel()
{
    if (m_pageSettingsPanel) {
        setCurrentWidget(m_pageSettingsPanel);
    }
}

void TabbedPropertyPanel::setLayerManager(LayerManager *layerManager)
{
    // 使用单例模式，忽略传入的参数
    Q_UNUSED(layerManager);
    
    if (!m_layerManager) {
        m_layerManager = LayerManager::instance();
    }
    
    // 更新属性面板的场景引用
    if (m_propertiesPanel) {
        m_propertiesPanel->setScene(m_scene);
    }
    
    // 更新图层面板的场景引用和LayerManager
    if (m_layersPanel) {
        m_layersPanel->setScene(m_scene);
        m_layersPanel->setLayerManager(m_layerManager);
    }
    
    // 更新工具面板的场景引用
    if (m_toolsPanel) {
        m_toolsPanel->setScene(m_scene);
    }
    
    // 更新页面设置面板的场景引用
    if (m_pageSettingsPanel) {
        m_pageSettingsPanel->setScene(m_scene);
    }
}

void TabbedPropertyPanel::setScene(DrawingScene *scene)
{
    if (m_scene == scene) {
        return;
    }
    
    m_scene = scene;
    
    // 更新属性面板的场景引用
    if (m_propertiesPanel) {
        m_propertiesPanel->setScene(m_scene);
    }
    
    // 更新图层面板的场景引用
    if (m_layersPanel) {
        m_layersPanel->setScene(m_scene);
    }
    
    // 更新工具面板的场景引用
    if (m_toolsPanel) {
        m_toolsPanel->setScene(m_scene);
    }
    
    // 更新页面设置面板的场景引用
    if (m_pageSettingsPanel) {
        m_pageSettingsPanel->setScene(m_scene);
    }
}

void TabbedPropertyPanel::setView(DrawingView *view)
{
    m_view = view;
    
    // 更新页面设置面板的视图引用
    if (m_pageSettingsPanel) {
        m_pageSettingsPanel->setView(m_view);
    }
}