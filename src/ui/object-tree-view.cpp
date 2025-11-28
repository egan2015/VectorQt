#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include "object-tree-view.h"
#include "drawingscene.h"
#include "../core/object-tree-model.h"
#include "../core/object-tree-item.h"
#include "../core/drawing-layer.h"
#include "../core/drawing-shape.h"
#include "../core/layer-manager.h"

ObjectTreeView::ObjectTreeView(QWidget *parent)
    : QTreeView(parent)
    , m_model(nullptr)
    , m_scene(nullptr)
    , m_layerManager(nullptr)
{
    m_model = new ObjectTreeModel(this);
    setModel(m_model);
    
    // 设置视图属性
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    
    // 连接信号
    connect(selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ObjectTreeView::onSelectionChanged);
    connect(this, &QTreeView::doubleClicked, this, &ObjectTreeView::onItemDoubleClicked);
}

ObjectTreeView::~ObjectTreeView()
{
}

void ObjectTreeView::setScene(DrawingScene *scene)
{
    m_scene = scene;
    m_model->setScene(scene);
}

void ObjectTreeView::setLayerManager(LayerManager *layerManager)
{
    m_layerManager = layerManager;
    m_model->setLayerManager(layerManager);
}

DrawingShape* ObjectTreeView::selectedShape() const
{
    QModelIndexList indexes = selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return nullptr;
    }
    
    ObjectTreeItem *item = m_model->itemFromIndex(indexes.first());
    if (item && item->itemType() == ObjectTreeItem::ShapeItem) {
        return item->shape();
    }
    
    return nullptr;
}

DrawingLayer* ObjectTreeView::selectedLayer() const
{
    QModelIndexList indexes = selectionModel()->selectedIndexes();
    if (indexes.isEmpty()) {
        return nullptr;
    }
    
    ObjectTreeItem *item = m_model->itemFromIndex(indexes.first());
    if (item && item->itemType() == ObjectTreeItem::LayerItem) {
        return item->layer();
    }
    
    return nullptr;
}

void ObjectTreeView::selectShape(DrawingShape *shape)
{
    if (!shape) {
        return;
    }
    
    // 遍历所有图层项，查找对应的图形项
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex layerIndex = m_model->index(i, 0);
        ObjectTreeItem *layerItem = m_model->itemFromIndex(layerIndex);
        
        if (layerItem) {
            for (int j = 0; j < layerItem->childCount(); ++j) {
                ObjectTreeItem *shapeItem = layerItem->child(j);
                if (shapeItem && shapeItem->shape() == shape) {
                    QModelIndex shapeIndex = m_model->index(j, 0, layerIndex);
                    selectionModel()->select(shapeIndex, QItemSelectionModel::ClearAndSelect);
                    scrollTo(shapeIndex);
                    return;
                }
            }
        }
    }
}

void ObjectTreeView::selectLayer(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex layerIndex = m_model->index(i, 0);
        ObjectTreeItem *layerItem = m_model->itemFromIndex(layerIndex);
        
        if (layerItem && layerItem->layer() == layer) {
            selectionModel()->select(layerIndex, QItemSelectionModel::ClearAndSelect);
            scrollTo(layerIndex);
            return;
        }
    }
}

void ObjectTreeView::clearSelection()
{
    selectionModel()->clearSelection();
}

void ObjectTreeView::contextMenuEvent(QContextMenuEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        return;
    }
    
    ObjectTreeItem *item = m_model->itemFromIndex(index);
    if (!item) {
        return;
    }
    
    QMenu contextMenu(this);
    
    if (item->itemType() == ObjectTreeItem::LayerItem) {
        // 图层右键菜单
        QAction *renameAction = contextMenu.addAction("重命名图层");
        QAction *deleteAction = contextMenu.addAction("删除图层");
        QAction *duplicateAction = contextMenu.addAction("复制图层");
        
        QAction *selectedAction = contextMenu.exec(event->globalPos());
        
        if (selectedAction == renameAction) {
            bool ok;
            QString newName = QInputDialog::getText(this, "重命名图层", 
                                                   "新名称:", QLineEdit::Normal,
                                                   item->name(), &ok);
            if (ok && !newName.isEmpty()) {
                item->setName(newName);
            }
        } else if (selectedAction == deleteAction) {
            // TODO: 实现删除图层功能
        } else if (selectedAction == duplicateAction) {
            // TODO: 实现复制图层功能
        }
    } else if (item->itemType() == ObjectTreeItem::ShapeItem) {
        // 图形右键菜单
        QAction *renameAction = contextMenu.addAction("重命名");
        QAction *deleteAction = contextMenu.addAction("删除");
        QAction *duplicateAction = contextMenu.addAction("复制");
        QAction *moveToTopAction = contextMenu.addAction("移至顶层");
        QAction *moveToBottomAction = contextMenu.addAction("移至底层");
        
        QAction *selectedAction = contextMenu.exec(event->globalPos());
        
        if (selectedAction == renameAction) {
            bool ok;
            QString newName = QInputDialog::getText(this, "重命名", 
                                                   "新名称:", QLineEdit::Normal,
                                                   item->name(), &ok);
            if (ok && !newName.isEmpty()) {
                item->setName(newName);
            }
        } else if (selectedAction == deleteAction) {
            // TODO: 实现删除图形功能
        } else if (selectedAction == duplicateAction) {
            // TODO: 实现复制图形功能
        } else if (selectedAction == moveToTopAction) {
            // TODO: 实现移至顶层功能
        } else if (selectedAction == moveToBottomAction) {
            // TODO: 实现移至底层功能
        }
    }
}

void ObjectTreeView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        return;
    }
    
    ObjectTreeItem *item = m_model->itemFromIndex(index);
    if (!item) {
        return;
    }
    
    // 双击编辑名称
    edit(index);
}

void ObjectTreeView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        // 删除选中的对象
        QModelIndexList indexes = selectionModel()->selectedIndexes();
        for (const QModelIndex &index : indexes) {
            ObjectTreeItem *item = m_model->itemFromIndex(index);
            if (item) {
                // TODO: 实现删除功能
            }
        }
    } else if (event->key() == Qt::Key_F2) {
        // 重命名选中的对象
        QModelIndexList indexes = selectionModel()->selectedIndexes();
        if (!indexes.isEmpty()) {
            edit(indexes.first());
        }
    } else {
        QTreeView::keyPressEvent(event);
    }
}

void ObjectTreeView::onSelectionChanged()
{
    updateSelection();
}

void ObjectTreeView::onItemDoubleClicked(const QModelIndex &index)
{
    ObjectTreeItem *item = m_model->itemFromIndex(index);
    if (!item) {
        return;
    }
    
    // 双击定位到场景中的对象
    if (item->itemType() == ObjectTreeItem::ShapeItem && m_scene) {
        DrawingShape *shape = item->shape();
        if (shape) {
            // 清除当前选择
            m_scene->clearSelection();
            
            // 选择并聚焦到该图形
            shape->setSelected(true);
            
            // TODO: 居中显示该图形
            // fitInView(sceneBounds, Qt::KeepAspectRatio);
        }
    }
}

void ObjectTreeView::setupContextMenu()
{
    // TODO: 设置右键菜单
}

void ObjectTreeView::updateSelection()
{
    DrawingShape *shape = selectedShape();
    DrawingLayer *layer = selectedLayer();
    
    if (shape) {
        emit shapeSelected(shape);
    } else if (layer) {
        emit layerSelected(layer);
    }
    
    emit selectionChanged();
}