#include "transform-system.h"
#include <QStack>
#include <QtMath>

// TransformObject 实现
TransformObject::TransformObject(const QRectF &localBounds)
    : m_localBounds(localBounds)
    , m_transformDirty(true)
{
}

void TransformObject::addOperation(std::shared_ptr<TransformOperation> op)
{
    m_operations.append(op);
    m_transformDirty = true;
}

void TransformObject::removeOperation(int index)
{
    if (index >= 0 && index < m_operations.size()) {
        m_operations.removeAt(index);
        m_transformDirty = true;
    }
}

void TransformObject::clearOperations()
{
    m_operations.clear();
    m_transformDirty = true;
}

QRectF TransformObject::transformedBounds() const
{
    QTransform t = combinedTransform();
    return t.mapRect(m_localBounds);
}

QPointF TransformObject::mapToScene(const QPointF &localPos) const
{
    return combinedTransform().map(localPos);
}

QPointF TransformObject::mapToLocal(const QPointF &scenePos) const
{
    return combinedTransform().inverted().map(scenePos);
}

QTransform TransformObject::combinedTransform() const
{
    if (m_transformDirty) {
        updateCache();
    }
    return m_cachedTransform;
}

void TransformObject::updateCache() const
{
    m_cachedTransform = QTransform();
    for (const auto &op : m_operations) {
        m_cachedTransform = op->toTransform() * m_cachedTransform;
    }
    m_transformDirty = false;
}

QPointF TransformObject::translation() const
{
    QPointF total;
    for (const auto &op : m_operations) {
        if (op->type() == TransformOperation::Translate) {
            auto trans = std::dynamic_pointer_cast<TranslateOperation>(op);
            total += trans->delta();
        }
    }
    return total;
}

double TransformObject::rotation() const
{
    double total = 0;
    for (const auto &op : m_operations) {
        if (op->type() == TransformOperation::Rotate) {
            auto rot = std::dynamic_pointer_cast<RotateOperation>(op);
            total += rot->angle();
        }
    }
    return total;
}

QPointF TransformObject::scale() const
{
    double sx = 1.0, sy = 1.0;
    for (const auto &op : m_operations) {
        if (op->type() == TransformOperation::Scale) {
            auto sc = std::dynamic_pointer_cast<ScaleOperation>(op);
            sx *= sc->scaleX();
            sy *= sc->scaleY();
        }
    }
    return QPointF(sx, sy);
}

QPointF TransformObject::shear() const
{
    double sh = 0, sv = 0;
    for (const auto &op : m_operations) {
        if (op->type() == TransformOperation::Shear) {
            auto shOp = std::dynamic_pointer_cast<ShearOperation>(op);
            sh += shOp->shearX();
            sv += shOp->shearY();
        }
    }
    return QPointF(sh, sv);
}

void TransformObject::saveState()
{
    m_savedState.clear();
    for (const auto &op : m_operations) {
        m_savedState.append(op->clone());
    }
}

void TransformObject::restoreState()
{
    m_operations.clear();
    for (const auto &op : m_savedState) {
        m_operations.append(op->clone());
    }
    m_transformDirty = true;
}

// TransformManager 实现
TransformManager::TransformManager()
{
}

void TransformManager::addObject(TransformObject *obj)
{
    if (obj && !m_objects.contains(obj)) {
        m_objects.append(obj);
    }
}

void TransformManager::removeObject(TransformObject *obj)
{
    m_objects.removeAll(obj);
}

void TransformManager::clearObjects()
{
    m_objects.clear();
}

void TransformManager::translateSelection(const QPointF &delta)
{
    for (auto *obj : m_objects) {
        obj->addOperation(std::make_shared<TranslateOperation>(delta));
    }
}

void TransformManager::rotateSelection(double angle, const QPointF &center)
{
    for (auto *obj : m_objects) {
        obj->addOperation(std::make_shared<RotateOperation>(angle, center));
    }
}

void TransformManager::scaleSelection(double sx, double sy, const QPointF &center)
{
    for (auto *obj : m_objects) {
        obj->addOperation(std::make_shared<ScaleOperation>(sx, sy, center));
    }
}

void TransformManager::shearSelection(double sh, double sv, const QPointF &center)
{
    for (auto *obj : m_objects) {
        obj->addOperation(std::make_shared<ShearOperation>(sh, sv, center));
    }
}

QRectF TransformManager::selectionBounds() const
{
    if (m_objects.isEmpty()) {
        return QRectF();
    }
    
    QRectF bounds = m_objects.first()->transformedBounds();
    for (int i = 1; i < m_objects.size(); ++i) {
        bounds = bounds.united(m_objects[i]->transformedBounds());
    }
    return bounds;
}

void TransformManager::saveSelectionState()
{
    m_selectionHistory.clear();
    for (auto *obj : m_objects) {
        QList<std::shared_ptr<TransformOperation>> state;
        for (const auto &op : obj->operations()) {
            state.append(op->clone());
        }
        m_selectionHistory.append(state);
    }
}

void TransformManager::restoreSelectionState()
{
    if (m_selectionHistory.size() != m_objects.size()) {
        qWarning() << "TransformManager: State history size mismatch";
        return;
    }
    
    for (int i = 0; i < m_objects.size(); ++i) {
        m_objects[i]->clearOperations();
        for (const auto &op : m_selectionHistory[i]) {
            m_objects[i]->addOperation(op->clone());
        }
    }
}