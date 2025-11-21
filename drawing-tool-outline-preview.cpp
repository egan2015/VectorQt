#include "drawing-tool-outline-preview.h"
#include "drawingscene.h"
#include "drawingview.h"
#include "drawing-document.h"
#include "drawing-canvas.h"
#include "drawing-shape.h"
#include "drawing-layer.h"
// #include "selection-layer.h" // 已移除 - 老的选择层系统
#include "drawing-transform.h"
#include "handle-item.h"
#include "transform-handle.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include <QtMath>
#include <QTimer>
#include <QDebug>

namespace
{
    inline qreal safeDiv(qreal a, qreal b)
    {
        return (qAbs(b) < 1e-6) ? 1.0 : a / b;
    }

    inline QPointF getHandlePos(const QRectF &rect, TransformHandle::HandleType type)
    {
        switch (type)
        {
        case TransformHandle::TopLeft:
            return rect.topLeft();
        case TransformHandle::TopRight:
            return rect.topRight();
        case TransformHandle::BottomLeft:
            return rect.bottomLeft();
        case TransformHandle::BottomRight:
            return rect.bottomRight();
        case TransformHandle::Left:
            return QPointF(rect.left(), rect.center().y());
        case TransformHandle::Right:
            return QPointF(rect.right(), rect.center().y());
        case TransformHandle::Top:
            return QPointF(rect.center().x(), rect.top());
        case TransformHandle::Bottom:
            return QPointF(rect.center().x(), rect.bottom());
        case TransformHandle::Center:
            return rect.center();
        default:
            return rect.center();
        }
    }
}

OutlinePreviewTransformTool::OutlinePreviewTransformTool(QObject *parent)
    : ToolBase(parent), m_anchorPoint(nullptr), m_dragPoint(nullptr), m_outlinePreview(nullptr), m_handleManager(nullptr)
{
}

OutlinePreviewTransformTool::~OutlinePreviewTransformTool()
{
    deactivate(); // 确保清理

    // 清理 HandleManager
    if (m_handleManager)
    {
        delete m_handleManager;
        m_handleManager = nullptr;
    }
}

void OutlinePreviewTransformTool::setRotationCenter(const QPointF &center)
{
    m_useCustomRotationCenter = true;
    m_customRotationCenter = center;

    // 如果有选中对象，更新手柄位置和旋转中心显示
    if (m_scene && !m_scene->selectedItems().isEmpty())
    {
        updateHandlePositions();
        updateVisualHelpers(QPointF());
    }
}

void OutlinePreviewTransformTool::resetRotationCenter()
{
    m_useCustomRotationCenter = false;
    m_customRotationCenter = QPointF();

    // 如果有选中对象，更新显示
    if (m_scene && !m_scene->selectedItems().isEmpty())
    {
        updateHandlePositions();
        updateVisualHelpers(QPointF());
    }
}

bool OutlinePreviewTransformTool::hasCustomRotationCenter() const
{
    return m_useCustomRotationCenter;
}

void OutlinePreviewTransformTool::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    if (view)
    {
        view->setDragMode(QGraphicsView::RubberBandDrag);
        // 确保视图可以接收键盘事件
        view->setFocusPolicy(Qt::StrongFocus);
        view->setFocus();
    }

    // 每次激活时都重新创建 HandleManager，确保场景指针正确
    if (m_handleManager)
    {
        delete m_handleManager;
        m_handleManager = nullptr;
    }

    if (scene)
    {
        m_handleManager = new HandleManager(scene, this);
        // 设置默认模式
        m_handleManager->setHandleMode(m_currentMode);
    }

    // 显示初始模式提示
    QString modeText = (m_currentMode == HandleMode::Scale) ? tr("缩放模式") : tr("旋转模式");
    emit statusMessageChanged(modeText + " - 按空格键或Tab键切换模式");

    // 连接选择变化信号
    if (scene)
    {
        connect(scene, &DrawingScene::selectionChanged, this,
                &OutlinePreviewTransformTool::onSelectionChanged, Qt::UniqueConnection);
        connect(scene, &DrawingScene::objectStateChanged, this,
                &OutlinePreviewTransformTool::onObjectStateChanged, Qt::UniqueConnection);

        // 禁用所有选中图形的内部选择框
        disableInternalSelectionIndicators();

        // 初始显示手柄
        updateHandlePositions();
    }
}

