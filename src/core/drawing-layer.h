#ifndef DRAWINGLAYER_H
#define DRAWINGLAYER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QDomElement>
#include <QTransform>

class DrawingShape;
class DrawingScene;

/**
 * 绘图层类 - 纯数据管理类，管理图层和图层中的图形
 */
class DrawingLayer : public QObject
{
    Q_OBJECT

public:
    explicit DrawingLayer(const QString &name = "Layer", QObject *parent = nullptr);
    ~DrawingLayer();
    
    // 图层属性
    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }
    
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);
    
    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal opacity);
    
    bool isLocked() const { return m_locked; }
    void setLocked(bool locked) { m_locked = locked; }
    
    // 图层内容管理
    void addShape(DrawingShape *shape);
    void removeShape(DrawingShape *shape);
    QList<DrawingShape*> shapes() const { return m_shapes; }
    
    // 场景管理
    void setScene(DrawingScene *scene);
    DrawingScene *scene() const { return m_scene; }
    
    // 图层变换
    void setLayerTransform(const QTransform &transform);
    QTransform layerTransform() const { return m_layerTransform; }
    
    // SVG相关
    void parseFromSvg(const QDomElement &element);
    QDomElement exportToSvg(QDomDocument &doc) const;

signals:
    void visibilityChanged(bool visible);
    void opacityChanged(qreal opacity);
    void nameChanged(const QString &name);

private:
    QString m_name;
    bool m_visible;
    qreal m_opacity;
    bool m_locked;
    QList<DrawingShape*> m_shapes;
    QTransform m_layerTransform;
    DrawingScene *m_scene;
};

#endif // DRAWINGLAYER_H