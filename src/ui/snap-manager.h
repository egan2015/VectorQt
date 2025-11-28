#ifndef SNAP_MANAGER_H
#define SNAP_MANAGER_H

#include <QObject>
#include <QPointF>
#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QRectF>

class DrawingScene;
class DrawingShape;

// 吸附点类型
enum SnapType {
    SnapToLeft,
    SnapToRight,
    SnapToTop,
    SnapToBottom,
    SnapToCenterX,
    SnapToCenterY,
    SnapToCenter,
    SnapToCorner
};

// 对象吸附点结构
struct ObjectSnapPoint {
    QPointF position;
    SnapType type;
    DrawingShape *shape;
    
    ObjectSnapPoint() = default;
    ObjectSnapPoint(const QPointF &pos, SnapType t, DrawingShape *s)
        : position(pos), type(t), shape(s) {}
};

// 对象吸附结果
struct ObjectSnapResult {
    QPointF snappedPos;
    bool snappedToObject;
    SnapType snapType;
    DrawingShape *targetShape;
    QString snapDescription;
};

// 网格吸附结果
struct SnapResult {
    bool snappedX;
    bool snappedY;
    QPointF snappedPos;
};

// 参考线吸附结果
struct GuideSnapResult {
    bool snappedToGuide;
    QPointF snappedPos;
};

class SnapManager : public QObject
{
    Q_OBJECT

public:
    explicit SnapManager(DrawingScene *scene, QObject *parent = nullptr);
    ~SnapManager();
    
    void setScene(DrawingScene *scene);
    DrawingScene* scene() const;

    // 网格吸附相关方法（保留在SnapManager中）
    void setGridAlignmentEnabled(bool enabled);
    bool isGridAlignmentEnabled() const;
    void toggleGridAlignment();
    
    // 智能吸附
    void setSnapEnabled(bool enabled);
    bool isSnapEnabled() const;
    
    void setSnapTolerance(int tolerance);
    int snapTolerance() const;
    
    // 对象吸附
    void setObjectSnapEnabled(bool enabled);
    bool isObjectSnapEnabled() const;
    
    void setObjectSnapTolerance(int tolerance);
    int objectSnapTolerance() const;
    
    // 吸附操作
    QPointF alignToGrid(const QPointF &pos, DrawingShape *excludeShape = nullptr, bool *isObjectSnap = nullptr);
    QPointF alignToGrid(const QPointF &pos) const;
    QRectF alignToGrid(const QRectF &rect) const;
    ObjectSnapResult snapToObjects(const QPointF &pos, DrawingShape *excludeShape = nullptr);
    GuideSnapResult snapToGuides(const QPointF &pos);
    SnapResult smartAlignToGrid(const QPointF &pos) const;
    QPointF snapPoint(const QPointF &pos, DrawingShape *excludeShape = nullptr);
    
    // 获取对象吸附点
    QList<ObjectSnapPoint> getObjectSnapPoints(DrawingShape *excludeShape = nullptr) const;
    
    // 吸附指示器显示
    void showSnapIndicators(const ObjectSnapResult &snapResult);
    void clearSnapIndicators();
    void clearExpiredSnapIndicators(const QPointF &currentPos);
    void setSnapIndicatorsVisible(bool visible);
    bool areSnapIndicatorsVisible() const;
    bool hasActiveSnap() const;
    
    // 获取当前吸附结果
    ObjectSnapResult getLastSnapResult() const;
    
    // 初始化吸附指示器（在场景准备好后调用）
    void initializeIndicator();

signals:
    void gridAlignmentChanged(bool enabled);
    void statusMessageChanged(const QString &message);

private:
    DrawingScene *m_scene;
    
    // 网格吸附相关属性（保留）
    bool m_gridAlignmentEnabled;
    
    bool m_snapEnabled;
    int m_snapTolerance;
    bool m_objectSnapEnabled;
    int m_objectSnapTolerance;
    bool m_snapIndicatorsVisible;
    
    // 参考线属性
    bool m_guidesEnabled;
    bool m_guideSnapEnabled;
    
    // 吸附指示器相关
    bool m_hasActiveSnap;
    ObjectSnapResult m_lastSnapResult;
    QGraphicsLineItem *m_snapIndicator;
};

#endif // SNAP_MANAGER_H