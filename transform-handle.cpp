#include "transform-handle.h"
#include "drawingscene.h"
#include <QGraphicsScene>
#include <QBrush>
#include <QPen>

// 静态颜色定义 - 现代化配色方案
const QColor HandleManager::HANDLE_COLOR = QColor(255, 255, 255, 220);  // 更不透明的白色
const QColor HandleManager::ACTIVE_HANDLE_COLOR = QColor(41, 121, 255, 255);  // 鲜明的蓝色
const QColor HandleManager::ROTATE_HANDLE_COLOR = QColor(76, 175, 80, 255);  // 鲜明的绿色

HandleManager::HandleManager(DrawingScene *scene, QObject *parent)
    : QObject(parent), m_scene(scene), m_handleMode(TransformHandle::Scale), m_activeHandle(TransformHandle::None), m_centerHandle(nullptr), m_rotateHandle(nullptr), m_selectionBorder(nullptr)
{
    createHandles();
}

HandleManager::~HandleManager()
{
    destroyHandles();
}

void HandleManager::createHandles()
{
    if (!m_scene)
    {
        return;
    }

    // 创建角点手柄
    for (int i = 0; i < 4; ++i)
    {
        QGraphicsRectItem *handle = new QGraphicsRectItem(0, 0, getHandleSize(), getHandleSize());
        handle->setBrush(QBrush(HANDLE_COLOR));
        handle->setPen(QPen(QColor(0, 0, 0, 180), 1)); // 半透明黑色边框
        handle->setZValue(2000);
        handle->setVisible(false);
        // 设置透明度
        handle->setOpacity(0.9);
        m_scene->addItem(handle);
        m_cornerHandles.append(handle);
    }

    // 创建边缘手柄
    for (int i = 0; i < 4; ++i)
    {
        QGraphicsRectItem *handle = new QGraphicsRectItem(0, 0, getHandleSize(), getHandleSize());
        handle->setBrush(QBrush(HANDLE_COLOR));
        handle->setPen(QPen(QColor(0, 0, 0, 180), 1)); // 半透明黑色边框
        handle->setZValue(2000);
        handle->setVisible(false);
        // 设置透明度
        handle->setOpacity(0.9);
        m_scene->addItem(handle);
        m_edgeHandles.append(handle);
    }

    // 创建中心手柄
    m_centerHandle = new QGraphicsEllipseItem(0, 0, getHandleSize() * 1.2, getHandleSize() * 1.2);
    m_centerHandle->setBrush(QBrush(HANDLE_COLOR));
    m_centerHandle->setPen(QPen(QColor(0, 0, 0, 180), 1)); // 半透明黑色边框
    m_centerHandle->setZValue(2000);
    m_centerHandle->setVisible(false);
    // 设置透明度
    m_centerHandle->setOpacity(0.9);
    m_scene->addItem(m_centerHandle);

    // 创建旋转手柄
    m_rotateHandle = new QGraphicsEllipseItem(0, 0, getHandleSize() * 1.2, getHandleSize() * 1.2);
    m_rotateHandle->setBrush(QBrush(ROTATE_HANDLE_COLOR));
    m_rotateHandle->setPen(QPen(QColor(0, 0, 0, 180), 1)); // 半透明黑色边框
    m_rotateHandle->setZValue(2000);
    m_rotateHandle->setVisible(false);
    // 设置透明度
    m_rotateHandle->setOpacity(0.9);
    m_scene->addItem(m_rotateHandle);
    
    // 创建4个角点的旋转手柄（用于旋转模式）
    for (int i = 0; i < 4; ++i)
    {
        QGraphicsEllipseItem *handle = new QGraphicsEllipseItem(0, 0, getHandleSize() * 1.2, getHandleSize() * 1.2);
        handle->setBrush(QBrush(ROTATE_HANDLE_COLOR));
        handle->setPen(QPen(QColor(0, 0, 0, 180), 1)); // 半透明黑色边框
        handle->setZValue(2000);
        handle->setVisible(false);
        handle->setOpacity(0.9);
        m_scene->addItem(handle);
        m_rotateCornerHandles.append(handle);
    }
    
    // 创建选择边框线
    m_selectionBorder = new QGraphicsRectItem();
    m_selectionBorder->setBrush(Qt::NoBrush);
    m_selectionBorder->setPen(QPen(QColor(100, 149, 237, 150), 1, Qt::DashLine)); // 蓝色虚线边框
    m_selectionBorder->setZValue(1999); // 在手柄下方
    m_selectionBorder->setVisible(false);
    m_scene->addItem(m_selectionBorder);
}

