#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QToolBar>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QIcon>
#include "../ui/layer-panel.h"
#include "../ui/drawingscene.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"
#include "../core/object-tree-model.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-group.h"

LayerPanel::LayerPanel(QWidget *parent)
    : QWidget(parent)
    , m_scene(nullptr)
    , m_layerManager(nullptr)
    , m_layerTree(nullptr)
    , m_layerCountLabel(nullptr)
    , m_objectTreeModel(nullptr)
    , m_addLayerAction(nullptr)
    , m_deleteLayerAction(nullptr)
    , m_moveUpAction(nullptr)
    , m_moveDownAction(nullptr)
    , m_duplicateAction(nullptr)
    , m_mergeAction(nullptr)
{
    setupUI();
}

void LayerPanel::setScene(DrawingScene *scene)
{
    if (m_scene == scene) {
        return;
    }
    
    m_scene = scene;
    updateLayerList();
}

void LayerPanel::setLayerManager(LayerManager *layerManager)
{
    if (m_layerManager == layerManager) {
        return;
    }
    
    // 断开之前的连接
    if (m_layerManager) {
        disconnect(m_layerManager, nullptr, this, nullptr);
    }
    
    m_layerManager = layerManager;
    
    // 创建ObjectTreeModel
    if (m_layerManager && !m_objectTreeModel) {
        m_objectTreeModel = new ObjectTreeModel(this);
        m_objectTreeModel->setLayerManager(m_layerManager);
    }
    
    // 连接LayerManager的信号
    if (m_layerManager) {
        connect(m_layerManager, &LayerManager::layerAdded, this, &LayerPanel::updateLayerList);
        connect(m_layerManager, &LayerManager::layerRemoved, this, &LayerPanel::updateLayerList);
        connect(m_layerManager, &LayerManager::layerMoved, this, &LayerPanel::updateLayerList);
        connect(m_layerManager, &LayerManager::layerChanged, this, &LayerPanel::updateLayerList);
        connect(m_layerManager, &LayerManager::activeLayerChanged, this, &LayerPanel::updateLayerList);
    }
    
    updateLayerList();
}

void LayerPanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    
    // 创建工具栏
    QToolBar *toolBar = new QToolBar(this);
    toolBar->setOrientation(Qt::Horizontal);
    toolBar->setIconSize(QSize(16, 16));
    
    // 添加工具栏动作
    m_addLayerAction = new QAction(tr("添加图层"), this);
    m_addLayerAction->setIcon(QIcon(":/icons/add-layer.png"));
    m_addLayerAction->setToolTip(tr("添加新图层"));
    connect(m_addLayerAction, &QAction::triggered, this, &LayerPanel::onAddLayer);
    
    m_deleteLayerAction = new QAction(tr("删除图层"), this);
    m_deleteLayerAction->setIcon(QIcon(":/icons/delete-layer.png"));
    m_deleteLayerAction->setToolTip(tr("删除当前图层"));
    connect(m_deleteLayerAction, &QAction::triggered, this, &LayerPanel::onDeleteLayer);
    
    m_moveUpAction = new QAction(tr("上移"), this);
    m_moveUpAction->setIcon(QIcon(":/icons/move-up.png"));
    m_moveUpAction->setToolTip(tr("将图层上移"));
    connect(m_moveUpAction, &QAction::triggered, this, &LayerPanel::onMoveLayerUp);
    
    m_moveDownAction = new QAction(tr("下移"), this);
    m_moveDownAction->setIcon(QIcon(":/icons/move-down.png"));
    m_moveDownAction->setToolTip(tr("将图层下移"));
    connect(m_moveDownAction, &QAction::triggered, this, &LayerPanel::onMoveLayerDown);
    
    m_duplicateAction = new QAction(tr("复制"), this);
    m_duplicateAction->setIcon(QIcon(":/icons/duplicate-layer.png"));
    m_duplicateAction->setToolTip(tr("复制当前图层"));
    connect(m_duplicateAction, &QAction::triggered, this, &LayerPanel::onDuplicateLayer);
    
    m_mergeAction = new QAction(tr("合并"), this);
    m_mergeAction->setIcon(QIcon(":/icons/merge-layer.png"));
    m_mergeAction->setToolTip(tr("向下合并图层"));
    connect(m_mergeAction, &QAction::triggered, this, &LayerPanel::onMergeLayerDown);
    
    toolBar->addAction(m_addLayerAction);
    toolBar->addAction(m_deleteLayerAction);
    toolBar->addSeparator();
    toolBar->addAction(m_moveUpAction);
    toolBar->addAction(m_moveDownAction);
    toolBar->addSeparator();
    toolBar->addAction(m_duplicateAction);
    toolBar->addAction(m_mergeAction);
    
    mainLayout->addWidget(toolBar);
    
    // 创建图层树
    m_layerTree = new QTreeWidget(this);
    m_layerTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerTree->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerTree->setDefaultDropAction(Qt::MoveAction);
    m_layerTree->setHeaderHidden(true);
    
    // 设置列
    m_layerTree->setColumnCount(2);
    m_layerTree->setColumnWidth(0, 150);  // 名称列
    m_layerTree->setColumnWidth(1, 30);   // 可见性列
    
    connect(m_layerTree, &QTreeWidget::itemChanged, this, &LayerPanel::onLayerItemChanged);
    connect(m_layerTree, &QTreeWidget::itemClicked, this, &LayerPanel::onLayerItemClicked);
    connect(m_layerTree, &QTreeWidget::itemDoubleClicked, this, &LayerPanel::onLayerItemDoubleClicked);
    
    mainLayout->addWidget(m_layerTree);
    
    // 创建图层计数标签
    m_layerCountLabel = new QLabel(tr("图层数量: 0"), this);
    m_layerCountLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_layerCountLabel);
    
    // 初始更新按钮状态
    updateLayerButtons();
}