void OutlinePreviewTransformTool::deactivate()
{
    if (m_state == STATE_GRABBED)
    {
        ungrab(false); // 取消变换
    }
    else if (m_state == STATE_DRAG_CENTER)
    {
        m_state = STATE_IDLE; // 重置拖动中心状态
    }

    if (m_view)
        m_view->setDragMode(QGraphicsView::NoDrag);
    if (m_handleManager)
    {
        m_handleManager->hideHandles();
        delete m_handleManager;
        m_handleManager = nullptr;
    }

    // 恢复内部选择框
    enableInternalSelectionIndicators();

    ToolBase::deactivate();
}

bool OutlinePreviewTransformTool::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || event->button() != Qt::LeftButton)
        return false;

    // 如果已在变换中，先结束
    if (m_state == STATE_GRABBED)
    {
        ungrab(true);
    }

    // 检查手柄
    if (m_handleManager)
    {
        TransformHandle::HandleType handle = m_handleManager->getHandleAtPosition(scenePos);
        if (handle != TransformHandle::None)
        {
            if (handle == TransformHandle::Center)
            {
                // 开始拖动旋转中心
                m_state = STATE_DRAG_CENTER;
                m_grabMousePos = scenePos;
                return true;
            }
            else
            {
                grab(handle, scenePos, event->modifiers());
                return true;
            }
        }
    }

    QGraphicsItem *item = m_scene->itemAt(scenePos, QTransform());
    for (QGraphicsItem *item : m_scene->selectedItems())
    {
        qDebug() << "Currently selected item:" << item;
        if (item)
        {
            // 如果点击了图形
            if (event->modifiers() & Qt::ControlModifier)
            {
                // Ctrl+点击：切换选择状态
                if (item->isSelected())
                {
                    item->setSelected(false);
                }
                else
                {
                    item->setSelected(true);
                }

                // 更新手柄位置
                QTimer::singleShot(10, this, [this]()
                                   { 
                                   disableInternalSelectionIndicators();
                                   updateHandlePositions(); });
                return false;
            }
            else if (!item->isSelected())
            {
                // 普通点击：如果图形未被选中，清除其他选择并选中当前图形
                m_scene->clearSelection();
                item->setSelected(true);

                // 更新手柄位置
                QTimer::singleShot(10, this, [this]()
                                   { 
                                   disableInternalSelectionIndicators();
                                   updateHandlePositions(); });
            }
            else
            {
                qDebug() << "Clicked on item:" << item;
                return false;
            }
        }
    }

    if (m_scene && m_scene->selectedItems().isEmpty())
    {
        // 点击空白处，清除选择
        m_scene->clearSelection();
        // 重置旋转中心位置，让它跟随新的选择
        resetRotationCenter();

        qDebug() << "Clicked on empty space";
    }
    // 不消费事件，让场景处理框选
    return false;
}

bool OutlinePreviewTransformTool::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (!m_scene || !(event->buttons() & Qt::LeftButton))
    {
        // 悬停光标和手柄效果
        if (m_handleManager && m_view && m_state == STATE_IDLE)
        {
            TransformHandle::HandleType handle = m_handleManager->getHandleAtPosition(scenePos);
            if (handle == TransformHandle::Center)
                m_view->setCursor(Qt::SizeAllCursor);
            else if (handle != TransformHandle::None)
                m_view->setCursor(Qt::CrossCursor);
            else
                m_view->setCursor(Qt::ArrowCursor);

            // 更新手柄悬停效果
            m_handleManager->updateHandleHover(scenePos);
        }
        return false;
    }

    if (m_state == STATE_DRAG_CENTER)
    {
        // Apply smart snapping to rotation center position
        QPointF alignedPos = scenePos;
        if (m_scene && m_scene->isGridAlignmentEnabled())
        {
            // 使用智能网格吸附
            DrawingScene::SnapResult gridSnap = m_scene->smartAlignToGrid(scenePos);
            alignedPos = gridSnap.snappedPos;

            // 尝试对象吸附
            DrawingScene::ObjectSnapResult objectSnap = m_scene->snapToObjects(scenePos, nullptr);
            if (objectSnap.snappedToObject)
            {
                // 对象吸附优先级更高
                alignedPos = objectSnap.snappedPos;
            }
        }

        // 实时更新旋转中心位置（使用对齐后的位置）
        setRotationCenter(alignedPos);
        return true;
    }
    else if (m_state == STATE_GRABBED)
    {
        transform(scenePos, event->modifiers());
        return true;
    }

    return false;
}