// 🌟 检查并确保手柄被添加到场景中
void HandleManager::ensureHandlesInScene()
{
    if (!m_scene)
    {
        return;
    }

    // 检查角点手柄
    for (QGraphicsRectItem *handle : m_cornerHandles)
    {
        if (handle && handle->scene() != m_scene)
        {
            // 如果手柄不在正确的场景中，重新添加
            if (handle->scene())
            {
                handle->scene()->removeItem(handle);
            }
            m_scene->addItem(handle);
        }
    }

    // 检查边缘手柄
    for (QGraphicsRectItem *handle : m_edgeHandles)
    {
        if (handle && handle->scene() != m_scene)
        {
            // 如果手柄不在正确的场景中，重新添加
            if (handle->scene())
            {
                handle->scene()->removeItem(handle);
            }
            m_scene->addItem(handle);
        }
    }

    // 检查中心手柄
    if (m_centerHandle && m_centerHandle->scene() != m_scene)
    {
        if (m_centerHandle->scene())
        {
            m_centerHandle->scene()->removeItem(m_centerHandle);
        }
        m_scene->addItem(m_centerHandle);
    }

    // 检查旋转手柄
    if (m_rotateHandle && m_rotateHandle->scene() != m_scene)
    {
        if (m_rotateHandle->scene())
        {
            m_rotateHandle->scene()->removeItem(m_rotateHandle);
        }
        m_scene->addItem(m_rotateHandle);
    }
    
    // 检查旋转角点手柄
    for (QGraphicsEllipseItem *handle : m_rotateCornerHandles)
    {
        if (handle && handle->scene() != m_scene)
        {
            // 如果手柄不在正确的场景中，重新添加
            if (handle->scene())
            {
                handle->scene()->removeItem(handle);
            }
            m_scene->addItem(handle);
        }
    }
}

void HandleManager::destroyHandles()
{
    if (!m_scene)
    {
        return;
    }

    // 销毁角点手柄
    for (QGraphicsRectItem *handle : m_cornerHandles)
    {
        if (handle && handle->scene())
        {
            m_scene->removeItem(handle);
        }
        delete handle;
    }
    m_cornerHandles.clear();

    // 销毁边缘手柄
    for (QGraphicsRectItem *handle : m_edgeHandles)
    {
        if (handle && handle->scene())
        {
            m_scene->removeItem(handle);
        }
        delete handle;
    }
    m_edgeHandles.clear();

    // 销毁中心手柄
    if (m_centerHandle)
    {
        if (m_centerHandle->scene())
        {
            m_centerHandle->scene()->removeItem(m_centerHandle);
        }
        delete m_centerHandle;
        m_centerHandle = nullptr;
    }

    // 销毁旋转手柄
    if (m_rotateHandle)
    {
        if (m_rotateHandle->scene())
        {
            m_rotateHandle->scene()->removeItem(m_rotateHandle);
        }
        delete m_rotateHandle;
        m_rotateHandle = nullptr;
    }
    
    // 销毁旋转角点手柄
    for (QGraphicsEllipseItem *handle : m_rotateCornerHandles)
    {
        if (handle && handle->scene())
        {
            m_scene->removeItem(handle);
        }
        delete handle;
    }
    m_rotateCornerHandles.clear();
    
    // 销毁选择边框线
    if (m_selectionBorder)
    {
        if (m_selectionBorder->scene())
        {
            m_scene->removeItem(m_selectionBorder);
        }
        delete m_selectionBorder;
        m_selectionBorder = nullptr;
    }
}
void HandleManager::setHandleMode(TransformHandle::HandleMode mode)
{
    if (m_handleMode != mode)
    {
        m_handleMode = mode;
        // 根据模式更新手柄显示
        updateHandlesVisibility();
    }
}

