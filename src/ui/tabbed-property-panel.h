#ifndef TABBED_PROPERTY_PANEL_H
#define TABBED_PROPERTY_PANEL_H

#include <QTabWidget>
#include <QWidget>

class DrawingScene;
class LayerPanel;
class LayerManager;
class PropertyPanel;
class ToolsPanel;
class PageSettingsPanel;

/**
 * Tab属性面板 - 使用TabBar布局组织不同功能面板
 */
class TabbedPropertyPanel : public QTabWidget
{
    Q_OBJECT

public:
    explicit TabbedPropertyPanel(QWidget *parent = nullptr);
    
    // 添加面板
    void addPropertiesPanel();
    void addLayersPanel();
    void addToolsPanel();
    void addPageSettingsPanel();
    
    // 获取特定面板
    PropertyPanel* getPropertiesPanel() const { return m_propertiesPanel; }
    LayerPanel* getLayersPanel() const { return m_layersPanel; }
    ToolsPanel* getToolsPanel() const { return m_toolsPanel; }
    PageSettingsPanel* getPageSettingsPanel() const { return m_pageSettingsPanel; }
    
    // 当前面板访问
    QWidget* getCurrentPanel() const;
    int getCurrentIndex() const { return currentIndex(); }
    
    // 面板切换
    void switchToPropertiesPanel();
    void switchToLayersPanel();
    void switchToToolsPanel();
    void switchToPageSettingsPanel();
    
    // 设置场景
    void setScene(DrawingScene *scene);
    void setView(class DrawingView *view);
    void setLayerManager(LayerManager *layerManager);

signals:
    void currentPanelChanged(int index);

private:
    // 面板实例
    PropertyPanel *m_propertiesPanel;
    LayerPanel *m_layersPanel;
    ToolsPanel *m_toolsPanel;
    PageSettingsPanel *m_pageSettingsPanel;
    
    // 图层管理器引用
    DrawingScene *m_scene;
    class DrawingView *m_view;
    LayerManager *m_layerManager;
};

#endif // TABBED_PROPERTY_PANEL_H