bool OutlinePreviewTransformTool::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_state == STATE_DRAG_CENTER)
    {
        // Apply smart snapping to final rotation center position
        QPointF alignedPos = scenePos;
        if (m_scene && m_scene->isGridAlignmentEnabled())
        {
            // 使用智能网格吸附
            DrawingScene::SnapResult gridSnap = m_scene->smartAlignToGrid(scenePos);
            alignedPos = gridSnap.snappedPos;

            // 尝试对象吸附
            DrawingScene::ObjectSnapResult objectSnap = m_scene->snapToObjects(scenePos, nullptr);
            if (objectSnap.snappedToObject)
            {
                // 对象吸附优先级更高
                alignedPos = objectSnap.snappedPos;
            }
        }

        // 完成旋转中心设置（使用对齐后的位置）
        setRotationCenter(alignedPos);
        m_state = STATE_IDLE;
        return true;
    }
    else if (m_state == STATE_GRABBED)
    {
        ungrab(true, scenePos); // 应用变换
        return true;
    }
    // 更新手柄位置
    QTimer::singleShot(10, this, [this]()
                       { 
                               disableInternalSelectionIndicators();
                               updateHandlePositions(); });
    return false;
}

bool OutlinePreviewTransformTool::keyPressEvent(QKeyEvent *event)
{
    // 调试输出
    qDebug() << "OutlinePreviewTransformTool::keyPressEvent called, key:" << event->key();

    // ESC 取消变换
    if (event->key() == Qt::Key_Escape && m_state == STATE_GRABBED)
    {
        ungrab(false); // 不应用，直接取消
        return true;
    }

    // 空格键或Tab键切换模式
    if ((event->key() == Qt::Key_Space || event->key() == Qt::Key_Tab) &&
        m_scene && !m_scene->selectedItems().isEmpty())
    {
        qDebug() << "Toggling mode due to key press";
        toggleMode();
        return true;
    }

    return false;
}

// ==================== 核心状态机 ====================

void OutlinePreviewTransformTool::grab(TransformHandle::HandleType handleType,
                                       const QPointF &mousePos,
                                       Qt::KeyboardModifiers modifiers)
{

    // 如果已经在变换中，先结束当前变换
    if (m_state == STATE_GRABBED)
    {

        ungrab(true); // 结束当前变换
    }

    m_state = STATE_GRABBED;
    m_activeHandle = handleType;
    m_grabMousePos = mousePos;

    // 获取选中的图形
    QList<QGraphicsItem *> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty())
    {
        resetState();
        return;
    }

    // 确定变换类型：如果是旋转模式下的角点手柄，或旋转手柄，则为旋转；否则为缩放
    DrawingScene::TransformType transformType = DrawingScene::Scale;
    if (m_activeHandle == TransformHandle::Rotate ||
        (m_handleManager && m_handleManager->handleMode() == HandleMode::RotateMode &&
         (m_activeHandle == TransformHandle::TopLeft || m_activeHandle == TransformHandle::TopRight ||
          m_activeHandle == TransformHandle::BottomLeft || m_activeHandle == TransformHandle::BottomRight)))
    {
        transformType = DrawingScene::Rotate;
    }
    m_scene->beginTransform(transformType);

    // 保存所有选中的图形和它们的初始变换
    m_selectedShapes.clear();
    m_originalTransforms.clear();

    for (QGraphicsItem *item : selectedItems)
    {
        DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
        if (shape)
        {
            m_selectedShapes.append(shape);
            m_originalTransforms[shape] = shape->transform().transform();
        }
    }

    // 计算基准数据（必须在填充m_selectedShapes之后）
    m_initialBounds = calculateInitialSelectionBounds();

    // 初始化选择框的本地坐标矩阵
    m_oppositeHandle = calculateOpposite(m_initialBounds, handleType);
    m_transformOrigin = calculateOrigin(m_initialBounds, m_oppositeHandle, modifiers);
    m_handleBounds = m_initialBounds; // 手柄始终基于初始边界

    // 计算并保存固定的缩放锚点
    QPointF ironPlateCenter = m_initialBounds.center();
    switch (handleType)
    {
    case TransformHandle::Right:
        m_scaleAnchor = QPointF(m_initialBounds.left(), ironPlateCenter.y());
        break;
    case TransformHandle::Left:
        m_scaleAnchor = QPointF(m_initialBounds.right(), ironPlateCenter.y());
        break;
    case TransformHandle::Bottom:
        m_scaleAnchor = QPointF(ironPlateCenter.x(), m_initialBounds.top());
        break;
    case TransformHandle::Top:
        m_scaleAnchor = QPointF(ironPlateCenter.x(), m_initialBounds.bottom());
        break;
    case TransformHandle::BottomRight:
        m_scaleAnchor = m_initialBounds.topLeft();
        break;
    case TransformHandle::TopLeft:
        m_scaleAnchor = m_initialBounds.bottomRight();
        break;
    case TransformHandle::TopRight:
        m_scaleAnchor = m_initialBounds.bottomLeft();
        break;
    case TransformHandle::BottomLeft:
        m_scaleAnchor = m_initialBounds.topRight();
        break;
    case TransformHandle::Center:
        m_scaleAnchor = m_initialBounds.center();
        break;
    default:
        m_scaleAnchor = m_initialBounds.center();
        break;
    }

    // 创建视觉辅助元素
    createVisualHelpers();

    // 开始变换时启动蚂蚁线效果
    if (m_outlinePreview && m_dashTimer)
    {
        QPen pen = m_outlinePreview->pen();
        pen.setDashPattern({8, 4}); // 蚂蚁线模式，加长虚线段
        pen.setColor(Qt::black);
        m_outlinePreview->setPen(pen);
        m_dashTimer->start(80); // 启动动画定时器
    }

    // 隐藏手柄，避免干扰预览
    if (m_handleManager)
    {
        m_handleManager->hideHandles();
        m_handleManager->setActiveHandle(handleType);
    }
}

