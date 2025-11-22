#include "../ui/layer-panel.h"
#include "../ui/drawingscene.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"
#include <QListWidget>
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

LayerPanel::LayerPanel(QWidget *parent)
    : QWidget(parent)
    , m_scene(nullptr)
    , m_layerManager(nullptr)
    , m_layerList(nullptr)
    , m_layerCountLabel(nullptr)
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
    
    m_layerManager = layerManager;
    
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
    
    // 创建图层列表
    m_layerList = new QListWidget(this);
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setDragDropMode(QAbstractItemView::InternalMove);
    m_layerList->setDefaultDropAction(Qt::MoveAction);
    
    connect(m_layerList, &QListWidget::itemChanged, this, &LayerPanel::onLayerItemChanged);
    connect(m_layerList, &QListWidget::itemClicked, this, &LayerPanel::onLayerItemClicked);
    connect(m_layerList, &QListWidget::itemDoubleClicked, this, &LayerPanel::onLayerItemDoubleClicked);
    
    mainLayout->addWidget(m_layerList);
    
    // 创建图层计数标签
    m_layerCountLabel = new QLabel(tr("图层数量: 0"), this);
    m_layerCountLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_layerCountLabel);
    
    // 初始更新按钮状态
    updateLayerButtons();
}

void LayerPanel::updateLayerList()
{
    qDebug() << "LayerPanel::updateLayerList called";
    
    if (!m_layerList) {
        qDebug() << "No layer list widget";
        return;
    }
    
    m_layerList->clear();
    
    if (!m_layerManager) {
        qDebug() << "No layer manager in updateLayerList";
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
        
        QListWidgetItem *item = new QListWidgetItem(layerName);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
        
        // 设置用户数据，存储图层索引
        item->setData(Qt::UserRole, i);
        
        // 如果是活动图层，高亮显示
        if (layer == m_layerManager->activeLayer()) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
        
        m_layerList->addItem(item);
        qDebug() << "Layer item added to list";
    }
    
    m_layerCountLabel->setText(tr("图层数量: %1").arg(layers.count()));
    
    // 选择活动图层
    int activeIndex = m_layerManager->activeLayerIndex();
    if (activeIndex >= 0 && activeIndex < m_layerList->count()) {
        m_layerList->setCurrentRow(activeIndex);
    }
    
    updateLayerButtons();
    
    // 强制刷新UI
    m_layerList->update();
    update();
}

void LayerPanel::updateLayerButtons()
{
    bool hasScene = (m_scene != nullptr);
    bool hasSelection = m_layerList && m_layerList->currentItem() != nullptr;
    int currentIndex = m_layerList ? m_layerList->currentRow() : -1;
    int layerCount = m_layerList ? m_layerList->count() : 0;
    
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
    if (!m_layerManager || !m_layerList || !m_layerList->currentItem()) {
        return;
    }
    
    int ret = QMessageBox::question(this, tr("删除图层"),
                                   tr("确定要删除当前图层吗？"),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        int currentIndex = m_layerList->currentRow();
        m_layerManager->deleteLayer(currentIndex);
    }
}

void LayerPanel::onMoveLayerUp()
{
    if (!m_layerManager) {
        return;
    }
    
    int currentRow = m_layerList->currentRow();
    if (currentRow > 0) {
        m_layerManager->moveLayerUp(currentRow);
    }
}

void LayerPanel::onMoveLayerDown()
{
    if (!m_layerManager) {
        return;
    }
    
    int currentRow = m_layerList->currentRow();
    if (currentRow >= 0 && currentRow < m_layerList->count() - 1) {
        m_layerManager->moveLayerDown(currentRow);
    }
}

void LayerPanel::onDuplicateLayer()
{
    if (!m_layerManager) {
        return;
    }
    
    int currentRow = m_layerList->currentRow();
    if (currentRow >= 0) {
        m_layerManager->duplicateLayer(currentRow);
    }
}

