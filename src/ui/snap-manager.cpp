#include "snap-manager.h"
#include "drawingscene.h"
#include "../core/drawing-shape.h"
#include <QtMath>

SnapManager::SnapManager(DrawingScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_gridAlignmentEnabled(true)
    , m_snapEnabled(true)
    , m_snapTolerance(3)
    , m_objectSnapEnabled(true)
    , m_objectSnapTolerance(3)
    , m_snapIndicatorsVisible(true)
    , m_guidesEnabled(true)
    , m_guideSnapEnabled(true)
    , m_hasActiveSnap(false)
{
}

SnapManager::~SnapManager()
{
}

void SnapManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
}

DrawingScene* SnapManager::scene() const
{
    return m_scene;
}

void SnapManager::setGridAlignmentEnabled(bool enabled)
{
    if (m_gridAlignmentEnabled != enabled) {
        m_gridAlignmentEnabled = enabled;
        emit gridAlignmentChanged(enabled);
        emit statusMessageChanged(enabled ? "网格对齐已启用" : "网格对齐已禁用");
    }
}

bool SnapManager::isGridAlignmentEnabled() const
{
    return m_gridAlignmentEnabled;
}

void SnapManager::toggleGridAlignment()
{
    setGridAlignmentEnabled(!m_gridAlignmentEnabled);
}

void SnapManager::setSnapEnabled(bool enabled)
{
    if (m_snapEnabled != enabled) {
        m_snapEnabled = enabled;
        emit statusMessageChanged(enabled ? "智能吸附已启用" : "智能吸附已禁用");
    }
}

bool SnapManager::isSnapEnabled() const
{
    return m_snapEnabled;
}

void SnapManager::setSnapTolerance(int tolerance)
{
    if (m_snapTolerance != tolerance && tolerance >= 0) {
        m_snapTolerance = tolerance;
    }
}

int SnapManager::snapTolerance() const
{
    return m_snapTolerance;
}

void SnapManager::setObjectSnapEnabled(bool enabled)
{
    if (m_objectSnapEnabled != enabled) {
        m_objectSnapEnabled = enabled;
        emit statusMessageChanged(enabled ? "对象吸附已启用" : "对象吸附已禁用");
    }
}

bool SnapManager::isObjectSnapEnabled() const
{
    return m_objectSnapEnabled;
}

void SnapManager::setObjectSnapTolerance(int tolerance)
{
    if (m_objectSnapTolerance != tolerance && tolerance >= 0) {
        m_objectSnapTolerance = tolerance;
    }
}

int SnapManager::objectSnapTolerance() const
{
    return m_objectSnapTolerance;
}

QPointF SnapManager::alignToGrid(const QPointF &pos, DrawingShape *excludeShape, bool *isObjectSnap)
{
    QPointF result = pos;
    bool snapped = false;
    bool isObjectSnapped = false;  // 标记是否是对象吸附
    
    // Try object snapping first (highest priority)
    if (m_objectSnapEnabled) {
        ObjectSnapResult objectResult = snapToObjects(pos, excludeShape);
        if (objectResult.snappedToObject) {
            result = objectResult.snappedPos;
            snapped = true;
            isObjectSnapped = true;
        }
    }
    
    // Then try guide line snapping
    if (!snapped && m_guidesEnabled && m_guideSnapEnabled) {
        GuideSnapResult guideResult = snapToGuides(pos);
        if (guideResult.snappedToGuide) {
            result = guideResult.snappedPos;
            snapped = true;
        }
    }
    
    // Finally try grid snapping
    if (!snapped && m_scene && m_scene->isGridVisible() && m_gridAlignmentEnabled) {
        if (m_snapEnabled) {
            SnapResult gridResult = smartAlignToGrid(pos);
            if (gridResult.snappedX || gridResult.snappedY) {
                result = gridResult.snappedPos;
                snapped = true;
            }
        } else {
            // 传统对齐方式，从Scene获取grid大小
            int gridSize = m_scene->gridSize();
            qreal x = qRound(pos.x() / gridSize) * gridSize;
            qreal y = qRound(pos.y() / gridSize) * gridSize;
            result = QPointF(x, y);
            snapped = true;
        }
    }
    
    // 返回对象吸附标志
    if (isObjectSnap) {
        *isObjectSnap = isObjectSnapped;
    }
    
    return result;
}

QPointF SnapManager::alignToGrid(const QPointF &pos) const
{
    if (!m_scene || !m_scene->isGridVisible() || !m_gridAlignmentEnabled) {
        return pos;
    }
    
    // 从Scene获取grid大小
    int gridSize = m_scene->gridSize();
    qreal x = qRound(pos.x() / gridSize) * gridSize;
    qreal y = qRound(pos.y() / gridSize) * gridSize;
    
    return QPointF(x, y);
}