void OutlinePreviewTransformTool::transform(const QPointF &mousePos, Qt::KeyboardModifiers modifiers)
{

    if (m_selectedShapes.isEmpty())
    {

        return;
    }

    // Apply smart snapping to mouse position
    QPointF alignedPos = mousePos;
    if (m_scene && m_scene->isGridAlignmentEnabled())
    {
        // 使用智能网格吸附
        DrawingScene::SnapResult gridSnap = m_scene->smartAlignToGrid(mousePos);
        alignedPos = gridSnap.snappedPos;

        // 尝试对象吸附（排除当前选中的图形）
        DrawingScene::ObjectSnapResult objectSnap = m_scene->snapToObjects(mousePos, nullptr);
        if (objectSnap.snappedToObject)
        {
            // 对象吸附优先级更高
            alignedPos = objectSnap.snappedPos;
        }
    }

    // 预先声明变换参数
    qreal sx = 1.0, sy = 1.0;

    // 处理缩放 - 基于鼠标移动的相对变化（旋转在循环中处理）
    if (m_activeHandle != TransformHandle::Rotate)
    {
        qreal initialWidth = m_initialBounds.width();
        qreal initialHeight = m_initialBounds.height();

        switch (m_activeHandle)
        {
        case TransformHandle::Right:
            sx = safeDiv(alignedPos.x() - m_scaleAnchor.x(), m_grabMousePos.x() - m_scaleAnchor.x());
            sy = 1.0;
            break;
        case TransformHandle::Left:
            sx = safeDiv(m_scaleAnchor.x() - alignedPos.x(), m_scaleAnchor.x() - m_grabMousePos.x());
            sy = 1.0;
            break;
        case TransformHandle::Bottom:
            sx = 1.0;
            sy = safeDiv(alignedPos.y() - m_scaleAnchor.y(), m_grabMousePos.y() - m_scaleAnchor.y());
            break;
        case TransformHandle::Top:
            sx = 1.0;
            sy = safeDiv(m_scaleAnchor.y() - alignedPos.y(), m_scaleAnchor.y() - m_grabMousePos.y());
            break;
        case TransformHandle::BottomRight:
            sx = safeDiv(alignedPos.x() - m_scaleAnchor.x(), m_grabMousePos.x() - m_scaleAnchor.x());
            sy = safeDiv(alignedPos.y() - m_scaleAnchor.y(), m_grabMousePos.y() - m_scaleAnchor.y());
            break;
        case TransformHandle::TopLeft:
            sx = safeDiv(m_scaleAnchor.x() - alignedPos.x(), m_scaleAnchor.x() - m_grabMousePos.x());
            sy = safeDiv(m_scaleAnchor.y() - alignedPos.y(), m_scaleAnchor.y() - m_grabMousePos.y());
            break;
        case TransformHandle::TopRight:
            sx = safeDiv(alignedPos.x() - m_scaleAnchor.x(), m_grabMousePos.x() - m_scaleAnchor.x());
            sy = safeDiv(m_scaleAnchor.y() - alignedPos.y(), m_scaleAnchor.y() - m_grabMousePos.y());
            break;
        case TransformHandle::BottomLeft:
            sx = safeDiv(m_scaleAnchor.x() - alignedPos.x(), m_scaleAnchor.x() - m_grabMousePos.x());
            sy = safeDiv(alignedPos.y() - m_scaleAnchor.y(), m_grabMousePos.y() - m_scaleAnchor.y());
            break;
        default:
            return;
        }

        sx = qBound(-10.0, sx, 10.0);
        sy = qBound(-10.0, sy, 10.0);
    }

    // Apply unified transform to all selected shapes
    for (DrawingShape *shape : m_selectedShapes)
    {
        if (!shape || !shape->scene())
            continue;  // 跳过无效的图形

        // 获取初始变换
        QTransform originalTransform = m_originalTransforms.value(shape, QTransform());

        // ✅ 为每个图形单独计算变换，使用统一的场景锚点
        QTransform individualTransform;

        if (m_activeHandle == TransformHandle::Rotate)
        {
            // Rotation: dynamically get current rotation center
            QPointF center = m_useCustomRotationCenter ? m_customRotationCenter : m_transformOrigin;
            qreal initialAngle = qAtan2(m_grabMousePos.y() - center.y(),
                                        m_grabMousePos.x() - center.x());
            qreal currentAngle = qAtan2(alignedPos.y() - center.y(),
                                        alignedPos.x() - center.x());
            qreal rotation = (currentAngle - initialAngle) * 180.0 / M_PI;

            // 将旋转中心转换为该图形的本地坐标
            QPointF shapeLocalAnchor = shape->mapFromScene(center);

            individualTransform.translate(shapeLocalAnchor.x(), shapeLocalAnchor.y());
            individualTransform.rotate(rotation);
            individualTransform.translate(-shapeLocalAnchor.x(), -shapeLocalAnchor.y());
        }
        else
        {
            // Scale: calculate scale for each shape based on scene anchor
            // 将场景锚点转换为该图形的本地坐标
            QPointF shapeLocalAnchor = shape->mapFromScene(m_scaleAnchor);

            individualTransform.translate(shapeLocalAnchor.x(), shapeLocalAnchor.y());
            individualTransform.scale(sx, sy);
            individualTransform.translate(-shapeLocalAnchor.x(), -shapeLocalAnchor.y());
        }

        // 应用变换：原始变换 * 该图形的个别变换
        QTransform newTransform = originalTransform * individualTransform;

        DrawingTransform drawingTransform;
        drawingTransform.setTransform(newTransform);
        shape->setTransform(drawingTransform);
        shape->updateShape(); // 刷新图形的边界和碰撞检测
    }

    // 更新视觉辅助元素（使用对齐后的位置）
    updateVisualHelpers(alignedPos);

    // Show scale or rotate hints
    if (m_scene) {
        if (m_activeHandle != TransformHandle::Rotate) {
            // Scale hint
            DrawingScene::ScaleHintResult scaleHint = m_scene->calculateScaleHint(sx, sy, alignedPos);
            m_scene->showScaleHint(scaleHint);
            m_scene->clearRotateHint(); // 清除旋转提示
        } else {
            // Rotate hint
            QPointF center = m_useCustomRotationCenter ? m_customRotationCenter : m_transformOrigin;
            qreal initialAngle = qAtan2(m_grabMousePos.y() - center.y(),
                                        m_grabMousePos.x() - center.x());
            qreal currentAngle = qAtan2(alignedPos.y() - center.y(),
                                        alignedPos.x() - center.x());
            qreal rotation = (currentAngle - initialAngle) * 180.0 / M_PI;
            
            DrawingScene::RotateHintResult rotateHint = m_scene->calculateRotateHint(rotation, alignedPos);
            m_scene->showRotateHint(rotateHint);
            m_scene->clearScaleHint(); // 清除缩放提示
        }
        
        m_scene->update();
    }
}

