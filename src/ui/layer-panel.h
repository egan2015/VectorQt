#ifndef LAYER_PANEL_H
#define LAYER_PANEL_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QToolBar>
#include <QAction>

class DrawingScene;
class DrawingLayer;
class LayerManager;

/**
 * 图层管理面板
 */
class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(QWidget *parent = nullptr);
    
    // 设置场景和图层管理器
    void setScene(DrawingScene *scene);
    void setLayerManager(LayerManager *layerManager);
    
    // 公共接口，供LayerManager调用
    void updateLayerList();
    
    // 图层操作
    void addLayer(const QString &name = QString());
    void deleteCurrentLayer();
    void moveLayerUp();
    void moveLayerDown();
    void duplicateLayer();
    void mergeLayerDown();
    void toggleLayerVisibility(int index);
    void toggleLayerLock(int index);
    void renameLayer(int index);
    void selectLayer(int index);

signals:
    void layerSelected(DrawingLayer *layer);
    void layerChanged();

private slots:
    void onAddLayer();
    void onDeleteLayer();
    void onMoveLayerUp();
    void onMoveLayerDown();
    void onDuplicateLayer();
    void onMergeLayerDown();
    void onLayerItemChanged(QListWidgetItem *item);
    void onLayerItemClicked(QListWidgetItem *item);
    void onLayerItemDoubleClicked(QListWidgetItem *item);

private:
    void setupUI();
    void updateLayerButtons();
    QListWidgetItem* createLayerItem(DrawingLayer *layer, int index);
    
    DrawingScene *m_scene;
    LayerManager *m_layerManager;
    QListWidget *m_layerList;
    QLabel *m_layerCountLabel;
    
    // 工具栏按钮
    QAction *m_addLayerAction;
    QAction *m_deleteLayerAction;
    QAction *m_moveUpAction;
    QAction *m_moveDownAction;
    QAction *m_duplicateAction;
    QAction *m_mergeAction;
};

#endif // LAYER_PANEL_H