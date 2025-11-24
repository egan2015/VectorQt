#ifndef LAYER_MANAGER_H
#define LAYER_MANAGER_H

#include <QObject>
#include <QList>
#include <QString>

class DrawingScene;
class DrawingLayer;
class DrawingShape;
class LayerPanel;

/**
 * 图层管理器 - 管理场景中的所有图层 (单例模式)
 */
class LayerManager : public QObject
{
    Q_OBJECT

public:
    // 单例模式
    static LayerManager* instance();
    static void destroyInstance();
    
    ~LayerManager();
    
    // 设置场景和面板
    void setScene(DrawingScene *scene);
    void setLayerPanel(LayerPanel *panel);
    
    // 图层管理
    DrawingLayer* createLayer(const QString &name = QString());
    bool deleteLayer(DrawingLayer *layer);
    bool deleteLayer(int index);
    
    // 图层操作
    void moveLayerUp(DrawingLayer *layer);
    void moveLayerDown(DrawingLayer *layer);
    void moveLayerUp(int index);
    void moveLayerDown(int index);
    void duplicateLayer(DrawingLayer *layer);
    void duplicateLayer(int index);
    void mergeLayerDown(DrawingLayer *layer);
    void mergeLayerDown(int index);
    
    // 图层属性
    void setLayerName(DrawingLayer *layer, const QString &name);
    void setLayerVisible(DrawingLayer *layer, bool visible);
    void setLayerLocked(DrawingLayer *layer, bool locked);
    void setLayerOpacity(DrawingLayer *layer, qreal opacity);
    
    // 图层选择
    void setActiveLayer(DrawingLayer *layer);
    void setActiveLayer(int index);
    DrawingLayer* activeLayer() const { return m_activeLayer; }
    int activeLayerIndex() const;
    
    // 获取图层
    QList<DrawingLayer*> layers() const { return m_layers; }
    DrawingLayer* layer(int index) const;
    DrawingLayer* layer(const QString &name) const;
    int layerCount() const { return m_layers.count(); }
    int indexOf(DrawingLayer *layer) const;
    
    // 查找图形所属图层
    DrawingLayer* findLayerForShape(DrawingShape *shape) const;
    
    // 图层操作信号
    void updateLayerPanel();

signals:
    void layerAdded(DrawingLayer *layer);
    void layerRemoved(DrawingLayer *layer);
    void layerMoved(DrawingLayer *layer, int fromIndex, int toIndex);
    void layerChanged(DrawingLayer *layer);
    void activeLayerChanged(DrawingLayer *layer);
    void layersReordered();
    void layerContentChanged(DrawingLayer *layer);  // 图层内容变化信号

private slots:
    void onLayerPropertyChanged();

private:
    // 私有构造函数，确保单例
    explicit LayerManager(QObject *parent = nullptr);
    
    void addLayerToScene(DrawingLayer *layer);
    void removeLayerFromScene(DrawingLayer *layer);
    void updatePanel();
    void connectLayer(DrawingLayer *layer);
    void disconnectLayer(DrawingLayer *layer);
    
    static LayerManager *s_instance;
    
    DrawingScene *m_scene;
    LayerPanel *m_layerPanel;
    QList<DrawingLayer*> m_layers;
    DrawingLayer *m_activeLayer;
    int m_layerCounter;  // 用于生成唯一图层名称
};

#endif // LAYER_MANAGER_H