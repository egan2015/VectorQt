#ifndef OBJECT_TREE_ITEM_H
#define OBJECT_TREE_ITEM_H

#include <QObject>
#include <QList>
#include <QVariant>
#include <QIcon>

class DrawingShape;
class DrawingLayer;

/**
 * 对象树项基类 - 用于构建图层与对象的树形结构
 */
class ObjectTreeItem : public QObject
{
    Q_OBJECT

public:
    enum ItemType {
        LayerItem,
        ShapeItem,
        GroupItem
    };

    explicit ObjectTreeItem(ItemType type, ObjectTreeItem *parent = nullptr);
    virtual ~ObjectTreeItem();

    // 树形结构
    ObjectTreeItem *parent() const { return m_parent; }
    void setParent(ObjectTreeItem *parent);
    
    QList<ObjectTreeItem*> children() const { return m_children; }
    ObjectTreeItem *child(int row) const;
    int childCount() const { return m_children.count(); }
    int row() const;
    
    void appendChild(ObjectTreeItem *child);
    void insertChild(int row, ObjectTreeItem *child);
    void removeChild(ObjectTreeItem *child);
    ObjectTreeItem *takeChild(int row);
    
    // 数据接口
    ItemType itemType() const { return m_itemType; }
    
    virtual QString name() const = 0;
    virtual void setName(const QString &name) = 0;
    virtual bool isVisible() const = 0;
    virtual void setVisible(bool visible) = 0;
    virtual bool isLocked() const = 0;
    virtual void setLocked(bool locked) = 0;
    virtual QIcon icon() const = 0;
    
    // 获取关联的对象
    virtual DrawingLayer* layer() const { return nullptr; }
    virtual DrawingShape* shape() const { return nullptr; }
    
    // 拖拽支持
    bool isDraggable() const { return m_isDraggable; }
    void setDraggable(bool draggable) { m_isDraggable = draggable; }
    
    bool isDropTarget() const { return m_isDropTarget; }
    void setDropTarget(bool dropTarget) { m_isDropTarget = dropTarget; }

signals:
    void itemChanged();
    void childAdded(ObjectTreeItem *child);
    void childRemoved(ObjectTreeItem *child);

protected:
    ItemType m_itemType;
    ObjectTreeItem *m_parent;
    QList<ObjectTreeItem*> m_children;
    bool m_isDraggable;
    bool m_isDropTarget;
};

/**
 * 图层树项
 */
class LayerTreeItem : public ObjectTreeItem
{
    Q_OBJECT

public:
    explicit LayerTreeItem(DrawingLayer *layer, ObjectTreeItem *parent = nullptr);
    
    QString name() const override;
    void setName(const QString &name) override;
    bool isVisible() const override;
    void setVisible(bool visible) override;
    bool isLocked() const override;
    void setLocked(bool locked) override;
    QIcon icon() const override;
    
    DrawingLayer* layer() const override { return m_layer; }

private slots:
    void onLayerPropertyChanged();

private:
    DrawingLayer *m_layer;
};

/**
 * 图形树项
 */
class ShapeTreeItem : public ObjectTreeItem
{
    Q_OBJECT

public:
    explicit ShapeTreeItem(DrawingShape *shape, ObjectTreeItem *parent = nullptr);
    
    QString name() const override;
    void setName(const QString &name) override;
    bool isVisible() const override;
    void setVisible(bool visible) override;
    bool isLocked() const override;
    void setLocked(bool locked) override;
    QIcon icon() const override;
    
    DrawingShape* shape() const override { return m_shape; }

private slots:
    void onShapePropertyChanged();

private:
    DrawingShape *m_shape;
    QString m_defaultName;
};

/**
 * 根节点类 - 虚拟根节点，不对应实际对象
 */
class RootTreeItem : public ObjectTreeItem
{
    Q_OBJECT

public:
    explicit RootTreeItem() : ObjectTreeItem(LayerItem, nullptr) {}
    
    QString name() const override { return "Root"; }
    void setName(const QString &name) override { Q_UNUSED(name); }
    bool isVisible() const override { return true; }
    void setVisible(bool visible) override { Q_UNUSED(visible); }
    bool isLocked() const override { return false; }
    void setLocked(bool locked) override { Q_UNUSED(locked); }
    QIcon icon() const override { return QIcon(); }
};

#endif // OBJECT_TREE_ITEM_H