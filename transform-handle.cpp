#include "transform-handle.h"
#include "drawingscene.h"
#include <QGraphicsScene>
#include <QBrush>
#include <QPen>


// 静态颜色定义
const QColor HandleManager::HANDLE_COLOR = QColor(255, 255, 255);
const QColor HandleManager::ACTIVE_HANDLE_COLOR = QColor(100, 149, 237);
const QColor HandleManager::ROTATE_HANDLE_COLOR = QColor(50, 205, 50);

HandleManager::HandleManager(DrawingScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_activeHandle(TransformHandle::None)
    , m_centerHandle(nullptr)
    , m_rotateHandle(nullptr)
{
    createHandles();
}

HandleManager::~HandleManager()
{
    destroyHandles();
}

void HandleManager::createHandles()
{
    if (!m_scene) {
        return;
    }
    
    // 创建角点手柄
    for (int i = 0; i < 4; ++i) {
        QGraphicsRectItem *handle = new QGraphicsRectItem(0, 0, getHandleSize(), getHandleSize());
        handle->setBrush(QBrush(HANDLE_COLOR));
        handle->setPen(QPen(Qt::black, 1));
        handle->setZValue(2000);
        handle->setVisible(false);
        m_scene->addItem(handle);
        m_cornerHandles.append(handle);
    }
    
    // 创建边缘手柄
    for (int i = 0; i < 4; ++i) {
        QGraphicsRectItem *handle = new QGraphicsRectItem(0, 0, getHandleSize(), getHandleSize());
        handle->setBrush(QBrush(HANDLE_COLOR));
        handle->setPen(QPen(Qt::black, 1));
        handle->setZValue(2000);
        handle->setVisible(false);
        m_scene->addItem(handle);
        
        m_edgeHandles.append(handle);
    }
    
    // 创建中心手柄
    m_centerHandle = new QGraphicsEllipseItem(0, 0, getHandleSize() * 1.5, getHandleSize() * 1.5);
    m_centerHandle->setBrush(QBrush(HANDLE_COLOR));
    m_centerHandle->setPen(QPen(Qt::black, 1));
    m_centerHandle->setZValue(2000);
    m_centerHandle->setVisible(false);
    m_scene->addItem(m_centerHandle);
    
    
    // 创建旋转手柄
    m_rotateHandle = new QGraphicsEllipseItem(0, 0, getHandleSize() * 1.2, getHandleSize() * 1.2);
    m_rotateHandle->setBrush(QBrush(ROTATE_HANDLE_COLOR));
    m_rotateHandle->setPen(QPen(Qt::black, 1));
    m_rotateHandle->setZValue(2000);
    m_rotateHandle->setVisible(false);
    m_scene->addItem(m_rotateHandle);
    
    
    
}

void HandleManager::destroyHandles()
{
    if (!m_scene) {
        return;
    }
    
    // 销毁角点手柄
    for (QGraphicsRectItem *handle : m_cornerHandles) {
        if (handle) {
            m_scene->removeItem(handle);
            delete handle;
        }
    }
    m_cornerHandles.clear();
    
    // 销毁边缘手柄
    for (QGraphicsRectItem *handle : m_edgeHandles) {
        if (handle) {
            m_scene->removeItem(handle);
            delete handle;
        }
    }
    m_edgeHandles.clear();
    
    // 销毁中心手柄
    if (m_centerHandle) {
        m_scene->removeItem(m_centerHandle);
        delete m_centerHandle;
        m_centerHandle = nullptr;
    }
    
    // 销毁旋转手柄
    if (m_rotateHandle) {
        m_scene->removeItem(m_rotateHandle);
        delete m_rotateHandle;
        m_rotateHandle = nullptr;
    }
}

void HandleManager::showHandles()
{
    if (!m_scene) return;
    
    
    
    int visibleCount = 0;
    for (int i = 0; i < m_cornerHandles.size(); ++i) {
        QGraphicsRectItem *handle = m_cornerHandles[i];
        if (handle) {
            handle->setVisible(true);
            visibleCount++;
            
        }
    }
    
    for (int i = 0; i < m_edgeHandles.size(); ++i) {
        QGraphicsRectItem *handle = m_edgeHandles[i];
        if (handle) {
            handle->setVisible(true);
            visibleCount++;
            
        }
    }
    
    if (m_centerHandle) {
        m_centerHandle->setVisible(true);
        visibleCount++;
        
    }
    
    if (m_rotateHandle) {
        m_rotateHandle->setVisible(true);
        visibleCount++;
        
    }
    
    
    
    // 强制更新场景
    if (m_scene) {
        m_scene->update();
    }
}

void HandleManager::hideHandles()
{
    for (QGraphicsRectItem *handle : m_cornerHandles) {
        if (handle) {
            handle->setVisible(false);
        }
    }
    for (QGraphicsRectItem *handle : m_edgeHandles) {
        if (handle) {
            handle->setVisible(false);
        }
    }
    if (m_centerHandle) {
        m_centerHandle->setVisible(false);
    }
    if (m_rotateHandle) {
        m_rotateHandle->setVisible(false);
    }
}

void HandleManager::updateHandles(const QRectF &bounds)
{
    m_bounds = bounds;
    if (bounds.isEmpty()) return;
    
    const qreal handleSize = getHandleSize();
    const qreal halfSize = handleSize / 2.0;
    
    // 更新角点手柄
    updateHandlePosition(TransformHandle::TopLeft, bounds.topLeft());
    updateHandlePosition(TransformHandle::TopRight, bounds.topRight());
    updateHandlePosition(TransformHandle::BottomLeft, bounds.bottomLeft());
    updateHandlePosition(TransformHandle::BottomRight, bounds.bottomRight());
    
    // 更新边缘手柄
    updateHandlePosition(TransformHandle::Left, QPointF(bounds.left(), bounds.center().y()));
    updateHandlePosition(TransformHandle::Right, QPointF(bounds.right(), bounds.center().y()));
    updateHandlePosition(TransformHandle::Top, QPointF(bounds.center().x(), bounds.top()));
    updateHandlePosition(TransformHandle::Bottom, QPointF(bounds.center().x(), bounds.bottom()));
    
    // 更新中心手柄
    updateHandlePosition(TransformHandle::Center, bounds.center());
    
    // 更新旋转手柄（在顶部中心上方）
    QPointF rotatePos = QPointF(bounds.center().x(), bounds.top());
    rotatePos.setY(rotatePos.y() - 20.0); // 在顶部上方20像素
    updateHandlePosition(TransformHandle::Rotate, rotatePos);
}

void HandleManager::updateHandlePosition(TransformHandle::HandleType type, const QPointF &pos)
{
    const qreal handleSize = getHandleSize();
    const qreal halfSize = handleSize / 2.0;
    const qreal centerSize = handleSize * 1.5 / 2.0;
    const qreal rotateSize = handleSize * 1.2 / 2.0;
    
    QGraphicsItem *handle = nullptr;
    
    switch (type) {
        case TransformHandle::TopLeft:
            if (m_cornerHandles.size() > 0) handle = m_cornerHandles[0];
            break;
        case TransformHandle::TopRight:
            if (m_cornerHandles.size() > 1) handle = m_cornerHandles[1];
            break;
        case TransformHandle::BottomLeft:
            if (m_cornerHandles.size() > 2) handle = m_cornerHandles[2];
            break;
        case TransformHandle::BottomRight:
            if (m_cornerHandles.size() > 3) handle = m_cornerHandles[3];
            break;
        case TransformHandle::Left:
            if (m_edgeHandles.size() > 0) handle = m_edgeHandles[0];
            break;
        case TransformHandle::Right:
            if (m_edgeHandles.size() > 1) handle = m_edgeHandles[1];
            break;
        case TransformHandle::Top:
            if (m_edgeHandles.size() > 2) handle = m_edgeHandles[2];
            break;
        case TransformHandle::Bottom:
            if (m_edgeHandles.size() > 3) handle = m_edgeHandles[3];
            break;
        case TransformHandle::Center:
            handle = m_centerHandle;
            break;
        case TransformHandle::Rotate:
            handle = m_rotateHandle;
            break;
        default:
            return;
    }
    
    if (!handle) return;
    
    // 根据手柄类型调整位置
    if (type == TransformHandle::Center) {
        handle->setPos(pos.x() - centerSize, pos.y() - centerSize);
    } else if (type == TransformHandle::Rotate) {
        handle->setPos(pos.x() - rotateSize, pos.y() - rotateSize);
    } else {
        handle->setPos(pos.x() - halfSize, pos.y() - halfSize);
    }
    
    // 更新活动手柄颜色
    if (type == m_activeHandle) {
        if (QGraphicsRectItem *rectHandle = qgraphicsitem_cast<QGraphicsRectItem*>(handle)) {
            rectHandle->setBrush(QBrush(ACTIVE_HANDLE_COLOR));
        } else if (QGraphicsEllipseItem *ellipseHandle = qgraphicsitem_cast<QGraphicsEllipseItem*>(handle)) {
            ellipseHandle->setBrush(QBrush(ACTIVE_HANDLE_COLOR));
        }
    } else {
        if (QGraphicsRectItem *rectHandle = qgraphicsitem_cast<QGraphicsRectItem*>(handle)) {
            rectHandle->setBrush(QBrush(HANDLE_COLOR));
        } else if (QGraphicsEllipseItem *ellipseHandle = qgraphicsitem_cast<QGraphicsEllipseItem*>(handle)) {
            if (type == TransformHandle::Rotate) {
                ellipseHandle->setBrush(QBrush(ROTATE_HANDLE_COLOR));
            } else {
                ellipseHandle->setBrush(QBrush(HANDLE_COLOR));
            }
        }
    }
}

TransformHandle::HandleType HandleManager::getHandleAtPosition(const QPointF &scenePos) const
{
    const qreal tolerance = getHandleSize() / 2.0 + 2.0; // 添加一些容差
    
    // 检查角点手柄
    for (int i = 0; i < m_cornerHandles.size(); ++i) {
        if (m_cornerHandles[i] && m_cornerHandles[i]->isVisible()) {
            QPointF handlePos = m_cornerHandles[i]->sceneBoundingRect().center();
            if (QLineF(scenePos, handlePos).length() <= tolerance) {
                switch (i) {
                    case 0: return TransformHandle::TopLeft;
                    case 1: return TransformHandle::TopRight;
                    case 2: return TransformHandle::BottomLeft;
                    case 3: return TransformHandle::BottomRight;
                }
            }
        }
    }
    
    // 检查边缘手柄
    for (int i = 0; i < m_edgeHandles.size(); ++i) {
        if (m_edgeHandles[i] && m_edgeHandles[i]->isVisible()) {
            QPointF handlePos = m_edgeHandles[i]->sceneBoundingRect().center();
            if (QLineF(scenePos, handlePos).length() <= tolerance) {
                switch (i) {
                    case 0: return TransformHandle::Left;
                    case 1: return TransformHandle::Right;
                    case 2: return TransformHandle::Top;
                    case 3: return TransformHandle::Bottom;
                }
            }
        }
    }
    
    // 检查中心手柄
    if (m_centerHandle && m_centerHandle->isVisible()) {
        QPointF handlePos = m_centerHandle->sceneBoundingRect().center();
        if (QLineF(scenePos, handlePos).length() <= tolerance) {
            return TransformHandle::Center;
        }
    }
    
    // 检查旋转手柄
    if (m_rotateHandle && m_rotateHandle->isVisible()) {
        QPointF handlePos = m_rotateHandle->sceneBoundingRect().center();
        if (QLineF(scenePos, handlePos).length() <= tolerance) {
            return TransformHandle::Rotate;
        }
    }
    
    return TransformHandle::None;
}

void HandleManager::setActiveHandle(TransformHandle::HandleType type)
{
    m_activeHandle = type;
    updateHandles(m_bounds); // 重新更新以应用颜色变化
}

QPointF HandleManager::getHandlePosition(TransformHandle::HandleType type) const
{
    const qreal handleSize = getHandleSize();
    const qreal halfSize = handleSize / 2.0;
    
    switch (type) {
        case TransformHandle::TopLeft:     return m_bounds.topLeft();
        case TransformHandle::TopRight:    return m_bounds.topRight();
        case TransformHandle::BottomLeft:  return m_bounds.bottomLeft();
        case TransformHandle::BottomRight: return m_bounds.bottomRight();
        case TransformHandle::Left:        return QPointF(m_bounds.left(), m_bounds.center().y());
            case TransformHandle::Right:       return QPointF(m_bounds.right(), m_bounds.center().y());
            case TransformHandle::Top:         return QPointF(m_bounds.center().x(), m_bounds.top());
            case TransformHandle::Bottom:      return QPointF(m_bounds.center().x(), m_bounds.bottom());
        case TransformHandle::Center:      return m_bounds.center();
        case TransformHandle::Rotate: {
            QPointF rotatePos = QPointF(m_bounds.center().x(), m_bounds.top());
            rotatePos.setY(rotatePos.y() - 20.0);
            return rotatePos;
        }
        default: return QPointF();
    }
}

void HandleManager::setCenterHandlePosition(const QPointF &pos)
{
    if (m_centerHandle)
    {
        updateHandlePosition(TransformHandle::Center, pos);
    }
}