void OutlinePreviewTransformTool::ungrab(bool apply, const QPointF &finalMousePos)
{
    // Clear hints
    if (m_scene) {
        m_scene->clearScaleHint();
        m_scene->clearRotateHint();
    }
    
    if (apply)
    {
        // 应用变换 - 变换已经在transform()中直接应用到图形了
    }
    else
    {
        // 取消变换 - 恢复到初始变换
        for (DrawingShape *shape : m_selectedShapes)
        {
            if (!shape || !shape->scene())
                continue;  // 跳过无效的图形

            QTransform originalTransform = m_originalTransforms.value(shape, QTransform());
            DrawingTransform drawingTransform;
            drawingTransform.setTransform(originalTransform);
            shape->setTransform(drawingTransform);
        }
    }

    destroyVisualHelpers();

    // 不重置旋转中心位置，保持用户的设置

    // 重置状态，在更新手柄之前
    resetState();

    // 清除活动手柄状态
    if (m_handleManager)
    {
        m_handleManager->setActiveHandle(TransformHandle::None);
    }

    // 重新显示手柄
    updateHandlePositions();

    if (m_scene)
        m_scene->setModified(true);

    // 变换完成后更新选择
    if (m_scene)
    {
        m_scene->endTransform();
        // m_scene->emitSelectionChanged();
    }
}

