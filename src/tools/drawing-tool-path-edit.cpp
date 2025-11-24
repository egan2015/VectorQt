#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QDebug>
#include <QPointer>
#include <QUndoCommand>
#include "../tools/drawing-tool-path-edit.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-shape.h"
#include "../core/patheditor.h"
#include "../core/layer-manager.h"
#include "../core/drawing-layer.h"

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

// 路径操作撤销命令
class PathOperationCommand : public QUndoCommand
{
public:
    PathOperationCommand(DrawingScene *scene, DrawingShape *originalShape, DrawingShape *secondShape,
                        DrawingPath *newPath, const QString &operationText, QUndoCommand *parent = nullptr)
        : QUndoCommand(operationText, parent)
        , m_scene(scene)
        , m_originalShape(originalShape)
        , m_secondShape(secondShape)
        , m_newPath(newPath)
        , m_originalShapeInScene(false)
        , m_secondShapeInScene(false)
        , m_newPathInScene(false)
        , m_originalLayer(nullptr)
        , m_secondLayer(nullptr)
        , m_targetLayer(nullptr)
    {
        // 记录对象初始状态
        if (m_originalShape) {
            m_originalShapeInScene = (m_originalShape->scene() != nullptr);
        }
        if (m_secondShape) {
            m_secondShapeInScene = (m_secondShape->scene() != nullptr);
        }
        if (m_newPath) {
            m_newPathInScene = (m_newPath->scene() != nullptr);
        }
        
        // 记录图形所属的图层
        LayerManager *layerManager = LayerManager::instance();
        if (layerManager) {
            m_originalLayer = layerManager->findLayerForShape(m_originalShape);
            m_secondLayer = layerManager->findLayerForShape(m_secondShape);
            // 新路径应该添加到活动图层
            m_targetLayer = layerManager->activeLayer();
        }
    }
    
    ~PathOperationCommand()
    {
        // 安全删除对象，只在确定不需要时删除
        if (m_newPath && !m_newPath->scene()) {
            delete m_newPath;
        }
        // 注意：不在这里删除原始形状，因为它们可能被其他命令引用
    }
    
    void undo() override
    {
        if (!m_scene) return;
        
        LayerManager *layerManager = LayerManager::instance();
        
        // 移除新创建的路径
        if (m_newPath && m_newPath->scene()) {
            m_scene->removeItem(m_newPath);
            m_newPath->setSelected(false);
            // 从图层中移除
            if (layerManager && m_targetLayer) {
                m_targetLayer->removeShape(m_newPath);
            }
        }
        
        // 恢复原始形状，只有在它们不在场景中时才添加
        if (m_originalShape && !m_originalShape->scene()) {
            m_scene->addItem(m_originalShape);
            m_originalShape->setSelected(true);
            // 添加到原始图层
            if (layerManager && m_originalLayer) {
                m_originalLayer->addShape(m_originalShape);
            }
        }
        if (m_secondShape && !m_secondShape->scene()) {
            m_scene->addItem(m_secondShape);
            m_secondShape->setSelected(true);
            // 添加到原始图层
            if (layerManager && m_secondLayer) {
                m_secondLayer->addShape(m_secondShape);
            }
        }
        
        m_scene->update();
        
        // 刷新对象树模型
        if (layerManager) {
            if (m_originalLayer) emit layerManager->layerContentChanged(m_originalLayer);
            if (m_secondLayer) emit layerManager->layerContentChanged(m_secondLayer);
            if (m_targetLayer) emit layerManager->layerContentChanged(m_targetLayer);
        }
    }
    