QRectF SnapManager::alignToGrid(const QRectF &rect) const
{
    if (!m_scene || !m_scene->isGridVisible() || !m_gridAlignmentEnabled) {
        return rect;
    }
    
    QPointF topLeft = alignToGrid(rect.topLeft());
    QPointF bottomRight = alignToGrid(rect.bottomRight());
    
    return QRectF(topLeft, bottomRight).normalized();
}

SnapResult SnapManager::smartAlignToGrid(const QPointF &pos) const
{
    SnapResult result;
    result.snappedPos = pos;
    
    if (!m_snapEnabled || !m_gridAlignmentEnabled || !m_scene || !m_scene->isGridVisible()) {
        return result;
    }
    
    const int tolerance = m_snapTolerance;
    const int gridSize = m_scene->gridSize(); // 从Scene获取grid大小
    
    // 计算最近的网格线
    int gridX = qRound(pos.x() / gridSize) * gridSize;
    int gridY = qRound(pos.y() / gridSize) * gridSize;
    
    // 检查X方向是否需要吸附
    if (qAbs(pos.x() - gridX) <= tolerance) {
        result.snappedPos.setX(gridX);
        result.snappedX = true;
    }
    
    // 检查Y方向是否需要吸附
    if (qAbs(pos.y() - gridY) <= tolerance) {
        result.snappedPos.setY(gridY);
        result.snappedY = true;
    }
    
    return result;
}

QPointF SnapManager::snapPoint(const QPointF &pos, DrawingShape *excludeShape)
{
    if (!m_snapEnabled) {
        return pos;
    }
    
    QPointF result = pos;
    bool snapped = false;
    bool isObjectSnapped = false;  // 标记是否是对象吸附
    
    // Try object snapping first (highest priority)
    if (m_objectSnapEnabled) {
        ObjectSnapResult objectResult = snapToObjects(pos, excludeShape);
        if (objectResult.snappedToObject) {
            result = objectResult.snappedPos;
            snapped = true;
            isObjectSnapped = true;
        }
    }
    
    // Then try guide line snapping
    if (!snapped && m_guidesEnabled && m_guideSnapEnabled) {
        GuideSnapResult guideResult = snapToGuides(pos);
        if (guideResult.snappedToGuide) {
            result = guideResult.snappedPos;
            snapped = true;
        }
    }
    
    // Finally try grid snapping
    if (!snapped && m_scene && m_scene->isGridVisible() && m_gridAlignmentEnabled) {
        if (m_snapEnabled) {
            SnapResult gridResult = smartAlignToGrid(pos);
            if (gridResult.snappedX || gridResult.snappedY) {
                result = gridResult.snappedPos;
                snapped = true;
            }
        } else {
            // 传统对齐方式，从Scene获取grid大小
            int gridSize = m_scene->gridSize();
            qreal x = qRound(pos.x() / gridSize) * gridSize;
            qreal y = qRound(pos.y() / gridSize) * gridSize;
            result = QPointF(x, y);
            snapped = true;
        }
    }
    
    return result;
}



ObjectSnapResult SnapManager::snapToObjects(const QPointF &pos, DrawingShape *excludeShape)
{
    ObjectSnapResult result;
    result.snappedPos = pos;
    
    if (!m_objectSnapEnabled) {
        return result;
    }
    
    const int tolerance = m_objectSnapTolerance;
    qreal minDistance = tolerance + 1;
    
    QList<ObjectSnapPoint> snapPoints = getObjectSnapPoints(excludeShape);
    
    for (const ObjectSnapPoint &snapPoint : snapPoints) {
        qreal distance = QLineF(pos, snapPoint.position).length();
        if (distance < minDistance) {
            minDistance = distance;
            result.snappedPos = snapPoint.position;
            result.snappedToObject = true;
            result.snapType = snapPoint.type;
            result.targetShape = snapPoint.shape;
            
            // 设置描述
            switch (snapPoint.type) {
                case SnapToLeft: result.snapDescription = tr("吸附到左边"); break;
                case SnapToRight: result.snapDescription = tr("吸附到右边"); break;
                case SnapToTop: result.snapDescription = tr("吸附到上边"); break;
                case SnapToBottom: result.snapDescription = tr("吸附到下边"); break;
                case SnapToCenterX: result.snapDescription = tr("吸附到水平中心"); break;
                case SnapToCenterY: result.snapDescription = tr("吸附到垂直中心"); break;
                case SnapToCenter: result.snapDescription = tr("吸附到中心"); break;
                case SnapToCorner: result.snapDescription = tr("吸附到角点"); break;
            }
        }
    }
    
    // 显示吸附指示器 - 只在真正接近时才显示
    if (result.snappedToObject) {
        // 确保距离在容差范围内
        qreal distance = QLineF(pos, result.snappedPos).length();
        // 更严格的检查：距离必须小于容差的一半，确保真正接近
        if (distance <= tolerance * 0.5) {
            m_hasActiveSnap = true;
            showSnapIndicators(result);
        } else {
            // 距离太远，不触发吸附
            result.snappedToObject = false;
            result.snappedPos = pos;
            m_hasActiveSnap = false;
            clearSnapIndicators();
        }
    } else {
        // 清除吸附指示器和活跃状态
        m_hasActiveSnap = false;
        clearSnapIndicators();
    }
    
    return result;
}

