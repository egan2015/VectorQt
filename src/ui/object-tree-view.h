#ifndef OBJECT_TREE_VIEW_H
#define OBJECT_TREE_VIEW_H

#include <QTreeView>
#include <QModelIndex>

class ObjectTreeModel;
class DrawingScene;
class DrawingLayer;
class DrawingShape;
class LayerManager;

/**
 * 对象树视图 - 显示图层与对象的树形结构
 */
class ObjectTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit ObjectTreeView(QWidget *parent = nullptr);
    ~ObjectTreeView();
    
    // 设置场景和图层管理器
    void setScene(DrawingScene *scene);
    void setLayerManager(LayerManager *layerManager);
    
    // 获取当前选中的对象
    DrawingShape* selectedShape() const;
    DrawingLayer* selectedLayer() const;
    
    // 选择操作
    void selectShape(DrawingShape *shape);
    void selectLayer(DrawingLayer *layer);
    void clearSelection();

signals:
    void shapeSelected(DrawingShape *shape);
    void layerSelected(DrawingLayer *layer);
    void selectionChanged();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSelectionChanged();
    void onItemDoubleClicked(const QModelIndex &index);

private:
    void setupContextMenu();
    void updateSelection();
    
    ObjectTreeModel *m_model;
    DrawingScene *m_scene;
    LayerManager *m_layerManager;
};

#endif // OBJECT_TREE_VIEW_H