    void redo() override
    {
        if (!m_scene) return;
        
        LayerManager *layerManager = LayerManager::instance();
        
        // 移除原始形状，只有在它们在场景中时才移除
        if (m_originalShape && m_originalShape->scene()) {
            m_scene->removeItem(m_originalShape);
            m_originalShape->setSelected(false);
            // 从图层中移除
            if (layerManager && m_originalLayer) {
                m_originalLayer->removeShape(m_originalShape);
            }
        }
        if (m_secondShape && m_secondShape->scene()) {
            m_scene->removeItem(m_secondShape);
            m_secondShape->setSelected(false);
            // 从图层中移除
            if (layerManager && m_secondLayer) {
                m_secondLayer->removeShape(m_secondShape);
            }
        }
        
        // 添加新路径，只有在它不在场景中时才添加
        if (m_newPath && !m_newPath->scene()) {
            m_scene->addItem(m_newPath);
            m_newPath->setSelected(true);
            // 添加到目标图层
            if (layerManager && m_targetLayer) {
                m_targetLayer->addShape(m_newPath);
            }
        }
        
        m_scene->update();
        m_scene->setModified(true);
        
        // 刷新对象树模型
        if (layerManager) {
            emit layerManager->layerContentChanged(m_targetLayer);
        }
    }
    
private:
    DrawingScene *m_scene;
    DrawingShape *m_originalShape;
    DrawingShape *m_secondShape;
    DrawingPath *m_newPath;
    bool m_originalShapeInScene;
    bool m_secondShapeInScene;
    bool m_newPathInScene;
    DrawingLayer *m_originalLayer;
    DrawingLayer *m_secondLayer;
    DrawingLayer *m_targetLayer;
};

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
    
    // 创建撤销命令 - 在修改场景之前创建
    QString operationText;
    switch (m_booleanOp) {
        case PathEditor::Union: operationText = "路径合并"; break;
        case PathEditor::Intersection: operationText = "路径相交"; break;
        case PathEditor::Subtraction: operationText = "路径减去"; break;
        case PathEditor::Xor: operationText = "路径异或"; break;
        default: operationText = "路径布尔运算"; break;
    }
    PathOperationCommand *command = new PathOperationCommand(m_scene, shape1, shape2, newPath, operationText);
    
    // 推送撤销命令，让命令处理场景和图层的同步
    m_scene->undoStack()->push(command);
    
    // 清空剩余的选择列表
    m_selectedPaths.clear();
    
    // 将新路径添加到选择列表
    if (newPath) {
        m_selectedPaths.append(newPath);
        newPath->setSelected(true);
    }
    
    // qDebug() << "Path operation completed successfully";
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
    
    // 检查是否有选中的文本对象
    bool hasTextSelection = false;
    if (m_scene) {
        QList<QGraphicsItem *> selected = m_scene->selectedItems();
        for (QGraphicsItem *item : selected) {
            DrawingText *textShape = qgraphicsitem_cast<DrawingText*>(item);
            if (textShape) {
                hasTextSelection = true;
                break;
            }
        }
    }
    
    // 如果有选中的文本，添加文本转路径选项
    QAction *convertTextToPathAction = nullptr;
    if (hasTextSelection) {
        contextMenu.addSeparator();
        convertTextToPathAction = contextMenu.addAction("文本转路径");
    }
    
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
                
                // 创建撤销命令 - 在修改场景之前创建
                PathOperationCommand *command = new PathOperationCommand(m_scene, shape, nullptr, newPath, "简化路径");
                
                // 推送撤销命令，让命令处理场景和图层的同步
                m_scene->undoStack()->push(command);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
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
                
                // 创建撤销命令 - 在修改场景之前创建
                PathOperationCommand *command = new PathOperationCommand(m_scene, shape, nullptr, newPath, "平滑路径");
                
                // 推送撤销命令，让命令处理场景和图层的同步
                m_scene->undoStack()->push(command);
                
                // 更新选择列表
                m_selectedPaths.clear();
                m_selectedPaths.append(newPath);
                newPath->setSelected(true);
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
                
                // 创建撤销命令 - 在修改场景之前创建
                PathOperationCommand *command = new PathOperationCommand(m_scene, shape, nullptr, newPath, "转换为曲线");
                
                // 推送撤销命令，让命令处理场景和图层的同步
                m_scene->undoStack()->push(command);
                
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
                
                // 添加到场景和活动图层
                m_scene->addItem(newPath);
                LayerManager *layerManager = LayerManager::instance();
                if (layerManager) {
                    DrawingLayer *activeLayer = layerManager->activeLayer();
                    if (activeLayer) {
                        activeLayer->addShape(newPath);
                    }
                }
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
                
                // 添加到场景和活动图层
                m_scene->addItem(newPath);
                LayerManager *layerManager = LayerManager::instance();
                if (layerManager) {
                    DrawingLayer *activeLayer = layerManager->activeLayer();
                    if (activeLayer) {
                        activeLayer->addShape(newPath);
                    }
                }
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
            
            // 添加到场景和活动图层
            m_scene->addItem(newPath);
            LayerManager *layerManager = LayerManager::instance();
            if (layerManager) {
                DrawingLayer *activeLayer = layerManager->activeLayer();
                if (activeLayer) {
                    activeLayer->addShape(newPath);
                }
            }
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
            
            // 添加到场景和活动图层
            m_scene->addItem(newPath);
            LayerManager *layerManager = LayerManager::instance();
            if (layerManager) {
                DrawingLayer *activeLayer = layerManager->activeLayer();
                if (activeLayer) {
                    activeLayer->addShape(newPath);
                }
            }
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
            
            // 添加到场景和活动图层
            m_scene->addItem(newPath);
            LayerManager *layerManager = LayerManager::instance();
            if (layerManager) {
                DrawingLayer *activeLayer = layerManager->activeLayer();
                if (activeLayer) {
                    activeLayer->addShape(newPath);
                }
            }
            m_scene->setModified(true);
        }
    } else if (selectedAction == convertTextToPathAction) {
        // 处理文本转路径
        if (m_scene) {
            QList<QGraphicsItem *> selected = m_scene->selectedItems();
            QList<DrawingShape*> convertedShapes;
            
            for (QGraphicsItem *item : selected) {
                DrawingText *textShape = qgraphicsitem_cast<DrawingText*>(item);
                if (textShape) {
                    // 将文本转换为路径
                    DrawingPath *pathShape = textShape->convertToPath();
                    if (pathShape) {
                        // 从选择列表中移除原始文本
                        m_selectedPaths.removeAll(textShape);
                        
                        // 添加到场景
                        m_scene->addItem(pathShape);
                        pathShape->setSelected(true);
                        convertedShapes.append(pathShape);
                        
                        // 添加到选择列表
                        m_selectedPaths.append(pathShape);
                        
                        // 添加到活动图层
                        LayerManager *layerManager = LayerManager::instance();
                        if (layerManager) {
                            DrawingLayer *activeLayer = layerManager->activeLayer();
                            if (activeLayer) {
                                activeLayer->addShape(pathShape);
                            }
                        }
                        
                        // 安全地移除原始文本
                        textShape->setSelected(false);
                        m_scene->removeItem(textShape);
                        
                        // 从图层中移除原始文本
                        if (layerManager) {
                            DrawingLayer *textLayer = layerManager->findLayerForShape(textShape);
                            if (textLayer) {
                                textLayer->removeShape(textShape);
                            }
                        }
                        
                        delete textShape; // 现在可以安全删除，因为已经从选择列表中移除
                    }
                }
            }
            
            if (!convertedShapes.isEmpty()) {
                m_scene->setModified(true);
                showTemporaryMessage(QString("已将 %1 个文本转换为路径").arg(convertedShapes.size()), QCursor::pos());
            }
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

