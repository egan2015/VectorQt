#ifndef LAYER_PANEL_H
#define LAYER_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QTreeWidget>
#include <QTreeWidgetItem>

class DrawingScene;
class DrawingLayer;
class DrawingGroup;
class DrawingShape;
class LayerManager;
class ObjectTreeView;
class ObjectTreeModel;

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
    void onLayerItemChanged(QTreeWidgetItem *item, int column);
    void onLayerItemClicked(QTreeWidgetItem *item, int column);
    void onLayerItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onLayerAdded();

private:
    void setupUI();
    void updateLayerButtons();
    QTreeWidgetItem* createLayerItem(DrawingLayer *layer, int index);
    void populateLayerTree();
    void addObjectsToLayerItem(QTreeWidgetItem *layerItem, DrawingLayer *layer);
    void addGroupAsShapeItem(QTreeWidgetItem *parentItem, DrawingGroup *group);
    void addGroupChildrenToShapeItem(QTreeWidgetItem *groupItem, DrawingGroup *group);
    QString getShapeName(DrawingShape *shape) const;
    QString getItemPath(QTreeWidgetItem *item) const;
    QTreeWidgetItem* findItemByPath(const QString &path) const;
    
    DrawingScene *m_scene;
    LayerManager *m_layerManager;
    QTreeWidget *m_layerTree;
    QLabel *m_layerCountLabel;
    ObjectTreeModel *m_objectTreeModel;
    
    // 工具栏按钮
    QAction *m_addLayerAction;
    QAction *m_deleteLayerAction;
    QAction *m_moveUpAction;
    QAction *m_moveDownAction;
    QAction *m_duplicateAction;
    QAction *m_mergeAction;
};

#endif // LAYER_PANEL_H