void LayerPanel::updateLayerList()
{
    populateLayerTree();
}

void LayerPanel::populateLayerTree()
{
    qDebug() << "LayerPanel::populateLayerTree called";
    
    if (!m_layerTree) {
        qDebug() << "No layer tree widget";
        return;
    }
    
    // 保存当前的展开状态
    QStringList expandedLayers;
    QTreeWidgetItem *currentItem = m_layerTree->currentItem();
    QString currentPath;
    
    for (int i = 0; i < m_layerTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_layerTree->topLevelItem(i);
        if (item && item->isExpanded()) {
            expandedLayers.append(item->text(0));
        }
    }
    
    // 保存当前选中项的路径
    if (currentItem) {
        currentPath = getItemPath(currentItem);
    }
    
    m_layerTree->clear();
    
    if (!m_layerManager) {
        qDebug() << "No layer manager in populateLayerTree";
        m_layerCountLabel->setText(tr("图层数量: 0"));
        updateLayerButtons();
        return;
    }
    
    // 从LayerManager获取真实的图层列表
    QList<DrawingLayer*> layers = m_layerManager->layers();
    qDebug() << "Got" << layers.count() << "layers from manager";
    
    for (int i = 0; i < layers.count(); ++i) {
        DrawingLayer *layer = layers.at(i);
        if (!layer) {
            qDebug() << "Layer at index" << i << "is null";
            continue;
        }
        
        QString layerName = layer->name();
        qDebug() << "Adding layer item:" << layerName;
        
        QTreeWidgetItem *layerItem = new QTreeWidgetItem();
        layerItem->setText(0, layerName);
        layerItem->setFlags(layerItem->flags() | Qt::ItemIsUserCheckable);
        layerItem->setCheckState(1, layer->isVisible() ? Qt::Checked : Qt::Unchecked);
        
        // 设置用户数据，存储图层索引
        layerItem->setData(0, Qt::UserRole, i);
        layerItem->setData(0, Qt::UserRole + 1, "layer");  // 标识为图层类型
        
        // 如果是活动图层，高亮显示
        if (layer == m_layerManager->activeLayer()) {
            QFont font = layerItem->font(0);
            font.setBold(true);
            layerItem->setFont(0, font);
        }
        
        // 添加图层下的对象 - 使用try-catch防止崩溃
        try {
            addObjectsToLayerItem(layerItem, layer);
        } catch (...) {
            qDebug() << "Error adding objects to layer item for layer:" << layerName;
        }
        
        // 恢复展开状态
        if (expandedLayers.contains(layerName)) {
            layerItem->setExpanded(true);
        }
        
        m_layerTree->addTopLevelItem(layerItem);
        qDebug() << "Layer item added to tree";
    }
    
    m_layerCountLabel->setText(tr("图层数量: %1").arg(layers.count()));
    
    // 恢复当前选中项
    if (!currentPath.isEmpty()) {
        QTreeWidgetItem *itemToSelect = findItemByPath(currentPath);
        if (itemToSelect) {
            m_layerTree->setCurrentItem(itemToSelect);
        }
    } else {
        // 选择活动图层
        int activeIndex = m_layerManager->activeLayerIndex();
        if (activeIndex >= 0 && activeIndex < m_layerTree->topLevelItemCount()) {
            m_layerTree->setCurrentItem(m_layerTree->topLevelItem(activeIndex));
        }
    }
    
    updateLayerButtons();
    
    // 强制刷新UI
    m_layerTree->update();
    update();
}

