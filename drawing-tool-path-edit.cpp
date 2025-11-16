#include "drawing-tool-path-edit.h"
#include "drawingscene.h"
#include "drawingview.h"
#include "drawing-shape.h"
#include "patheditor.h"
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QDebug>
#include <QPointer>

DrawingToolPathEdit::DrawingToolPathEdit(QObject *parent)
    : ToolBase(parent)
    , m_editMode(SelectMode)
    , m_booleanOp(PathEditor::Union)
    , m_pathEditor(nullptr)
    , m_isDragging(false)
{
}

DrawingToolPathEdit::~DrawingToolPathEdit()
{
    hideTemporaryMessage();
    if (m_pathEditor) {
        delete m_pathEditor;
        m_pathEditor = nullptr;
    }
}

void DrawingToolPathEdit::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    m_selectedPaths.clear();
    m_isDragging = false;
    
    // 设置视图为框选模式
    if (view) {
        view->setDragMode(QGraphicsView::RubberBandDrag);
    }
    
    // 连接对象状态变化信号
    if (scene) {
        connect(scene, &DrawingScene::objectStateChanged, this, &DrawingToolPathEdit::onObjectStateChanged, Qt::UniqueConnection);
    }
}

void DrawingToolPathEdit::deactivate()
{
    // 清理所有选择的形状 - 只设置选择状态，不删除对象
    // 因为这些对象可能已经被其他操作删除了
    for (DrawingShape *shape : m_selectedPaths) {
        if (shape) {
            // 检查对象是否仍然有效，通过检查它是否还在某个场景中
            if (shape->scene()) {
                shape->setSelected(false);
            }
        }
    }
    m_selectedPaths.clear();
    
    // 恢复视图的拖动模式
    if (m_view) {
        m_view->setDragMode(QGraphicsView::NoDrag);
    }
    
    // 断开对象状态变化信号
    if (m_scene) {
        disconnect(m_scene, &DrawingScene::objectStateChanged, this, &DrawingToolPathEdit::onObjectStateChanged);
    }
    
    ToolBase::deactivate();
}

bool DrawingToolPathEdit::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        // 让场景处理选择和框选，不进行拖动
        return false;
    } else if (event->button() == Qt::RightButton && m_scene) {
        // 右键显示上下文菜单
        // qDebug() << "Right click, showing context menu";
        
        // 更新选择列表为当前场景选中的对象
        updateSelectedPathsFromScene();
        
        showContextMenu(scenePos);
        return true;
    }
    
    return false;
}

bool DrawingToolPathEdit::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    // 不处理拖动，让场景处理框选
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

bool DrawingToolPathEdit::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    // 不处理拖动，让场景处理
    Q_UNUSED(event)
    Q_UNUSED(scenePos)
    return false;
}