QPointF OutlinePreviewTransformTool::calculateOpposite(const QRectF &bounds,
                                                       TransformHandle::HandleType type) const
{
    switch (type)
    {
    case TransformHandle::TopLeft:
        return bounds.bottomRight();
    case TransformHandle::TopRight:
        return bounds.bottomLeft();
    case TransformHandle::BottomLeft:
        return bounds.topRight();
    case TransformHandle::BottomRight:
        return bounds.topLeft();
    case TransformHandle::Left:
        return QPointF(bounds.right(), bounds.center().y());
    case TransformHandle::Right:
        return QPointF(bounds.left(), bounds.center().y());
    case TransformHandle::Top:
        return QPointF(bounds.center().x(), bounds.bottom());
    case TransformHandle::Bottom:
        return QPointF(bounds.center().x(), bounds.top());
    case TransformHandle::Center:
        return bounds.center();
    default:
        return bounds.center();
    }
}

QPointF OutlinePreviewTransformTool::calculateOrigin(const QRectF &bounds,
                                                     const QPointF &opposite,
                                                     Qt::KeyboardModifiers modifiers) const
{
    // SHIFT：围绕对角点
    if (modifiers & Qt::ShiftModifier)
    {
        return opposite;
    }

    // 优先使用自定义旋转中心
    if (m_useCustomRotationCenter)
    {
        return m_customRotationCenter;
    }

    // 默认：边界框中心（场景坐标）
    return bounds.center();
}

QRectF OutlinePreviewTransformTool::calculateInitialSelectionBounds() const
{
    // 如果在变换中，直接返回Group的边界
    if (m_state == STATE_GRABBED && !m_selectedShapes.isEmpty())
    {
        // 计算所有选中图形的联合边界框
        QRectF combinedBounds;
        bool first = true;
        for (DrawingShape *shape : m_selectedShapes)
        {
            if (!shape || !shape->scene())
                continue;  // 跳过无效的图形

            QRectF shapeBounds = shape->sceneBoundingRect();
            if (first)
            {
                combinedBounds = shapeBounds;
                first = false;
            }
            else
            {
                combinedBounds = combinedBounds.united(shapeBounds);
            }
        }
        return combinedBounds;
    }

    // 否则从场景获取当前选中的对象
    QRectF bounds;
    bool first = true;

    if (m_scene)
    {
        QList<QGraphicsItem *> items = m_scene->selectedItems();
        for (QGraphicsItem *item : items)
        {
            DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
            if (!shape)
                continue;
            QRectF sceneBounds = shape->sceneBoundingRect();
            if (first)
            {
                bounds = sceneBounds;
                first = false;
            }
            else
            {
                bounds = bounds.united(sceneBounds);
            }
        }
    }
    return bounds;
}

void OutlinePreviewTransformTool::cleanupInvalidShapes()
{
    // 清理m_selectedShapes中无效的图形引用
    QMutableListIterator<DrawingShape*> it(m_selectedShapes);
    while (it.hasNext()) {
        DrawingShape* shape = it.next();
        if (!shape || !shape->scene()) {
            // 图形已被删除或不再在任何场景中，移除引用
            it.remove();
            m_originalTransforms.remove(shape);
            qDebug() << "Removed invalid shape reference from selection";
        }
    }
}

void OutlinePreviewTransformTool::onSelectionChanged()
{
    // 清理无效的图形引用（已被删除的对象）
    cleanupInvalidShapes();
    qDebug() << "Selection changed";
    // 更新UI
    //  disableInternalSelectionIndicators();

    // 延迟更新手柄，确保选择状态完全更新
    QTimer::singleShot(0, this, [this]()
                       { updateHandlePositions(); });
}

void OutlinePreviewTransformTool::onObjectStateChanged(DrawingShape *shape)
{
    // 如果图形当前被选中，更新手柄
    if (shape && shape->isSelected())
    {
        updateHandlePositions();
    }
}

void OutlinePreviewTransformTool::updateDashOffset()
{
    if (!m_outlinePreview)
        return;

    // 动态改变虚线偏移，实现蚂蚁线移动效果
    QPen pen = m_outlinePreview->pen();
    qreal dashOffset = pen.dashOffset();
    pen.setDashOffset(dashOffset + 0.5);
    m_outlinePreview->setPen(pen);
}