QString LayerPanel::getItemPath(QTreeWidgetItem *item) const
{
    if (!item) {
        return QString();
    }
    
    QStringList path;
    QTreeWidgetItem *current = item;
    while (current) {
        path.prepend(current->text(0));
        current = current->parent();
    }
    return path.join("/");
}

QTreeWidgetItem* LayerPanel::findItemByPath(const QString &path) const
{
    if (!m_layerTree || path.isEmpty()) {
        return nullptr;
    }
    
    QStringList parts = path.split("/");
    QTreeWidgetItem *current = nullptr;
    
    // 查找顶级图层项
    for (int i = 0; i < m_layerTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_layerTree->topLevelItem(i);
        if (item && item->text(0) == parts.first()) {
            current = item;
            break;
        }
    }
    
    // 查找子项
    if (current && parts.size() > 1) {
        for (int i = 1; i < parts.size(); ++i) {
            bool found = false;
            for (int j = 0; j < current->childCount(); ++j) {
                QTreeWidgetItem *child = current->child(j);
                if (child && child->text(0) == parts[i]) {
                    current = child;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return nullptr;
            }
        }
    }
    
    return current;
}

void LayerPanel::addObjectsToLayerItem(QTreeWidgetItem *layerItem, DrawingLayer *layer)
{
    if (!layerItem || !layer) {
        return;
    }
    
    // 获取图层中的所有对象
    QList<DrawingShape*> shapes = layer->shapes();
    qDebug() << "Processing layer with" << shapes.count() << "shapes";
    
    for (int i = 0; i < shapes.count(); ++i) {
        DrawingShape *shape = shapes.at(i);
        
        if (shape) {
            try {
                QTreeWidgetItem *shapeItem = new QTreeWidgetItem(layerItem);
                QString shapeName = getShapeName(shape);
                shapeItem->setText(0, shapeName);
                shapeItem->setFlags(shapeItem->flags() | Qt::ItemIsUserCheckable);
                shapeItem->setCheckState(1, shape->isVisible() ? Qt::Checked : Qt::Unchecked);
                
                // 设置用户数据，存储形状指针 - 使用quintptr避免指针转换问题
            shapeItem->setData(0, Qt::UserRole, reinterpret_cast<quintptr>(shape));
            shapeItem->setData(0, Qt::UserRole + 1, "shape");  // 标识为形状类型
                
                qDebug() << "Added shape item:" << shapeName;
                
                // 如果是组对象，递归添加子对象
            DrawingGroup *group = dynamic_cast<DrawingGroup*>(shape);
            if (group) {
                addGroupChildrenToShapeItem(shapeItem, group);
            }
            } catch (...) {
                qDebug() << "Error processing shape at index" << i;
            }
        } else {
            qDebug() << "Null shape at index" << i;
        }
    }
}

QString LayerPanel::getShapeName(DrawingShape *shape) const
{
    if (!shape) {
        return tr("未知对象");
    }
    
    try {
        // 根据形状类型返回中文名称
        switch (shape->shapeType()) {
            case DrawingShape::Rectangle:
                return tr("矩形");
            case DrawingShape::Ellipse:
                return tr("椭圆");
            case DrawingShape::Path:
                return tr("路径");
            case DrawingShape::Line:
                return tr("直线");
            case DrawingShape::Polyline:
                return tr("折线");
            case DrawingShape::Polygon:
                return tr("多边形");
            case DrawingShape::Text:
                return tr("文本");
            case DrawingShape::Group:
                return tr("组");
            default:
                return tr("对象");
        }
    } catch (...) {
        qDebug() << "Error getting shape name";
        return tr("对象");
    }
}

void LayerPanel::addGroupChildrenToShapeItem(QTreeWidgetItem *shapeItem, DrawingGroup *group)
{
    if (!shapeItem || !group) {
        return;
    }
    
    QList<DrawingShape*> children = group->items();
    qDebug() << "Processing group with" << children.count() << "children";
    
    for (int i = 0; i < children.count(); ++i) {
        DrawingShape *shape = children.at(i);
        
        if (shape) {
            try {
                QTreeWidgetItem *childItem = new QTreeWidgetItem(shapeItem);
                QString shapeName = getShapeName(shape);
                childItem->setText(0, shapeName);
                childItem->setFlags(childItem->flags() | Qt::ItemIsUserCheckable);
                childItem->setCheckState(1, shape->isVisible() ? Qt::Checked : Qt::Unchecked);
                
                // 设置用户数据，存储形状指针 - 使用quintptr避免指针转换问题
            childItem->setData(0, Qt::UserRole, reinterpret_cast<quintptr>(shape));
            childItem->setData(0, Qt::UserRole + 1, "shape");  // 标识为形状类型
                
                qDebug() << "Added group child item:" << shapeName;
                
                // 递归处理嵌套组
                DrawingGroup *childGroup = dynamic_cast<DrawingGroup*>(shape);
                if (childGroup) {
                    addGroupChildrenToShapeItem(childItem, childGroup);
                }
            } catch (...) {
                qDebug() << "Error processing group child at index" << i;
            }
        } else {
            qDebug() << "Null group child at index" << i;
        }
    }
}

void LayerPanel::updateLayerButtons()
{
    bool hasScene = (m_scene != nullptr);
    bool hasSelection = m_layerTree && m_layerTree->currentItem() != nullptr;
    int currentIndex = -1;
    int layerCount = m_layerTree ? m_layerTree->topLevelItemCount() : 0;
    
    // 获取当前选中的图层索引
    if (m_layerTree && m_layerTree->currentItem()) {
        QTreeWidgetItem *currentItem = m_layerTree->currentItem();
        // 如果是图层项
        if (currentItem->data(0, Qt::UserRole + 1).toString() == "layer") {
            currentIndex = m_layerTree->indexOfTopLevelItem(currentItem);
        }
        // 如果是形状项，找到其父图层
        else if (currentItem->parent()) {
            currentIndex = m_layerTree->indexOfTopLevelItem(currentItem->parent());
        }
    }
    
    m_addLayerAction->setEnabled(hasScene);
    m_deleteLayerAction->setEnabled(hasSelection && layerCount > 1);
    m_moveUpAction->setEnabled(hasSelection && currentIndex > 0);
    m_moveDownAction->setEnabled(hasSelection && currentIndex >= 0 && currentIndex < layerCount - 1);
    m_duplicateAction->setEnabled(hasSelection);
    m_mergeAction->setEnabled(hasSelection && currentIndex > 0);
}

void LayerPanel::onAddLayer()
{
    qDebug() << "LayerPanel::onAddLayer called";
    
    // 使用单例模式获取LayerManager
    if (!m_layerManager) {
        m_layerManager = LayerManager::instance();
    }
    
    if (!m_layerManager) {
        qDebug() << "No layer manager available";
        return;
    }
    
    bool ok;
    QString layerName = QInputDialog::getText(this, tr("添加图层"), 
                                              tr("图层名称:"), QLineEdit::Normal,
                                              tr("新图层"), &ok);
    
    qDebug() << "Dialog result - ok:" << ok << "layerName:" << layerName;
    
    if (ok && !layerName.isEmpty()) {
        qDebug() << "Calling LayerManager::createLayer";
        m_layerManager->createLayer(layerName);
    } else {
        qDebug() << "User cancelled or empty name";
    }
}

void LayerPanel::onDeleteLayer()
{
    if (!m_layerManager || !m_layerTree || !m_layerTree->currentItem()) {
        return;
    }
    
    int ret = QMessageBox::question(this, tr("删除图层"),
                                   tr("确定要删除当前图层吗？"),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // 获取当前选中的图层索引
        QTreeWidgetItem *currentItem = m_layerTree->currentItem();
        int currentIndex = -1;
        
        // 如果是图层项
        if (currentItem->data(0, Qt::UserRole + 1).toString() == "layer") {
            currentIndex = m_layerTree->indexOfTopLevelItem(currentItem);
        }
        // 如果是形状项，找到其父图层
        else if (currentItem->parent()) {
            currentIndex = m_layerTree->indexOfTopLevelItem(currentItem->parent());
        }
        
        if (currentIndex >= 0) {
            m_layerManager->deleteLayer(currentIndex);
        }
    }
}

void LayerPanel::onMoveLayerUp()
{
    if (!m_layerManager) {
        return;
    }
    
    // 获取当前选中的图层索引
    QTreeWidgetItem *currentItem = m_layerTree->currentItem();
    int currentIndex = -1;
    
    // 如果是图层项
    if (currentItem->data(0, Qt::UserRole + 1).toString() == "layer") {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem);
    }
    // 如果是形状项，找到其父图层
    else if (currentItem->parent()) {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem->parent());
    }
    
    if (currentIndex > 0) {
        m_layerManager->moveLayerUp(currentIndex);
    }
}