void DrawingToolPathEdit::executePathOperation()
{
    // qDebug() << "executePathOperation called, selectedPaths count:" << m_selectedPaths.size();
    
    if (m_selectedPaths.size() < 2) {
        // 显示用户友好的提示
        if (m_scene && m_selectedPaths.size() == 1) {
            // 在场景中显示一个临时提示信息
            showTemporaryMessage("按住Ctrl或Shift键点击可多选路径\n需要至少选择两个路径进行布尔运算", 
                                m_selectedPaths.first()->mapToScene(m_selectedPaths.first()->boundingRect().center()));
        }
        return;
    }
    
    // 获取前两个选中的形状
    DrawingShape *shape1 = m_selectedPaths[0];
    DrawingShape *shape2 = m_selectedPaths[1];
    
    // 检查形状是否有效
    if (!shape1 || !shape2) {
        qDebug() << "无效的形状对象";
        return;
    }
    
    // qDebug() << "Shape1 valid:" << (shape1 != nullptr) << "Shape2 valid:" << (shape2 != nullptr);
    
    // 确保形状在场景中
    if (!m_scene->items().contains(shape1) || !m_scene->items().contains(shape2)) {
        qDebug() << "形状不在场景中";
        return;
    }
    
    // 执行布尔运算
    QPainterPath result;
    try {
        // 获取形状的实际位置
        QPointF pos1 = shape1->pos();
        QPointF pos2 = shape2->pos();
        
        // qDebug() << "=== 布尔运算调试信息 ===";
        // qDebug() << "Shape1 pos:" << pos1 << "Shape2 pos:" << pos2;
        // qDebug() << "Shape1 boundingRect:" << shape1->boundingRect();
        // qDebug() << "Shape2 boundingRect:" << shape2->boundingRect();
        
        // 获取基础路径（不包含位置）
        QPainterPath path1Base = shape1->transformedShape();
        QPainterPath path2Base = shape2->transformedShape();
        
        // 创建包含位置信息的路径
        QPainterPath path1WithPos;
        QPainterPath path2WithPos;
        
        // 将路径平移到正确的位置
        QTransform transform1;
        transform1.translate(pos1.x(), pos1.y());
        path1WithPos = transform1.map(path1Base);
        
        QTransform transform2;
        transform2.translate(pos2.x(), pos2.y());
        path2WithPos = transform2.map(path2Base);
        
        // qDebug() << "Path1 transformed boundingRect:" << path1WithPos.boundingRect();
        // qDebug() << "Path2 transformed boundingRect:" << path2WithPos.boundingRect();
        // qDebug() << "Path1 elementCount:" << path1WithPos.elementCount();
        // qDebug() << "Path2 elementCount:" << path2WithPos.elementCount();
        
        // 检查路径是否真的相交
        QRectF bounds1 = path1WithPos.boundingRect();
        QRectF bounds2 = path2WithPos.boundingRect();
        // qDebug() << "Bounds intersect:" << bounds1.intersects(bounds2);
        // qDebug() << "Intersection rect:" << bounds1.intersected(bounds2);
        
        result = PathEditor::booleanOperation(
            path1WithPos, 
            path2WithPos, 
            m_booleanOp
        );
        // qDebug() << "Boolean operation completed";
        // qDebug() << "Result boundingRect:" << result.boundingRect();
        // qDebug() << "Result elementCount:" << result.elementCount();
    } catch (...) {
        qDebug() << "布尔运算异常";
        return;
    }
    
    // 检查结果是否为空
    if (result.isEmpty()) {
        qDebug() << "布尔运算结果为空";
        return;
    }
    
    // qDebug() << "Creating new path";
    
    // 创建新的路径对象
    DrawingPath *newPath = nullptr;
    try {
        newPath = new DrawingPath();
        
        // 获取原始形状的边界框
        QRectF bounds1 = shape1->boundingRect();
        QRectF bounds2 = shape2->boundingRect();
        
        // qDebug() << "Original bounds1:" << bounds1;
        // qDebug() << "Original bounds2:" << bounds2;
        // qDebug() << "Result elementCount:" << result.elementCount();
        
        // 计算结果路径的边界框
        QRectF resultBounds = result.boundingRect();
        // qDebug() << "Result bounds:" << resultBounds;
        
        // 布尔运算的结果已经包含了正确的位置信息
        // 我们需要将结果路径转换为相对于图形原点的路径
        // 计算结果路径的实际位置（使用第一个路径的位置作为参考）
        QPointF resultPos = shape1->pos();
        
        // 创建一个变换，将路径移动到相对于图形原点的位置
        QTransform offsetTransform;
        offsetTransform.translate(-resultBounds.left(), -resultBounds.top());
        QPainterPath adjustedPath = offsetTransform.map(result);
        
        // 设置调整后的路径
        newPath->setPath(adjustedPath);
        
        // 设置新路径的位置为第一个形状的位置
        newPath->setPos(shape1->pos());
        
        // 复制样式
        newPath->setStrokePen(shape1->strokePen());
        newPath->setFillBrush(shape1->fillBrush());
        
        // qDebug() << "New path created successfully";
        // qDebug() << "New path bounds after setting:" << newPath->boundingRect();
        // qDebug() << "New path position:" << newPath->pos();
    } catch (...) {
        qDebug() << "创建新路径失败";
        if (newPath) {
            delete newPath;
        }
        return;
    }
    
    // 先从选择列表中移除要删除的形状，避免悬空指针
    m_selectedPaths.removeAll(shape1);
    m_selectedPaths.removeAll(shape2);
    
    // 清除剩余形状的选择状态
    for (DrawingShape *shape : m_selectedPaths) {
        if (shape) {
            shape->setSelected(false);
        }
    }
    
    // 先从场景中移除原始形状
    // qDebug() << "Removing original shapes";
    if (shape1 && m_scene->items().contains(shape1)) {
        m_scene->removeItem(shape1);
    }
    if (shape2 && m_scene->items().contains(shape2)) {
        m_scene->removeItem(shape2);
    }
    
    // 添加新路径到场景
    // qDebug() << "Adding new path to scene";
    if (newPath && m_scene) {
        m_scene->addItem(newPath);
    }
    
    // 安全删除原始形状 - 先从场景中移除再删除
    // qDebug() << "Deleting original shapes";
    if (shape1) {
        shape1->setSelected(false);
        if (m_scene && m_scene->items().contains(shape1)) {
            m_scene->removeItem(shape1);
        }
        delete shape1;
        shape1 = nullptr;
    }
    if (shape2) {
        shape2->setSelected(false);
        if (m_scene && m_scene->items().contains(shape2)) {
            m_scene->removeItem(shape2);
        }
        delete shape2;
        shape2 = nullptr;
    }
    
    // 清空剩余的选择列表
    m_selectedPaths.clear();
    
    // 将新路径添加到选择列表
    if (newPath) {
        m_selectedPaths.append(newPath);
        newPath->setSelected(true);
    }
    
    // qDebug() << "Path operation completed successfully";
    m_scene->setModified(true);
}

