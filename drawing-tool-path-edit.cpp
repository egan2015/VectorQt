#include "drawing-tool-path-edit.h"
#include "drawingscene.h"
#include "drawingview.h"
#include "drawing-shape.h"
#include "patheditor.h"
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QDebug>

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
    // 清理选择的形状列表 - 只设置选择状态，不删除对象
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
    
    // 清理路径编辑器
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
    
    ToolBase::deactivate();
}

bool DrawingToolPathEdit::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        if (m_editMode == SelectMode) {
            // 选择模式：选择路径
            QList<QGraphicsItem*> items = m_scene->items(scenePos);
            DrawingShape *clickedShape = nullptr;
            
            for (QGraphicsItem *item : items) {
                // 尝试转换为DrawingShape（包括DrawingPath和DrawingPolygon）
                if (auto shape = qgraphicsitem_cast<DrawingShape*>(item)) {
                    clickedShape = shape;
                    break;
                }
            }
            
            if (clickedShape) {
                qDebug() << "Clicked on shape, current selection count:" << m_selectedPaths.size();
                
                if (event->modifiers() & Qt::ControlModifier) {
                    // Ctrl+点击：添加到选择
                    if (!m_selectedPaths.contains(clickedShape)) {
                        m_selectedPaths.append(clickedShape);
                        clickedShape->setSelected(true);
                        qDebug() << "Shape added to selection, new count:" << m_selectedPaths.size();
                    }
                } else {
                    // 普通点击：替换选择
                    for (DrawingShape *shape : m_selectedPaths) {
                        if (shape) {
                            shape->setSelected(false);
                        }
                    }
                    m_selectedPaths.clear();
                    m_selectedPaths.append(clickedShape);
                    clickedShape->setSelected(true);
                    qDebug() << "Selection replaced, new count:" << m_selectedPaths.size();
                }
                
                m_isDragging = true;
                m_lastPos = scenePos;
            } else {
                // 点击空白处：清除选择
                qDebug() << "Clicked on empty space, clearing selection";
                for (DrawingShape *shape : m_selectedPaths) {
                    if (shape) {
                        shape->setSelected(false);
                    }
                }
                m_selectedPaths.clear();
            }
        }
        
        return true;
    } else if (event->button() == Qt::RightButton && m_scene) {
        // 右键显示上下文菜单
        qDebug() << "Right click, showing context menu";
        showContextMenu(scenePos);
        return true;
    }
    
    return false;
}

bool DrawingToolPathEdit::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_isDragging && !m_selectedPaths.isEmpty() && m_scene) {
        // 移动选中的路径
        QPointF delta = scenePos - m_lastPos;
        
        for (DrawingShape *shape : m_selectedPaths) {
            shape->setPos(shape->pos() + delta);
        }
        
        m_lastPos = scenePos;
        m_scene->setModified(true);
        return true;
    }
    
    return false;
}

bool DrawingToolPathEdit::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        return true;
    }
    
    return false;
}