void LayerPanel::onMoveLayerDown()
{
    if (!m_layerManager) {
        return;
    }
    
    // 获取当前选中的图层索引
    QTreeWidgetItem *currentItem = m_layerTree->currentItem();
    int currentIndex = -1;
    
    // 如果是图层项
    if (currentItem->data(0, Qt::UserRole + 1).toString() == "layer") {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem);
    }
    // 如果是形状项，找到其父图层
    else if (currentItem->parent()) {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem->parent());
    }
    
    if (currentIndex >= 0 && currentIndex < m_layerTree->topLevelItemCount() - 1) {
        m_layerManager->moveLayerDown(currentIndex);
    }
}

void LayerPanel::onDuplicateLayer()
{
    if (!m_layerManager) {
        return;
    }
    
    // 获取当前选中的图层索引
    QTreeWidgetItem *currentItem = m_layerTree->currentItem();
    int currentIndex = -1;
    
    // 如果是图层项
    if (currentItem->data(0, Qt::UserRole + 1).toString() == "layer") {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem);
    }
    // 如果是形状项，找到其父图层
    else if (currentItem->parent()) {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem->parent());
    }
    
    if (currentIndex >= 0) {
        m_layerManager->duplicateLayer(currentIndex);
    }
}

void LayerPanel::onMergeLayerDown()
{
    if (!m_layerManager) {
        return;
    }
    
    // 获取当前选中的图层索引
    QTreeWidgetItem *currentItem = m_layerTree->currentItem();
    int currentIndex = -1;
    
    // 如果是图层项
    if (currentItem->data(0, Qt::UserRole + 1).toString() == "layer") {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem);
    }
    // 如果是形状项，找到其父图层
    else if (currentItem->parent()) {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem->parent());
    }
    
    if (currentIndex <= 0) {
        return;
    }
    
    int ret = QMessageBox::question(this, tr("合并图层"),
                                   tr("确定要将当前图层向下合并吗？"),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        m_layerManager->mergeLayerDown(currentIndex);
    }
}

