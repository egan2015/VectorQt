#ifndef OBJECT_TREE_MODEL_H
#define OBJECT_TREE_MODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include "object-tree-item.h"

class DrawingScene;
class DrawingLayer;
class DrawingShape;
class LayerManager;

/**
 * 对象树模型 - 为对象树提供数据模型
 */
class ObjectTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit ObjectTreeModel(QObject *parent = nullptr);
    ~ObjectTreeModel();
    
    // 设置场景和图层管理器
    void setScene(DrawingScene *scene);
    void setLayerManager(LayerManager *layerManager);

    // QAbstractItemModel 接口
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    
    // 拖拽支持
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    
    // 获取对象树项
    ObjectTreeItem* itemFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromItem(ObjectTreeItem *item) const;

public slots:
    void refreshModel();
    void onLayerAdded(DrawingLayer *layer);
    void onLayerRemoved(DrawingLayer *layer);
    void onActiveLayerChanged(DrawingLayer *layer);

private:
    void buildTree();
    void clearTree();
    
    ObjectTreeItem *m_rootItem;
    DrawingScene *m_scene;
    LayerManager *m_layerManager;
};

#endif // OBJECT_TREE_MODEL_H