void OutlinePreviewTransformTool::updateHandlePositions()
{

    if (!m_handleManager)
    {
        return;
    }

    QRectF bounds = calculateInitialSelectionBounds();

    // 如果有选中的图形，就更新手柄位置
    if (bounds.isEmpty())
    {
        m_handleManager->hideHandles();
    }
    else
    {
        // 手柄基于边界
        m_handleManager->updateHandles(bounds);

        // 如果有自定义旋转中心，更新中心手柄位置
        if (m_useCustomRotationCenter)
        {
            m_handleManager->setCenterHandlePosition(m_customRotationCenter);
        }

        // 根据当前状态决定显示或隐藏手柄
        if (m_state == STATE_GRABBED)
        {
            m_handleManager->hideHandles();
        }
        else
        {
            m_handleManager->showHandles();
        }
    }
}

void OutlinePreviewTransformTool::resetState()
{
    m_state = STATE_IDLE;
    m_activeHandle = TransformHandle::None;
    m_grabMousePos = QPointF();
    m_initialBounds = QRectF();
    m_oppositeHandle = QPointF();
    m_transformOrigin = QPointF();
    m_handleBounds = QRectF();

    // 不需要重置变换，因为我们直接操作每个图形
}

void OutlinePreviewTransformTool::createVisualHelpers()
{
    if (!m_scene)
        return;

    // 创建锚点（红色十字）- 显示在变换中心位置
    m_anchorPoint = new CustomHandleItem(TransformHandle::Center);
    m_anchorPoint->setStyle(HandleItemBase::Cross);
    m_anchorPoint->setSpecificColor(Qt::red);
    m_anchorPoint->setSize(12.0); // 增大尺寸
    m_anchorPoint->setZValue(2001);
    m_scene->addItem(m_anchorPoint);
    m_anchorPoint->setPos(m_transformOrigin);

    // 创建拖动点（绿色十字）
    m_dragPoint = new CustomHandleItem(TransformHandle::Center);
    m_dragPoint->setStyle(HandleItemBase::Cross);
    m_dragPoint->setSpecificColor(Qt::green);
    m_dragPoint->setSize(12.0); // 增大尺寸
    m_dragPoint->setZValue(2001);
    m_scene->addItem(m_dragPoint);

    // 创建旋转中心（浅蓝色）
    m_rotationCenter = new CustomHandleItem(TransformHandle::Center);
    m_rotationCenter->setStyle(HandleItemBase::Circle);
    m_rotationCenter->setSpecificColor(QColor(173, 216, 230, 160)); // 更淡的半透明浅蓝色
    m_rotationCenter->setSize(10.0);                                // 稍微小一点
    m_rotationCenter->setZValue(2002);
    m_scene->addItem(m_rotationCenter);

    // 创建整体轮廓预览
    m_outlinePreview = new QGraphicsPathItem();

    // 创建静态虚线画笔（选中状态）
    QPen outlinePen(Qt::black, 1);
    outlinePen.setCosmetic(true);      // 不受缩放影响
    outlinePen.setDashPattern({8, 4}); // 静态虚线模式，加长虚线段
    outlinePen.setDashOffset(0);       // 固定偏移，不动画
    m_outlinePreview->setPen(outlinePen);
    m_outlinePreview->setBrush(Qt::NoBrush);
    m_outlinePreview->setZValue(1999);
    m_scene->addItem(m_outlinePreview);

    // 创建动画定时器，用于实现蚂蚁线效果（初始不启动）
    m_dashTimer = new QTimer(this);
    connect(m_dashTimer, SIGNAL(timeout()), this, SLOT(updateDashOffset()));

    // 构建整体轮廓
    updateOutlinePreview();
}

void OutlinePreviewTransformTool::destroyVisualHelpers()
{
    if (m_anchorPoint)
    {
        m_scene->removeItem(m_anchorPoint);
        delete m_anchorPoint;
        m_anchorPoint = nullptr;
    }

    if (m_dragPoint)
    {
        m_scene->removeItem(m_dragPoint);
        delete m_dragPoint;
        m_dragPoint = nullptr;
    }

    if (m_rotationCenter)
    {
        m_scene->removeItem(m_rotationCenter);
        delete m_rotationCenter;
        m_rotationCenter = nullptr;
    }

    if (m_outlinePreview)
    {
        m_scene->removeItem(m_outlinePreview);
        delete m_outlinePreview;
        m_outlinePreview = nullptr;
    }

    if (m_dashTimer)
    {
        m_dashTimer->stop(); // 只停止定时器，不删除它
    }
}