void HandleManager::updateHandlesVisibility()
{
    // 隐藏所有手柄
    for (auto handle : m_cornerHandles) {
        if (handle) handle->setVisible(false);
    }
    for (auto handle : m_edgeHandles) {
        if (handle) handle->setVisible(false);
    }
    for (auto handle : m_rotateCornerHandles) {
        if (handle) handle->setVisible(false);
    }
    if (m_centerHandle) m_centerHandle->setVisible(false);
    if (m_rotateHandle) m_rotateHandle->setVisible(false);
    
    // 根据模式显示相应的手柄
    if (m_handleMode == TransformHandle::Scale)
    {
        // 缩放模式：显示8个缩放手柄
        for (auto handle : m_cornerHandles) {
            if (handle) handle->setVisible(true);
        }
        for (auto handle : m_edgeHandles) {
            if (handle) handle->setVisible(true);
        }
    }
    else if (m_handleMode == TransformHandle::RotateMode)
    {
        // 旋转模式：显示4个旋转手柄+中心手柄
        for (auto handle : m_rotateCornerHandles) {
            if (handle) handle->setVisible(true);
        }
        if (m_centerHandle) m_centerHandle->setVisible(true);
    }
}

void HandleManager::showHandles()
{
    if (!m_scene)
        return;

    // 🌟 确保所有手柄都在正确的场景中
    ensureHandlesInScene();
    
    // 根据当前模式更新手柄显示
    updateHandlesVisibility();
}

void HandleManager::hideHandles()
{
    // 设置手柄为隐藏状态
    setShowHandles(false);
    
    for (QGraphicsRectItem *handle : m_cornerHandles)
    {
        if (handle)
        {
            handle->setVisible(false);
        }
    }
    for (QGraphicsRectItem *handle : m_edgeHandles)
    {
        if (handle)
        {
            handle->setVisible(false);
        }
    }
    if (m_centerHandle)
    {
        m_centerHandle->setVisible(false);
    }
    if (m_rotateHandle)
    {
        m_rotateHandle->setVisible(false);
    }
    for (QGraphicsEllipseItem *handle : m_rotateCornerHandles) {
        if (handle) handle->setVisible(false);
    }
    if (m_selectionBorder)
    {
        m_selectionBorder->setVisible(false);
    }
}

void HandleManager::updateHandles(const QRectF &bounds)
{
    m_bounds = bounds;
    if (bounds.isEmpty())
        return;

    // 🌟 确保所有手柄都在正确的场景中
    ensureHandlesInScene();

    const qreal handleSize = getHandleSize();
    const qreal halfSize = handleSize / 2.0;
    const qreal offset = 4.0; // 手柄向外偏移量，避免遮挡图形

    if (m_handleMode == TransformHandle::Scale)
    {
        // 缩放模式：更新8个缩放手柄
        updateHandlePosition(TransformHandle::TopLeft, bounds.topLeft() + QPointF(-offset, -offset));
        updateHandlePosition(TransformHandle::TopRight, bounds.topRight() + QPointF(offset, -offset));
        updateHandlePosition(TransformHandle::BottomLeft, bounds.bottomLeft() + QPointF(-offset, offset));
        updateHandlePosition(TransformHandle::BottomRight, bounds.bottomRight() + QPointF(offset, offset));

        updateHandlePosition(TransformHandle::Left, QPointF(bounds.left() - offset, bounds.center().y()));
        updateHandlePosition(TransformHandle::Right, QPointF(bounds.right() + offset, bounds.center().y()));
        updateHandlePosition(TransformHandle::Top, QPointF(bounds.center().x(), bounds.top() - offset));
        updateHandlePosition(TransformHandle::Bottom, QPointF(bounds.center().x(), bounds.bottom() + offset));
    }
    else if (m_handleMode == TransformHandle::RotateMode)
    {
        // 旋转模式：更新4个旋转手柄+中心手柄
        // 4个角点的旋转手柄位置（比缩放手柄更远一些）
        const qreal rotateOffset = offset;
        updateRotateCornerHandle(0, bounds.topLeft() + QPointF(-rotateOffset, -rotateOffset));     // TopLeft
        updateRotateCornerHandle(1, bounds.topRight() + QPointF(rotateOffset, -rotateOffset));    // TopRight
        updateRotateCornerHandle(2, bounds.bottomLeft() + QPointF(-rotateOffset, rotateOffset));   // BottomLeft
        updateRotateCornerHandle(3, bounds.bottomRight() + QPointF(rotateOffset, rotateOffset));  // BottomRight
        
        // 更新中心手柄
        updateHandlePosition(TransformHandle::Center, bounds.center());
    }
    
    // 更新选择边框线
    if (m_selectionBorder)
    {
        // 稍微扩展边框，使其包围手柄
        QRectF borderRect = bounds.adjusted(-offset, -offset, offset, offset);
        m_selectionBorder->setRect(borderRect);
        m_selectionBorder->setVisible(true);
    }
    
    // 根据模式更新手柄显示
    updateHandlesVisibility();
}