void LayerPanel::onLayerItemChanged(QTreeWidgetItem *item, int column)
{
    if (!item || !m_layerManager) {
        return;
    }
    
    // 只处理可见性列的变化
    if (column != 1) {
        return;
    }
    
    QString itemType = item->data(0, Qt::UserRole + 1).toString();
    
    if (itemType == "layer") {
        // 处理图层可见性变化
        int layerIndex = item->data(0, Qt::UserRole).toInt();
        DrawingLayer *layer = m_layerManager->layer(layerIndex);
        if (layer) {
            bool visible = (item->checkState(1) == Qt::Checked);
            m_layerManager->setLayerVisible(layer, visible);
        }
    } else if (itemType == "shape") {
        // 处理形状可见性变化
        quintptr ptr = item->data(0, Qt::UserRole).value<quintptr>();
        DrawingShape *shape = reinterpret_cast<DrawingShape*>(ptr);
        if (shape) {
            bool visible = (item->checkState(1) == Qt::Checked);
            shape->setVisible(visible);
        }
    }
}

void LayerPanel::onLayerItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item || !m_layerManager) {
        return;
    }
    
    QString itemType = item->data(0, Qt::UserRole + 1).toString();
    
    if (itemType == "layer") {
        // 点击图层项，设置为活动图层
        int index = item->data(0, Qt::UserRole).toInt();
        m_layerManager->setActiveLayer(index);
        updateLayerButtons();
    } else if (itemType == "shape") {
        // 点击形状项，选中该形状
        quintptr ptr = item->data(0, Qt::UserRole).value<quintptr>();
        DrawingShape *shape = reinterpret_cast<DrawingShape*>(ptr);
        if (shape && m_scene) {
            m_scene->clearSelection();
            shape->setSelected(true);
            
            // 同时选中形状所在的图层
            QTreeWidgetItem *layerItem = item->parent();
            if (layerItem) {
                int layerIndex = layerItem->data(0, Qt::UserRole).toInt();
                // 避免循环调用，检查是否已经是活动图层
                if (m_layerManager->activeLayerIndex() != layerIndex) {
                    m_layerManager->setActiveLayer(layerIndex);
                }
            }
        }
    }
}

