#include <QDebug>
#include "../core/layer-manager.h"
#include "../ui/drawingscene.h"
#include "../core/drawing-layer.h"
#include "../core/drawing-shape.h"
#include "../ui/layer-panel.h"

// 静态成员变量初始化
LayerManager *LayerManager::s_instance = nullptr;

LayerManager* LayerManager::instance()
{
    if (!s_instance) {
        s_instance = new LayerManager();
    }
    return s_instance;
}

void LayerManager::destroyInstance()
{
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

LayerManager::LayerManager(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
    , m_layerPanel(nullptr)
    , m_activeLayer(nullptr)
    , m_layerCounter(1)
{
    // 暂时不做任何初始化
}

LayerManager::~LayerManager()
{
    // 清理所有图层
    for (DrawingLayer *layer : m_layers) {
        if (layer) {
            disconnectLayer(layer);
            delete layer;
        }
    }
    m_layers.clear();
}

void LayerManager::setScene(DrawingScene *scene)
{
    qDebug() << "LayerManager::setScene called with scene:" << scene;
    qDebug() << "Current m_scene:" << m_scene << "Current layers count:" << m_layers.count();
    
    if (m_scene == scene) {
        qDebug() << "Scene is the same, returning";
        return;
    }
    
    m_scene = scene;
    
    // 如果场景已设置，创建默认图层
    if (m_scene && m_layers.isEmpty()) {
        qDebug() << "Creating default layer";
        // 使用标准方法创建默认图层
        DrawingLayer *layer = new DrawingLayer(tr("背景图层"));
        connectLayer(layer);
        m_layers.append(layer);
        m_activeLayer = layer;
        
        // 重要：添加到场景
        addLayerToScene(layer);
        
        // 设置背景图层的Z值（确保它在最底层）
        for (DrawingShape *shape : layer->shapes()) {
            if (shape) {
                shape->setZValue(-999);  // 背景图层使用很低的Z值
            }
        }
        
        qDebug() << "Default layer created successfully, total layers:" << m_layers.count();
        
        // 发出图层添加信号，通知UI
        qDebug() << "LayerManager: Emitting layerAdded signal for default layer:" << layer->name();
        emit layerAdded(layer);
        
        // 检查是否有LayerPanel已经连接，如果有但可能错过了信号，手动更新
        // 注意：这是一个临时解决方案，更好的架构应该是延迟初始化
        if (m_layerPanel) {
            qDebug() << "LayerManager: LayerPanel already exists, updating it";
            updatePanel();
        }
    } else {
        qDebug() << "Not creating default layer. m_scene:" << m_scene << "layers.isEmpty():" << m_layers.isEmpty();
    }
}

void LayerManager::setLayerPanel(LayerPanel *panel)
{
    if (m_layerPanel == panel) {
        return;
    }
    
    m_layerPanel = panel;
    qDebug() << "Layer panel set, updating with" << m_layers.count() << "layers";
    updatePanel();
}

DrawingLayer* LayerManager::createLayer(const QString &name)
{
    qDebug() << "LayerManager::createLayer called with name:" << name;
    
    QString layerName = name;
    if (layerName.isEmpty()) {
        layerName = tr("图层 %1").arg(m_layerCounter++);
    }
    
    qDebug() << "Creating layer with name:" << layerName;
    
    DrawingLayer *layer = new DrawingLayer(layerName);
    connectLayer(layer);
    
    // 添加到列表开头（新图层在最上层）
    m_layers.insert(0, layer);
    qDebug() << "Layer added to list, total layers:" << m_layers.count();
    
    // 添加到场景
    addLayerToScene(layer);
    
    // 更新所有图层的Z值
    for (int i = 0; i < m_layers.count(); ++i) {
        DrawingLayer *currentLayer = m_layers[i];
        for (DrawingShape *shape : currentLayer->shapes()) {
            if (shape) {
                shape->setZValue(-i);  // 索引0的图层在最上层
            }
        }
    }
    
    // 设置为活动图层
    setActiveLayer(layer);
    
    // 更新面板
    updatePanel();
    
    emit layerAdded(layer);
    qDebug() << "Layer created successfully:" << layer->name();
    return layer;
}

bool LayerManager::deleteLayer(DrawingLayer *layer)
{
    if (!layer || !m_layers.contains(layer)) {
        return false;
    }
    
    // 不能删除最后一个图层
    if (m_layers.count() <= 1) {
        return false;
    }
    
    int index = m_layers.indexOf(layer);
    
    // 如果删除的是活动图层，先设置新的活动图层
    DrawingLayer *newActiveLayer = nullptr;
    if (m_activeLayer == layer) {
        int newActiveIndex = qMin(index, m_layers.count() - 1);
        if (newActiveIndex >= 0 && newActiveIndex < m_layers.count()) {
            newActiveLayer = m_layers.at(newActiveIndex);
        }
    }
    
    // 从场景移除
    removeLayerFromScene(layer);
    
    // 从列表移除
    m_layers.removeOne(layer);
    
    // 断开连接
    disconnectLayer(layer);
    
    // 设置新的活动图层
    if (newActiveLayer) {
        setActiveLayer(newActiveLayer);
    }
    
    // 删除图层对象
    delete layer;
    
    // 更新面板
    updatePanel();
    
    emit layerRemoved(layer);
    return true;
}

bool LayerManager::deleteLayer(int index)
{
    if (index < 0 || index >= m_layers.count()) {
        return false;
    }
    
    return deleteLayer(m_layers.at(index));
}

void LayerManager::moveLayerUp(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    int index = m_layers.indexOf(layer);
    moveLayerUp(index);
}

void LayerManager::moveLayerUp(int index)
{
    if (index <= 0 || index >= m_layers.count()) {
        return;
    }
    
    // 交换位置
    m_layers.swapItemsAt(index, index - 1);
    
    // 更新所有图形的Z值（确保正确的绘制顺序）
    for (int i = 0; i < m_layers.count(); ++i) {
        DrawingLayer *layer = m_layers[i];
        for (DrawingShape *shape : layer->shapes()) {
            if (shape) {
                shape->setZValue(-i);  // 负值确保索引0的图层在最上层
            }
        }
    }
    
    updatePanel();
    emit layerMoved(m_layers[index - 1], index, index - 1);
}

void LayerManager::moveLayerDown(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    int index = m_layers.indexOf(layer);
    moveLayerDown(index);
}

void LayerManager::moveLayerDown(int index)
{
    if (index < 0 || index >= m_layers.count() - 1) {
        return;
    }
    
    // 交换位置
    m_layers.swapItemsAt(index, index + 1);
    
    // 更新所有图形的Z值
    for (int i = 0; i < m_layers.count(); ++i) {
        DrawingLayer *layer = m_layers[i];
        for (DrawingShape *shape : layer->shapes()) {
            if (shape) {
                shape->setZValue(-i);
            }
        }
    }
    
    updatePanel();
    emit layerMoved(m_layers[index + 1], index, index + 1);
}

void LayerManager::duplicateLayer(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    int index = m_layers.indexOf(layer);
    duplicateLayer(index);
}

void LayerManager::duplicateLayer(int index)
{
    if (index < 0 || index >= m_layers.count()) {
        return;
    }
    
    DrawingLayer *originalLayer = m_layers.at(index);
    QString newName = tr("%1 副本").arg(originalLayer->name());
    
    DrawingLayer *newLayer = createLayer(newName);
    
    // 复制属性（除了图形）
    newLayer->setVisible(originalLayer->isVisible());
    newLayer->setOpacity(originalLayer->opacity());
    newLayer->setLocked(originalLayer->isLocked());
    newLayer->setLayerTransform(originalLayer->layerTransform());
    
    // TODO: 复制图形内容
    // 这里需要实现图形的深拷贝
}

void LayerManager::mergeLayerDown(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    int index = m_layers.indexOf(layer);
    mergeLayerDown(index);
}

void LayerManager::mergeLayerDown(int index)
{
    if (index <= 0 || index >= m_layers.count()) {
        return;
    }
    
    DrawingLayer *topLayer = m_layers.at(index);
    DrawingLayer *bottomLayer = m_layers.at(index - 1);
    
    // 将topLayer的图形移动到bottomLayer
    QList<DrawingShape*> shapesToMove = topLayer->shapes();  // 创建副本
    for (DrawingShape *shape : shapesToMove) {
        if (shape) {
            // 从topLayer移除图形
            topLayer->removeShape(shape);
            // 添加到bottomLayer
            bottomLayer->addShape(shape);
        }
    }
    
    // 删除topLayer
    deleteLayer(index);
    
    // 设置bottomLayer为活动图层
    setActiveLayer(bottomLayer);
}

void LayerManager::setLayerName(DrawingLayer *layer, const QString &name)
{
    if (!layer || !m_layers.contains(layer)) {
        return;
    }
    
    layer->setName(name);
    updatePanel();
    emit layerChanged(layer);
}

void LayerManager::setLayerVisible(DrawingLayer *layer, bool visible)
{
    if (!layer || !m_layers.contains(layer)) {
        return;
    }
    
    layer->setVisible(visible);
    emit layerChanged(layer);
}

void LayerManager::setLayerLocked(DrawingLayer *layer, bool locked)
{
    if (!layer || !m_layers.contains(layer)) {
        return;
    }
    
    layer->setLocked(locked);
    emit layerChanged(layer);
}

void LayerManager::setLayerOpacity(DrawingLayer *layer, qreal opacity)
{
    if (!layer || !m_layers.contains(layer)) {
        return;
    }
    
    layer->setOpacity(opacity);
    emit layerChanged(layer);
}

void LayerManager::setActiveLayer(DrawingLayer *layer)
{
    if (m_activeLayer == layer) {
        return;
    }
    
    m_activeLayer = layer;
    updatePanel();
    emit activeLayerChanged(layer);
}

void LayerManager::setActiveLayer(int index)
{
    if (index < 0 || index >= m_layers.count()) {
        return;
    }
    
    setActiveLayer(m_layers.at(index));
}

int LayerManager::activeLayerIndex() const
{
    if (!m_activeLayer) {
        return -1;
    }
    
    return m_layers.indexOf(m_activeLayer);
}

DrawingLayer* LayerManager::layer(int index) const
{
    if (index < 0 || index >= m_layers.count()) {
        return nullptr;
    }
    
    return m_layers.at(index);
}

DrawingLayer* LayerManager::layer(const QString &name) const
{
    for (DrawingLayer *layer : m_layers) {
        if (layer && layer->name() == name) {
            return layer;
        }
    }
    return nullptr;
}

int LayerManager::indexOf(DrawingLayer *layer) const
{
    return m_layers.indexOf(layer);
}

void LayerManager::updateLayerPanel()
{
    updatePanel();
}

void LayerManager::addLayerToScene(DrawingLayer *layer)
{
    if (layer) {
        layer->setScene(m_scene);
    }
}

void LayerManager::removeLayerFromScene(DrawingLayer *layer)
{
    if (layer) {
        layer->setScene(nullptr);
    }
}

void LayerManager::updatePanel()
{
    if (!m_layerPanel) {
        return;
    }
    
    qDebug() << "Updating layer panel with" << m_layers.count() << "layers";
    
    // 调用图层面板的updateLayerList方法来显示真实图层
    m_layerPanel->updateLayerList();
}

void LayerManager::connectLayer(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    // 连接图层属性变化信号
    connect(layer, &DrawingLayer::visibilityChanged, this, [this, layer]() {
        emit layerChanged(layer);
    });
    connect(layer, &DrawingLayer::opacityChanged, this, [this, layer]() {
        emit layerChanged(layer);
    });
    connect(layer, &DrawingLayer::nameChanged, this, [this, layer]() {
        emit layerChanged(layer);
    });
    
    // 连接图层内容变化信号
    connect(layer, &DrawingLayer::shapeAdded, this, [this, layer]() {
        emit layerContentChanged(layer);
    });
    connect(layer, &DrawingLayer::shapeRemoved, this, [this, layer]() {
        emit layerContentChanged(layer);
    });
}

void LayerManager::disconnectLayer(DrawingLayer *layer)
{
    if (!layer) {
        return;
    }
    
    // 断开所有信号连接
    disconnect(layer, nullptr, this, nullptr);
}

void LayerManager::onLayerPropertyChanged()
{
    // 由于DrawingLayer不继承QObject，这里暂时不实现信号连接
    // TODO: 可以考虑让DrawingLayer继承QObject或使用其他方式通知属性变化
}