void HandleManager::updateRotateCornerHandle(int index, const QPointF &pos)
{
    if (index < 0 || index >= m_rotateCornerHandles.size())
        return;

    QGraphicsEllipseItem *handle = m_rotateCornerHandles[index];
    if (!handle)
        return;

    const qreal handleSize = getHandleSize() * 1.2;
    const qreal halfSize = handleSize / 2.0;

    handle->setPos(pos.x() - halfSize, pos.y() - halfSize);

    // 更新活动手柄颜色
    if (TransformHandle::HandleType(index + TransformHandle::TopLeft) == m_activeHandle)
    {
        handle->setBrush(QBrush(ACTIVE_HANDLE_COLOR));
    }
    else
    {
        handle->setBrush(QBrush(ROTATE_HANDLE_COLOR));
    }
}

void HandleManager::updateHandlePosition(TransformHandle::HandleType type, const QPointF &pos)
{
    const qreal handleSize = getHandleSize();
    const qreal halfSize = handleSize / 2.0;
    const qreal centerSize = handleSize * 1.5 / 2.0;
    const qreal rotateSize = handleSize * 1.2 / 2.0;

    QGraphicsItem *handle = nullptr;

    switch (type)
    {
    case TransformHandle::TopLeft:
        if (m_cornerHandles.size() > 0)
            handle = m_cornerHandles[0];
        break;
    case TransformHandle::TopRight:
        if (m_cornerHandles.size() > 1)
            handle = m_cornerHandles[1];
        break;
    case TransformHandle::BottomLeft:
        if (m_cornerHandles.size() > 2)
            handle = m_cornerHandles[2];
        break;
    case TransformHandle::BottomRight:
        if (m_cornerHandles.size() > 3)
            handle = m_cornerHandles[3];
        break;
    case TransformHandle::Left:
        if (m_edgeHandles.size() > 0)
            handle = m_edgeHandles[0];
        break;
    case TransformHandle::Right:
        if (m_edgeHandles.size() > 1)
            handle = m_edgeHandles[1];
        break;
    case TransformHandle::Top:
        if (m_edgeHandles.size() > 2)
            handle = m_edgeHandles[2];
        break;
    case TransformHandle::Bottom:
        if (m_edgeHandles.size() > 3)
            handle = m_edgeHandles[3];
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

    if (!handle)
        return;

    // 根据手柄类型调整位置
    if (type == TransformHandle::Center)
    {
        handle->setPos(pos.x() - centerSize, pos.y() - centerSize);
    }
    else if (type == TransformHandle::Rotate)
    {
        handle->setPos(pos.x() - rotateSize, pos.y() - rotateSize);
    }
    else
    {
        handle->setPos(pos.x() - halfSize, pos.y() - halfSize);
    }

    // 更新活动手柄颜色
    if (type == m_activeHandle)
    {
        if (QGraphicsRectItem *rectHandle = qgraphicsitem_cast<QGraphicsRectItem *>(handle))
        {
            rectHandle->setBrush(QBrush(ACTIVE_HANDLE_COLOR));
        }
        else if (QGraphicsEllipseItem *ellipseHandle = qgraphicsitem_cast<QGraphicsEllipseItem *>(handle))
        {
            ellipseHandle->setBrush(QBrush(ACTIVE_HANDLE_COLOR));
        }
    }
    else
    {
        if (QGraphicsRectItem *rectHandle = qgraphicsitem_cast<QGraphicsRectItem *>(handle))
        {
            rectHandle->setBrush(QBrush(HANDLE_COLOR));
        }
        else if (QGraphicsEllipseItem *ellipseHandle = qgraphicsitem_cast<QGraphicsEllipseItem *>(handle))
        {
            if (type == TransformHandle::Rotate)
            {
                ellipseHandle->setBrush(QBrush(ROTATE_HANDLE_COLOR));
            }
            else
            {
                ellipseHandle->setBrush(QBrush(HANDLE_COLOR));
            }
        }
    }
}

TransformHandle::HandleType HandleManager::getHandleAtPosition(const QPointF &scenePos) const
{
    const qreal tolerance = getHandleSize() / 2.0 + 2.0; // 添加一些容差

    // 检查旋转角点手柄（旋转模式下）
    for (int i = 0; i < m_rotateCornerHandles.size(); ++i)
    {
        if (m_rotateCornerHandles[i] && m_rotateCornerHandles[i]->isVisible())
        {
            QPointF handlePos = m_rotateCornerHandles[i]->sceneBoundingRect().center();
            if (QLineF(scenePos, handlePos).length() <= tolerance)
            {
                // 旋转模式下的角点手柄返回Rotate类型
                return TransformHandle::Rotate;
            }
        }
    }

    // 检查角点手柄
    for (int i = 0; i < m_cornerHandles.size(); ++i)
    {
        if (m_cornerHandles[i] && m_cornerHandles[i]->isVisible())
        {
            QPointF handlePos = m_cornerHandles[i]->sceneBoundingRect().center();
            if (QLineF(scenePos, handlePos).length() <= tolerance)
            {
                switch (i)
                {
                case 0:
                    return TransformHandle::TopLeft;
                case 1:
                    return TransformHandle::TopRight;
                case 2:
                    return TransformHandle::BottomLeft;
                case 3:
                    return TransformHandle::BottomRight;
                }
            }
        }
    }

    // 检查边缘手柄
    for (int i = 0; i < m_edgeHandles.size(); ++i)
    {
        if (m_edgeHandles[i] && m_edgeHandles[i]->isVisible())
        {
            QPointF handlePos = m_edgeHandles[i]->sceneBoundingRect().center();
            if (QLineF(scenePos, handlePos).length() <= tolerance)
            {
                switch (i)
                {
                case 0:
                    return TransformHandle::Left;
                case 1:
                    return TransformHandle::Right;
                case 2:
                    return TransformHandle::Top;
                case 3:
                    return TransformHandle::Bottom;
                }
            }
        }
    }

    // 检查中心手柄
    if (m_centerHandle && m_centerHandle->isVisible())
    {
        QPointF handlePos = m_centerHandle->sceneBoundingRect().center();
        if (QLineF(scenePos, handlePos).length() <= tolerance)
        {
            return TransformHandle::Center;
        }
    }

    // 检查旋转手柄
    if (m_rotateHandle && m_rotateHandle->isVisible())
    {
        QPointF handlePos = m_rotateHandle->sceneBoundingRect().center();
        if (QLineF(scenePos, handlePos).length() <= tolerance)
        {
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

    switch (type)
    {
    case TransformHandle::TopLeft:
        return m_bounds.topLeft();
    case TransformHandle::TopRight:
        return m_bounds.topRight();
    case TransformHandle::BottomLeft:
        return m_bounds.bottomLeft();
    case TransformHandle::BottomRight:
        return m_bounds.bottomRight();
    case TransformHandle::Left:
        return QPointF(m_bounds.left(), m_bounds.center().y());
    case TransformHandle::Right:
        return QPointF(m_bounds.right(), m_bounds.center().y());
    case TransformHandle::Top:
        return QPointF(m_bounds.center().x(), m_bounds.top());
    case TransformHandle::Bottom:
        return QPointF(m_bounds.center().x(), m_bounds.bottom());
    case TransformHandle::Center:
        return m_bounds.center();
    case TransformHandle::Rotate:
    {
        QPointF rotatePos = QPointF(m_bounds.center().x(), m_bounds.top());
        rotatePos.setY(rotatePos.y() - 20.0);
        return rotatePos;
    }
    default:
        return QPointF();
    }
}

void HandleManager::setCenterHandlePosition(const QPointF &pos)
{
    if (m_centerHandle)
    {
        updateHandlePosition(TransformHandle::Center, pos);
    }
}

void HandleManager::setShowHandles(bool show)
{
    m_shouldShowHandles = show;
}

bool HandleManager::shouldShowHandles() const
{
    return m_shouldShowHandles;
}

void HandleManager::updateHandleHover(const QPointF &scenePos)
{
    TransformHandle::HandleType hoveredHandle = getHandleAtPosition(scenePos);
    
    // 重置所有手柄的悬停状态
    for (QGraphicsRectItem *handle : m_cornerHandles)
    {
        updateHandleHoverEffect(handle, false);
    }
    for (QGraphicsRectItem *handle : m_edgeHandles)
    {
        updateHandleHoverEffect(handle, false);
    }
    if (m_centerHandle)
    {
        updateHandleHoverEffect(m_centerHandle, false);
    }
    if (m_rotateHandle)
    {
        updateHandleHoverEffect(m_rotateHandle, false);
    }
    for (QGraphicsEllipseItem *handle : m_rotateCornerHandles)
    {
        updateHandleHoverEffect(handle, false);
    }
    
    // 设置悬停手柄的效果
    if (hoveredHandle != TransformHandle::None)
    {
        QGraphicsItem *hoveredItem = nullptr;
        
        // 找到对应的手柄项
        switch (hoveredHandle)
        {
        case TransformHandle::TopLeft:
        case TransformHandle::TopRight:
        case TransformHandle::BottomLeft:
        case TransformHandle::BottomRight:
            if (m_handleMode == TransformHandle::Scale)
                hoveredItem = m_cornerHandles[hoveredHandle - TransformHandle::TopLeft];
            else
                hoveredItem = m_rotateCornerHandles[hoveredHandle - TransformHandle::TopLeft];
            break;
        case TransformHandle::Left:
        case TransformHandle::Right:
        case TransformHandle::Top:
        case TransformHandle::Bottom:
            if (hoveredHandle >= TransformHandle::Left && hoveredHandle <= TransformHandle::Bottom)
                hoveredItem = m_edgeHandles[hoveredHandle - TransformHandle::Left];
            break;
        case TransformHandle::Center:
            hoveredItem = m_centerHandle;
            break;
        case TransformHandle::Rotate:
            hoveredItem = m_rotateHandle;
            break;
        default:
            break;
        }
        
        if (hoveredItem)
        {
            updateHandleHoverEffect(hoveredItem, true);
        }
    }
}

void HandleManager::updateHandleHoverEffect(QGraphicsItem *handle, bool isHovered)
{
    if (!handle)
        return;
    
    if (isHovered)
    {
        // 悬停效果：放大并改变颜色
        handle->setScale(1.2);
        
        if (QGraphicsRectItem *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(handle))
        {
            rectItem->setBrush(QBrush(ACTIVE_HANDLE_COLOR));
        }
        else if (QGraphicsEllipseItem *ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(handle))
        {
            ellipseItem->setBrush(QBrush(ACTIVE_HANDLE_COLOR));
        }
    }
    else
    {
        // 恢复正常状态
        handle->setScale(1.0);
        
        if (QGraphicsRectItem *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(handle))
        {
            rectItem->setBrush(QBrush(HANDLE_COLOR));
        }
        else if (QGraphicsEllipseItem *ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(handle))
        {
            // 检查是否是旋转手柄
            if (ellipseItem == m_rotateHandle || m_rotateCornerHandles.contains(ellipseItem))
            {
                ellipseItem->setBrush(QBrush(ROTATE_HANDLE_COLOR));
            }
            else
            {
                ellipseItem->setBrush(QBrush(HANDLE_COLOR));
            }
        }
    }
}