void DrawingToolPathEdit::showContextMenu(const QPointF &scenePos)
{
    // 确保场景存在
    if (!m_scene) {
        return;
    }
    
    QMenu contextMenu;
    
    // 布尔运算菜单
    QMenu *booleanMenu = contextMenu.addMenu("布尔运算");
    
    QAction *unionAction = booleanMenu->addAction("合并");
    QAction *intersectAction = booleanMenu->addAction("相交");
    QAction *subtractAction = booleanMenu->addAction("减去");
    QAction *xorAction = booleanMenu->addAction("异或");
    
    // 路径操作菜单
    QMenu *pathMenu = contextMenu.addMenu("路径操作");
    
    QAction *simplifyAction = pathMenu->addAction("简化路径");
    QAction *smoothAction = pathMenu->addAction("平滑路径");
    QAction *curveAction = pathMenu->addAction("转换为曲线");
    QAction *offsetAction = pathMenu->addAction("偏移路径");
    QAction *clipAction = pathMenu->addAction("裁剪路径");
    
    // 创建形状菜单
    QMenu *shapeMenu = contextMenu.addMenu("创建形状");
    
    QAction *arrowAction = shapeMenu->addAction("箭头");
    QAction *starAction = shapeMenu->addAction("星形");
    QAction *gearAction = shapeMenu->addAction("齿轮");
    
    // 显示菜单
    QAction *selectedAction = contextMenu.exec(QCursor::pos());
    
    // 处理选择
    if (selectedAction == unionAction) {
        m_booleanOp = PathEditor::Union;
        executePathOperation();
    } else if (selectedAction == intersectAction) {
        m_booleanOp = PathEditor::Intersection;
        executePathOperation();
    } else if (selectedAction == subtractAction) {
        m_booleanOp = PathEditor::Subtraction;
        executePathOperation();
    } else if (selectedAction == xorAction) {
        m_booleanOp = PathEditor::Xor;
        executePathOperation();
    } else if (selectedAction == simplifyAction) {
        if (!m_selectedPaths.isEmpty() && m_scene) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape && shape->scene()) {
                // 使用变换后的路径，确保考虑了所有变换
                QPainterPath transformedPath = shape->transformedShape();
                // 简化路径
                QPainterPath simplified = PathEditor::simplifyPath(transformedPath, 0.5);
                
                // 创建一个新的DrawingPath来存储简化后的路径
                DrawingPath *newPath = new DrawingPath();
                // 重置路径的位置信息
                QRectF bounds = simplified.boundingRect();
                QTransform offsetTransform;
                offsetTransform.translate(-bounds.left(), -bounds.top());
                QPainterPath adjustedPath = offsetTransform.map(simplified);
                newPath->setPath(adjustedPath);
                newPath->setPos(shape->pos() + bounds.topLeft());
                newPath->setStrokePen(shape->strokePen());
                newPath->setFillBrush(shape->fillBrush());
                
                // 安全地从场景中移除原始形状
                shape->setSelected(false);
                m_scene->removeItem(shape);
                
                // 添加新路径到场景
                m_scene->addItem(newPath);
                m_scene->setModified(true);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
                
                // 安全删除原始形状
                delete shape;
            }
        }
    } else if (selectedAction == smoothAction) {
        if (!m_selectedPaths.isEmpty() && m_scene) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape && shape->scene()) {
                // 使用变换后的路径
                QPainterPath transformedPath = shape->transformedShape();
                QPainterPath smoothed = PathEditor::smoothPath(transformedPath, 0.5);
                
                // 创建一个新的DrawingPath来存储平滑后的路径
                DrawingPath *newPath = new DrawingPath();
                // 重置路径的位置信息
                QRectF bounds = smoothed.boundingRect();
                QTransform offsetTransform;
                offsetTransform.translate(-bounds.left(), -bounds.top());
                QPainterPath adjustedPath = offsetTransform.map(smoothed);
                newPath->setPath(adjustedPath);
                newPath->setPos(shape->pos() + bounds.topLeft());
                newPath->setStrokePen(shape->strokePen());
                newPath->setFillBrush(shape->fillBrush());
                
                // 安全地从场景中移除原始形状
                shape->setSelected(false);
                m_scene->removeItem(shape);
                
                // 添加新路径到场景
                m_scene->addItem(newPath);
                m_scene->setModified(true);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
                
                // 安全删除原始形状
                delete shape;
            }
        }
    } else if (selectedAction == curveAction) {
        if (!m_selectedPaths.isEmpty() && m_scene) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape && shape->scene()) {
                // 使用变换后的路径
                QPainterPath transformedPath = shape->transformedShape();
                QPainterPath curved = PathEditor::convertToCurve(transformedPath);
                
                // 创建一个新的DrawingPath来存储曲线后的路径
                DrawingPath *newPath = new DrawingPath();
                // 重置路径的位置信息
                QRectF bounds = curved.boundingRect();
                QTransform offsetTransform;
                offsetTransform.translate(-bounds.left(), -bounds.top());
                QPainterPath adjustedPath = offsetTransform.map(curved);
                newPath->setPath(adjustedPath);
                newPath->setPos(shape->pos() + bounds.topLeft());
                newPath->setStrokePen(shape->strokePen());
                newPath->setFillBrush(shape->fillBrush());
                
                // 安全地从场景中移除原始形状
                shape->setSelected(false);
                m_scene->removeItem(shape);
                
                // 添加新路径到场景
                m_scene->addItem(newPath);
                m_scene->setModified(true);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
                
                // 安全删除原始形状
                delete shape;
            }
        }
    } else if (selectedAction == offsetAction) {
        if (!m_selectedPaths.isEmpty()) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape) {
                // 使用变换后的路径
                QPainterPath transformedPath = shape->transformedShape();
                QPainterPath offset = PathEditor::offsetPath(transformedPath, 5);
                DrawingPath *newPath = new DrawingPath();
                // 重置路径的位置信息
                QRectF bounds = offset.boundingRect();
                QTransform offsetTransform;
                offsetTransform.translate(-bounds.left(), -bounds.top());
                QPainterPath adjustedPath = offsetTransform.map(offset);
                newPath->setPath(adjustedPath);
                newPath->setPos(shape->pos() + bounds.topLeft());
                newPath->setStrokePen(shape->strokePen());
                newPath->setFillBrush(shape->fillBrush());
                m_scene->addItem(newPath);
                m_scene->setModified(true);
            }
        }
    } else if (selectedAction == clipAction) {
        if (!m_selectedPaths.isEmpty()) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape) {
                // 使用变换后的路径
                QPainterPath transformedPath = shape->transformedShape();
                // 使用图形的边界框作为裁剪区域
                QRectF bounds = transformedPath.boundingRect();
                QRectF clipRect = bounds.adjusted(10, 10, -10, -10); // 稍微缩小边界框
                QPainterPath clipped = PathEditor::clipPath(transformedPath, clipRect);
                
                DrawingPath *newPath = new DrawingPath();
                // 重置路径的位置信息
                QRectF clippedBounds = clipped.boundingRect();
                QTransform offsetTransform;
                offsetTransform.translate(-clippedBounds.left(), -clippedBounds.top());
                QPainterPath adjustedPath = offsetTransform.map(clipped);
                newPath->setPath(adjustedPath);
                newPath->setPos(shape->pos() + clippedBounds.topLeft());
                newPath->setStrokePen(shape->strokePen());
                newPath->setFillBrush(shape->fillBrush());
                m_scene->addItem(newPath);
                m_scene->setModified(true);
            }
        }
    } else if (selectedAction == arrowAction) {
        if (m_scene) {
            QPainterPath arrow = PathEditor::createArrow(
                QPointF(scenePos.x() - 50, scenePos.y()),
                QPointF(scenePos.x() + 50, scenePos.y())
            );
            DrawingPath *newPath = new DrawingPath();
            newPath->setPath(arrow);
            newPath->setPos(0, 0);
            newPath->setStrokePen(QPen(Qt::black, 2));
            newPath->setFillBrush(Qt::NoBrush);
            m_scene->addItem(newPath);
            m_scene->setModified(true);
        }
    } else if (selectedAction == starAction) {
        if (m_scene) {
            QPainterPath star = PathEditor::createStar(scenePos, 50, 5);
            DrawingPath *newPath = new DrawingPath();
            newPath->setPath(star);
            newPath->setPos(0, 0);
            newPath->setStrokePen(QPen(Qt::black, 2));
            newPath->setFillBrush(QBrush(Qt::yellow));
            m_scene->addItem(newPath);
            m_scene->setModified(true);
        }
    } else if (selectedAction == gearAction) {
        if (m_scene) {
            QPainterPath gear = PathEditor::createGear(scenePos, 50, 8);
            DrawingPath *newPath = new DrawingPath();
            newPath->setPath(gear);
            newPath->setPos(0, 0);
            newPath->setStrokePen(QPen(Qt::black, 2));
            newPath->setFillBrush(QBrush(Qt::gray));
            m_scene->addItem(newPath);
            m_scene->setModified(true);
        }
    }
}