void DrawingToolPathEdit::executePathOperation()
{
    qDebug() << "executePathOperation called, selectedPaths count:" << m_selectedPaths.size();
    
    if (m_selectedPaths.size() < 2) {
        qDebug() << "需要至少选择两个路径进行布尔运算";
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
    
    qDebug() << "Shape1 valid:" << (shape1 != nullptr) << "Shape2 valid:" << (shape2 != nullptr);
    
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
        
        qDebug() << "=== 布尔运算调试信息 ===";
        qDebug() << "Shape1 pos:" << pos1 << "Shape2 pos:" << pos2;
        qDebug() << "Shape1 boundingRect:" << shape1->boundingRect();
        qDebug() << "Shape2 boundingRect:" << shape2->boundingRect();
        
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
        
        qDebug() << "Path1 transformed boundingRect:" << path1WithPos.boundingRect();
        qDebug() << "Path2 transformed boundingRect:" << path2WithPos.boundingRect();
        qDebug() << "Path1 elementCount:" << path1WithPos.elementCount();
        qDebug() << "Path2 elementCount:" << path2WithPos.elementCount();
        
        // 检查路径是否真的相交
        QRectF bounds1 = path1WithPos.boundingRect();
        QRectF bounds2 = path2WithPos.boundingRect();
        qDebug() << "Bounds intersect:" << bounds1.intersects(bounds2);
        qDebug() << "Intersection rect:" << bounds1.intersected(bounds2);
        
        result = PathEditor::booleanOperation(
            path1WithPos, 
            path2WithPos, 
            m_booleanOp
        );
        qDebug() << "Boolean operation completed";
        qDebug() << "Result boundingRect:" << result.boundingRect();
        qDebug() << "Result elementCount:" << result.elementCount();
    } catch (...) {
        qDebug() << "布尔运算异常";
        return;
    }
    
    // 检查结果是否为空
    if (result.isEmpty()) {
        qDebug() << "布尔运算结果为空";
        return;
    }
    
    qDebug() << "Creating new path";
    
    // 创建新的路径对象
    DrawingPath *newPath = nullptr;
    try {
        newPath = new DrawingPath();
        
        // 获取原始形状的边界框
        QRectF bounds1 = shape1->boundingRect();
        QRectF bounds2 = shape2->boundingRect();
        
        qDebug() << "Original bounds1:" << bounds1;
        qDebug() << "Original bounds2:" << bounds2;
        qDebug() << "Result elementCount:" << result.elementCount();
        
        // 计算结果路径的边界框
        QRectF resultBounds = result.boundingRect();
        qDebug() << "Result bounds:" << resultBounds;
        
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
        
        qDebug() << "New path created successfully";
        qDebug() << "New path bounds after setting:" << newPath->boundingRect();
        qDebug() << "New path position:" << newPath->pos();
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
    qDebug() << "Removing original shapes";
    if (shape1 && m_scene->items().contains(shape1)) {
        m_scene->removeItem(shape1);
    }
    if (shape2 && m_scene->items().contains(shape2)) {
        m_scene->removeItem(shape2);
    }
    
    // 添加新路径到场景
    qDebug() << "Adding new path to scene";
    if (newPath && m_scene) {
        m_scene->addItem(newPath);
    }
    
    // 安全删除原始形状 - 先从场景中移除再删除
    qDebug() << "Deleting original shapes";
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
    
    qDebug() << "Path operation completed successfully";
    m_scene->setModified(true);
}

void DrawingToolPathEdit::showContextMenu(const QPointF &scenePos)
{
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
        if (!m_selectedPaths.isEmpty()) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape) {
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
                
                // 从场景中移除原始形状
                m_scene->removeItem(shape);
                // 添加新路径到场景
                m_scene->addItem(newPath);
                m_scene->setModified(true);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
            }
        }
    } else if (selectedAction == smoothAction) {
        if (!m_selectedPaths.isEmpty()) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape) {
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
                
                // 从场景中移除原始形状
                m_scene->removeItem(shape);
                // 添加新路径到场景
                m_scene->addItem(newPath);
                m_scene->setModified(true);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
            }
        }
    } else if (selectedAction == curveAction) {
        if (!m_selectedPaths.isEmpty()) {
            DrawingShape *shape = m_selectedPaths.first();
            if (shape) {
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
                
                // 从场景中移除原始形状
                m_scene->removeItem(shape);
                // 添加新路径到场景
                m_scene->addItem(newPath);
                m_scene->setModified(true);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
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
    } else if (selectedAction == starAction) {
        QPainterPath star = PathEditor::createStar(scenePos, 50, 5);
        DrawingPath *newPath = new DrawingPath();
        newPath->setPath(star);
        newPath->setPos(0, 0);
        newPath->setStrokePen(QPen(Qt::black, 2));
        newPath->setFillBrush(QBrush(Qt::yellow));
        m_scene->addItem(newPath);
        m_scene->setModified(true);
    } else if (selectedAction == gearAction) {
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

#include "drawing-tool-path-edit.moc"