void LayerPanel::onLayerItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item) {
        return;
    }
    
    QString itemType = item->data(0, Qt::UserRole + 1).toString();
    
    if (itemType == "layer") {
        // 双击图层项，重命名图层
        int index = m_layerTree->indexOfTopLevelItem(item);
        renameLayer(index);
    } else if (itemType == "shape") {
        // 双击形状项，可以在这里添加形状重命名功能
        // 目前暂不实现
    }
}

void LayerPanel::addLayer(const QString &name)
{
    if (!m_layerTree) {
        return;
    }
    
    QString layerName = name.isEmpty() ? tr("新图层") : name;
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, layerName);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(1, Qt::Checked);
    item->setData(0, Qt::UserRole + 1, "layer");
    
    m_layerTree->insertTopLevelItem(0, item); // 插入到顶部
    m_layerTree->setCurrentItem(item);
    
    m_layerCountLabel->setText(tr("图层数量: %1").arg(m_layerTree->topLevelItemCount()));
    updateLayerButtons();
    emit layerChanged();
}

void LayerPanel::deleteCurrentLayer()
{
    if (!m_layerTree || !m_layerTree->currentItem()) {
        return;
    }
    
    QTreeWidgetItem *currentItem = m_layerTree->currentItem();
    int currentIndex = -1;
    
    // 如果是图层项
    if (currentItem->data(0, Qt::UserRole + 1).toString() == "layer") {
        currentIndex = m_layerTree->indexOfTopLevelItem(currentItem);
        delete m_layerTree->takeTopLevelItem(currentIndex);
    }
    // 如果是形状项，不允许删除图层
    else {
        return;
    }
    
    // 选择其他图层
    if (m_layerTree->topLevelItemCount() > 0) {
        int selectRow = qMin(currentIndex, m_layerTree->topLevelItemCount() - 1);
        m_layerTree->setCurrentItem(m_layerTree->topLevelItem(selectRow));
    }
    
    m_layerCountLabel->setText(tr("图层数量: %1").arg(m_layerTree->topLevelItemCount()));
    updateLayerButtons();
    emit layerChanged();
}

void LayerPanel::moveLayerUp()
{
    onMoveLayerUp();
}

void LayerPanel::moveLayerDown()
{
    onMoveLayerDown();
}

void LayerPanel::duplicateLayer()
{
    onDuplicateLayer();
}

void LayerPanel::mergeLayerDown()
{
    onMergeLayerDown();
}

void LayerPanel::toggleLayerVisibility(int index)
{
    if (!m_layerTree || index < 0 || index >= m_layerTree->topLevelItemCount()) {
        return;
    }
    
    QTreeWidgetItem *item = m_layerTree->topLevelItem(index);
    if (item) {
        Qt::CheckState newState = (item->checkState(1) == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
        item->setCheckState(1, newState);
    }
}

void LayerPanel::toggleLayerLock(int index)
{
    // TODO: 实现图层锁定功能
    Q_UNUSED(index);
}

void LayerPanel::renameLayer(int index)
{
    if (!m_layerTree || index < 0 || index >= m_layerTree->topLevelItemCount()) {
        return;
    }
    
    QTreeWidgetItem *item = m_layerTree->topLevelItem(index);
    if (!item) {
        return;
    }
    
    bool ok;
    QString newName = QInputDialog::getText(this, tr("重命名图层"),
                                           tr("新图层名称:"), QLineEdit::Normal,
                                           item->text(0), &ok);
    
    if (ok && !newName.isEmpty()) {
        if (m_layerManager) {
            int layerIndex = item->data(0, Qt::UserRole).toInt();
            DrawingLayer *layer = m_layerManager->layer(layerIndex);
            if (layer) {
                m_layerManager->setLayerName(layer, newName);
            }
        }
    }
}

void LayerPanel::selectLayer(int index)
{
    if (!m_layerTree || index < 0 || index >= m_layerTree->topLevelItemCount()) {
        return;
    }
    
    m_layerTree->setCurrentItem(m_layerTree->topLevelItem(index));
    
    // TODO: 实现图层选择逻辑
    // emit layerSelected(layer);
}

QTreeWidgetItem* LayerPanel::createLayerItem(DrawingLayer *layer, int index)
{
    // TODO: 实现从DrawingLayer创建QTreeWidgetItem的逻辑
    Q_UNUSED(layer);
    Q_UNUSED(index);
    return nullptr;
}