void OutlinePreviewTransformTool::updateVisualHelpers(const QPointF &mousePos)
{
    if (!m_scene)
        return;

    // 更新拖动点位置
    if (m_dragPoint)
    {
        m_dragPoint->setPos(mousePos);
    }

    // 确保锚点可见（但旋转时隐藏，避免视觉混乱）
    if (m_anchorPoint)
    {
        // 旋转时隐藏锚点，让它与中心手柄重叠
        bool showAnchor = (m_activeHandle != TransformHandle::Rotate);
        m_anchorPoint->setVisible(showAnchor);
        if (showAnchor)
        {
            m_anchorPoint->setPos(m_scaleAnchor);
        }
    }

    // 更新旋转中心显示
    if (m_rotationCenter)
    {
        if (m_activeHandle == TransformHandle::Rotate || m_useCustomRotationCenter)
        {
            m_rotationCenter->setVisible(true);
            // 旋转时显示实际的旋转中心
            QPointF rotationCenter = m_useCustomRotationCenter ? m_customRotationCenter : m_transformOrigin;
            m_rotationCenter->setPos(rotationCenter);
        }
        else
        {
            m_rotationCenter->setVisible(false);
        }
    }

    // 更新整体轮廓
    updateOutlinePreview();
}

void OutlinePreviewTransformTool::updateOutlinePreview()
{
    if (!m_outlinePreview)
    {
        return;
    }

    // 使用统一的选择包围框，而不是合并路径
    QRectF unifiedBounds;

    // 如果在变换中，直接使用Group的边界
    if (m_state == STATE_GRABBED && !m_selectedShapes.isEmpty())
    {
        // 计算所有选中图形的统一边界框
        for (DrawingShape *shape : m_selectedShapes)
        {
            if (!shape || !shape->scene())
                continue;  // 跳过无效的图形
            QRectF shapeBounds = shape->sceneBoundingRect();

            if (unifiedBounds.isEmpty())
            {
                unifiedBounds = shapeBounds;
            }
            else
            {
                unifiedBounds = unifiedBounds.united(shapeBounds);
            }
        }
    }
    else if (m_scene)
    {
        // 否则从场景获取选中的图形
        QList<QGraphicsItem *> items = m_scene->selectedItems();
        for (QGraphicsItem *item : items)
        {
            DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
            if (!shape)
                continue;

            QRectF sceneBounds = shape->sceneBoundingRect();

            if (unifiedBounds.isEmpty())
            {
                unifiedBounds = sceneBounds;
            }
            else
            {
                unifiedBounds = unifiedBounds.united(sceneBounds);
            }
        }
    }

    // 创建统一边界框的路径
    QPainterPath boundsPath;
    if (!unifiedBounds.isEmpty())
    {
        boundsPath.addRect(unifiedBounds);
    }

    m_outlinePreview->setPath(boundsPath);
}

void OutlinePreviewTransformTool::disableInternalSelectionIndicators()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem *item : selectedItems)
    {
        DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
        if (shape)
        {
            shape->setShowSelectionIndicator(false);
        }
    }
}

void OutlinePreviewTransformTool::enableInternalSelectionIndicators()
{
    if (!m_scene)
        return;

    QList<QGraphicsItem *> selectedItems = m_scene->selectedItems();
    for (QGraphicsItem *item : selectedItems)
    {
        DrawingShape *shape = dynamic_cast<DrawingShape *>(item);
        if (shape)
        {
            shape->setShowSelectionIndicator(true);
        }
    }
}

void OutlinePreviewTransformTool::toggleMode()
{
    if (!m_handleManager)
        return;

    // 切换模式
    if (m_currentMode == HandleMode::Scale)
    {
        m_currentMode = HandleMode::RotateMode;
        emit statusMessageChanged("旋转模式 - 按空格键或Tab键切换到缩放模式");
    }
    else
    {
        m_currentMode = HandleMode::Scale;
        emit statusMessageChanged("缩放模式 - 按空格键或Tab键切换到旋转模式");
    }

    // 设置手柄管理器的新模式
    m_handleManager->setHandleMode(m_currentMode);

    // 重置自定义旋转中心，让它跟随选择边界
    resetRotationCenter();

    // 立即更新手柄显示
    updateHandlePositions();

    // 如果有视觉辅助元素，更新它们的位置
    if (m_rotationCenter)
    {
        updateVisualHelpers(QPointF());
    }
}

void OutlinePreviewTransformTool::setMode(HandleMode::Mode mode)
{
    if (!m_handleManager || m_currentMode == mode)
        return;

    m_currentMode = mode;
    m_handleManager->setHandleMode(m_currentMode);

    // 更新手柄显示
    updateHandlePositions();
}

HandleMode::Mode OutlinePreviewTransformTool::currentMode() const
{
    return m_currentMode;
}