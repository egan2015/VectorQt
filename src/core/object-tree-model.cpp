#include "object-tree-model.h"
#include "drawing-layer.h"
#include "drawing-shape.h"
#include "layer-manager.h"
#include "../ui/drawingscene.h"
#include <QDebug>
#include <QMimeData>

ObjectTreeModel::ObjectTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_rootItem(nullptr)
    , m_scene(nullptr)
    , m_layerManager(nullptr)
{
    m_rootItem = new RootTreeItem(); // 根节点是虚拟节点
}

ObjectTreeModel::~ObjectTreeModel()
{
    delete m_rootItem;
}

void ObjectTreeModel::setScene(DrawingScene *scene)
{
    m_scene = scene;
    refreshModel();
}

void ObjectTreeModel::setLayerManager(LayerManager *layerManager)
{
    if (m_layerManager) {
        disconnect(m_layerManager, nullptr, this, nullptr);
    }
    
    m_layerManager = layerManager;
    
    if (m_layerManager) {
        connect(m_layerManager, &LayerManager::layerAdded, this, &ObjectTreeModel::onLayerAdded);
        connect(m_layerManager, &LayerManager::layerRemoved, this, &ObjectTreeModel::onLayerRemoved);
        connect(m_layerManager, &LayerManager::activeLayerChanged, this, &ObjectTreeModel::onActiveLayerChanged);
        connect(m_layerManager, &LayerManager::layerContentChanged, this, &ObjectTreeModel::onLayerContentChanged);
    }
    
    refreshModel();
}

QModelIndex ObjectTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    
    ObjectTreeItem *parentItem;
    
    if (!parent.isValid()) {
        parentItem = m_rootItem;
    } else {
        parentItem = static_cast<ObjectTreeItem*>(parent.internalPointer());
    }
    
    ObjectTreeItem *childItem = parentItem->child(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }
    
    return QModelIndex();
}

QModelIndex ObjectTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }
    
    ObjectTreeItem *childItem = static_cast<ObjectTreeItem*>(index.internalPointer());
    ObjectTreeItem *parentItem = childItem->parent();
    
    if (parentItem == m_rootItem || parentItem == nullptr) {
        return QModelIndex();
    }
    
    return createIndex(parentItem->row(), 0, parentItem);
}

int ObjectTreeModel::rowCount(const QModelIndex &parent) const
{
    ObjectTreeItem *parentItem;
    
    if (!parent.isValid()) {
        parentItem = m_rootItem;
    } else {
        parentItem = static_cast<ObjectTreeItem*>(parent.internalPointer());
    }
    
    return parentItem->childCount();
}

int ObjectTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1; // 只有一列：名称
}

QVariant ObjectTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    
    ObjectTreeItem *item = static_cast<ObjectTreeItem*>(index.internalPointer());
    if (!item) {
        return QVariant();
    }
    
    switch (role) {
        case Qt::DisplayRole:
            return item->name();
            
        case Qt::DecorationRole:
            return item->icon();
            
        case Qt::CheckStateRole:
            return item->isVisible() ? Qt::Checked : Qt::Unchecked;
            
        default:
            return QVariant();
    }
}

bool ObjectTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }
    
    ObjectTreeItem *item = static_cast<ObjectTreeItem*>(index.internalPointer());
    if (!item) {
        return false;
    }
    
    switch (role) {
        case Qt::EditRole:
            item->setName(value.toString());
            emit dataChanged(index, index);
            return true;
            
        case Qt::CheckStateRole:
            item->setVisible(value == Qt::Checked);
            emit dataChanged(index, index);
            return true;
            
        default:
            return false;
    }
}

QVariant ObjectTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0) {
            return "对象";
        }
    }
    
    return QVariant();
}