GuideSnapResult SnapManager::snapToGuides(const QPointF &pos)
{
    GuideSnapResult result;
    result.snappedPos = pos;
    result.snappedToGuide = false;
    
    // 简单的参考线吸附实现
    // TODO: 实现完整的参考线吸附逻辑
    return result;
}

QList<ObjectSnapPoint> SnapManager::getObjectSnapPoints(DrawingShape *excludeShape) const
{
    QList<ObjectSnapPoint> points;
    
    if (!m_scene) {
        return points;
    }
    
    QList<QGraphicsItem*> allItems = m_scene->items();
    
    for (QGraphicsItem *item : allItems) {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (!shape || shape == excludeShape || !shape->isVisible()) {
            continue;
        }
        
        QRectF bounds = shape->boundingRect();
        QPointF center = bounds.center();
        
        // 转换为场景坐标
        QRectF sceneBounds = shape->mapRectToScene(bounds);
        QPointF sceneCenter = sceneBounds.center();
        
        // 添加关键吸附点（使用场景坐标）
        points.append(ObjectSnapPoint(sceneBounds.topLeft(), SnapToCorner, shape));
        points.append(ObjectSnapPoint(sceneBounds.topRight(), SnapToCorner, shape));
        points.append(ObjectSnapPoint(sceneBounds.bottomLeft(), SnapToCorner, shape));
        points.append(ObjectSnapPoint(sceneBounds.bottomRight(), SnapToCorner, shape));
        
        // 边缘中点
        points.append(ObjectSnapPoint(QPointF(sceneBounds.center().x(), sceneBounds.top()), SnapToTop, shape));
        points.append(ObjectSnapPoint(QPointF(sceneBounds.center().x(), sceneBounds.bottom()), SnapToBottom, shape));
        points.append(ObjectSnapPoint(QPointF(sceneBounds.left(), sceneBounds.center().y()), SnapToLeft, shape));
        points.append(ObjectSnapPoint(QPointF(sceneBounds.right(), sceneBounds.center().y()), SnapToRight, shape));
        
        // 中心点
        points.append(ObjectSnapPoint(sceneCenter, SnapToCenter, shape));
    }
    
    return points;
}

// 吸附指示器显示方法实现
void SnapManager::showSnapIndicators(const ObjectSnapResult &snapResult)
{
    if (!m_snapIndicatorsVisible || !m_scene) {
        return;
    }
    
    m_lastSnapResult = snapResult;
    m_hasActiveSnap = true;
    m_scene->update(); // 简单的全局刷新
}

void SnapManager::clearSnapIndicators()
{
    if (m_hasActiveSnap) {
        m_lastSnapResult = ObjectSnapResult();
        m_hasActiveSnap = false;
        if (m_scene) {
            //m_scene->update(); // 简单的全局刷新
        }
    }
}

void SnapManager::clearExpiredSnapIndicators(const QPointF &currentPos)
{
    if (m_hasActiveSnap && m_lastSnapResult.snappedToObject) {
        // 检查当前位置是否还在吸附范围内
        qreal distance = QLineF(currentPos, m_lastSnapResult.snappedPos).length();
        if (distance > m_objectSnapTolerance) {
            clearSnapIndicators();
        }
    }
}

void SnapManager::setSnapIndicatorsVisible(bool visible)
{
    m_snapIndicatorsVisible = visible;
    if (!visible) {
        clearSnapIndicators();
    }
}

bool SnapManager::areSnapIndicatorsVisible() const
{
    return m_snapIndicatorsVisible;
}

bool SnapManager::hasActiveSnap() const
{
    return m_hasActiveSnap;
}

ObjectSnapResult SnapManager::getLastSnapResult() const
{
    return m_lastSnapResult;
}