void DrawingToolPathEdit::showTemporaryMessage(const QString &message, const QPointF &pos)
{
    hideTemporaryMessage(); // 先隐藏之前的消息
    
    if (!m_scene) return;
    
    // 创建一个临时的文本项
    QGraphicsTextItem *textItem = new QGraphicsTextItem(message);
    textItem->setDefaultTextColor(QColor(255, 0, 0)); // 红色文字
    textItem->setFont(QFont("Arial", 12));
    textItem->setPos(pos.x() + 20, pos.y() - 30); // 稍微偏移位置
    
    m_scene->addItem(textItem);
    
    // 使用QTimer在3秒后自动删除
    // 使用QPointer确保对象仍然存在
    QPointer<QGraphicsTextItem> textItemRef(textItem);
    QTimer::singleShot(3000, [textItemRef]() {
        if (textItemRef && !textItemRef.isNull()) {
            if (textItemRef->scene()) {
                textItemRef->scene()->removeItem(textItemRef);
            }
            delete textItemRef;
        }
    });
}

void DrawingToolPathEdit::hideTemporaryMessage()
{
    // 这个方法可以用于立即隐藏当前显示的消息
    // 由于我们使用了QTimer自动删除，这里暂时不需要额外处理
}

void DrawingToolPathEdit::updateSelectedPathsFromScene()
{
    // 清空当前选择列表
    m_selectedPaths.clear();
    
    // 从场景获取选中的项目
    if (m_scene) {
        QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
        for (QGraphicsItem *item : selectedItems) {
            // 尝试转换为DrawingShape
            if (auto shape = qgraphicsitem_cast<DrawingShape*>(item)) {
                m_selectedPaths.append(shape);
            }
        }
    }
}

void DrawingToolPathEdit::onObjectStateChanged(DrawingShape* shape)
{
    // 如果状态变化的路径是当前选中的路径之一，更新路径编辑器
    if (m_selectedPaths.contains(shape)) {
        // 路径编辑器会自动更新显示
        // 这里不需要显式调用update，因为场景会自动重绘
        if (m_scene) {
            m_scene->update();
        }
    }
}