Qt::ItemFlags ObjectTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    
    ObjectTreeItem *item = static_cast<ObjectTreeItem*>(index.internalPointer());
    if (!item) {
        return defaultFlags;
    }
    
    // 可编辑
    defaultFlags |= Qt::ItemIsEditable;
    
    // 可勾选（显示/隐藏）
    defaultFlags |= Qt::ItemIsUserCheckable;
    
    // 可拖拽
    if (item->isDraggable()) {
        defaultFlags |= Qt::ItemIsDragEnabled;
    }
    
    // 可放置
    if (item->isDropTarget()) {
        defaultFlags |= Qt::ItemIsDropEnabled;
    }
    
    return defaultFlags;
}

Qt::DropActions ObjectTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList ObjectTreeModel::mimeTypes() const
{
    QStringList types;
    types << "application/x-objecttreeitem";
    return types;
}

bool ObjectTreeModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(action)
    Q_UNUSED(row)
    Q_UNUSED(column)
    
    if (!data || !data->hasFormat("application/x-objecttreeitem")) {
        return false;
    }
    
    if (!parent.isValid()) {
        return false;
    }
    
    ObjectTreeItem *parentItem = static_cast<ObjectTreeItem*>(parent.internalPointer());
    return parentItem && parentItem->isDropTarget();
}

bool ObjectTreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(action)
    Q_UNUSED(column)
    
    if (!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }
    
    // TODO: 实现拖拽功能
    return false;
}

ObjectTreeItem* ObjectTreeModel::itemFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    
    return static_cast<ObjectTreeItem*>(index.internalPointer());
}

QModelIndex ObjectTreeModel::indexFromItem(ObjectTreeItem *item) const
{
    if (!item || item == m_rootItem) {
        return QModelIndex();
    }
    
    return createIndex(item->row(), 0, item);
}

void ObjectTreeModel::refreshModel()
{
    beginResetModel();
    clearTree();
    buildTree();
    endResetModel();
}

void ObjectTreeModel::onLayerAdded(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    beginInsertRows(QModelIndex(), m_rootItem->childCount(), m_rootItem->childCount());
    new LayerTreeItem(layer, m_rootItem);
    endInsertRows();
}

void ObjectTreeModel::onLayerRemoved(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    // 查找对应的树项
    for (int i = 0; i < m_rootItem->childCount(); ++i) {
        LayerTreeItem *layerItem = static_cast<LayerTreeItem*>(m_rootItem->child(i));
        if (layerItem && layerItem->layer() == layer) {
            beginRemoveRows(QModelIndex(), i, i);
            m_rootItem->takeChild(i);
            endRemoveRows();
            break;
        }
    }
}

void ObjectTreeModel::onActiveLayerChanged(DrawingLayer *layer)
{
    Q_UNUSED(layer)
    // TODO: 更新活动图层的显示
}

void ObjectTreeModel::onLayerContentChanged(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    // 查找对应的树项
    for (int i = 0; i < m_rootItem->childCount(); ++i) {
        LayerTreeItem *layerItem = static_cast<LayerTreeItem*>(m_rootItem->child(i));
        if (layerItem && layerItem->layer() == layer) {
            // 移除旧的图形项
            while (layerItem->childCount() > 0) {
                delete layerItem->takeChild(0);
            }
            
            // 添加新的图形项
            QList<DrawingShape*> shapes = layer->shapes();
            for (int j = 0; j < shapes.count(); ++j) {
                beginInsertRows(indexFromItem(layerItem), j, j);
                new ShapeTreeItem(shapes[j], layerItem);
                endInsertRows();
            }
            
            break;
        }
    }
}

void ObjectTreeModel::buildTree()
{
    if (!m_layerManager) {
        return;
    }
    
    // 为每个图层创建树项
    QList<DrawingLayer*> layers = m_layerManager->layers();
    for (DrawingLayer *layer : layers) {
        LayerTreeItem *layerItem = new LayerTreeItem(layer, m_rootItem);
        
        // 为图层中的每个图形创建树项
        QList<DrawingShape*> shapes = layer->shapes();
        for (DrawingShape *shape : shapes) {
            new ShapeTreeItem(shape, layerItem);
        }
    }
}

void ObjectTreeModel::clearTree()
{
    while (m_rootItem->childCount() > 0) {
        delete m_rootItem->takeChild(0);
    }
}