void LayerPanel::onMergeLayerDown()
{
    if (!m_layerManager) {
        return;
    }
    
    int currentRow = m_layerList->currentRow();
    if (currentRow <= 0) {
        return;
    }
    
    int ret = QMessageBox::question(this, tr("合并图层"),
                                   tr("确定要将当前图层向下合并吗？"),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        m_layerManager->mergeLayerDown(currentRow);
    }
}

void LayerPanel::onLayerItemChanged(QListWidgetItem *item)
{
    if (!item || !m_layerManager) {
        return;
    }
    
    int layerIndex = item->data(Qt::UserRole).toInt();
    DrawingLayer *layer = m_layerManager->layer(layerIndex);
    if (!layer) {
        return;
    }
    
    // 处理可见性变化
    bool visible = (item->checkState() == Qt::Checked);
    m_layerManager->setLayerVisible(layer, visible);
}

void LayerPanel::onLayerItemClicked(QListWidgetItem *item)
{
    if (!item || !m_layerManager) {
        return;
    }
    
    int index = item->data(Qt::UserRole).toInt();
    m_layerManager->setActiveLayer(index);
    updateLayerButtons();
}

void LayerPanel::onLayerItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    
    int index = m_layerList->row(item);
    renameLayer(index);
}

void LayerPanel::addLayer(const QString &name)
{
    if (!m_layerList) {
        return;
    }
    
    QString layerName = name.isEmpty() ? tr("新图层") : name;
    QListWidgetItem *item = new QListWidgetItem(layerName);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Checked);
    
    m_layerList->insertItem(0, item); // 插入到顶部
    m_layerList->setCurrentRow(0);
    
    m_layerCountLabel->setText(tr("图层数量: %1").arg(m_layerList->count()));
    updateLayerButtons();
    emit layerChanged();
}

void LayerPanel::deleteCurrentLayer()
{
    if (!m_layerList || !m_layerList->currentItem()) {
        return;
    }
    
    int currentRow = m_layerList->currentRow();
    delete m_layerList->takeItem(currentRow);
    
    // 选择其他图层
    if (m_layerList->count() > 0) {
        int selectRow = qMin(currentRow, m_layerList->count() - 1);
        m_layerList->setCurrentRow(selectRow);
    }
    
    m_layerCountLabel->setText(tr("图层数量: %1").arg(m_layerList->count()));
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
    if (!m_layerList || index < 0 || index >= m_layerList->count()) {
        return;
    }
    
    QListWidgetItem *item = m_layerList->item(index);
    if (item) {
        Qt::CheckState newState = (item->checkState() == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
        item->setCheckState(newState);
    }
}

void LayerPanel::toggleLayerLock(int index)
{
    // TODO: 实现图层锁定功能
    Q_UNUSED(index);
}

void LayerPanel::renameLayer(int index)
{
    if (!m_layerList || index < 0 || index >= m_layerList->count()) {
        return;
    }
    
    QListWidgetItem *item = m_layerList->item(index);
    if (!item) {
        return;
    }
    
    bool ok;
    QString newName = QInputDialog::getText(this, tr("重命名图层"),
                                           tr("新图层名称:"), QLineEdit::Normal,
                                           item->text(), &ok);
    
    if (ok && !newName.isEmpty()) {
        if (m_layerManager) {
            int layerIndex = item->data(Qt::UserRole).toInt();
            DrawingLayer *layer = m_layerManager->layer(layerIndex);
            if (layer) {
                m_layerManager->setLayerName(layer, newName);
            }
        }
    }
}

void LayerPanel::selectLayer(int index)
{
    if (!m_layerList || index < 0 || index >= m_layerList->count()) {
        return;
    }
    
    m_layerList->setCurrentRow(index);
    
    // TODO: 实现图层选择逻辑
    // emit layerSelected(layer);
}

QListWidgetItem* LayerPanel::createLayerItem(DrawingLayer *layer, int index)
{
    // TODO: 实现从DrawingLayer创建QListWidgetItem的逻辑
    Q_UNUSED(layer);
    Q_UNUSED(index);
    return nullptr;
}