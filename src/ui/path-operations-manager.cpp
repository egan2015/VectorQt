#include "path-operations-manager.h"
#include "mainwindow.h"
#include "drawingscene.h"
#include "../core/drawing-shape.h"
#include "../core/patheditor.h"
#include "command-manager.h"
#include <QGraphicsTextItem>
#include <QMenu>

PathOperationsManager::PathOperationsManager(MainWindow *parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_scene(nullptr)
{
}

PathOperationsManager::~PathOperationsManager()
{
}

void PathOperationsManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
}

DrawingScene *PathOperationsManager::scene() const
{
    return m_scene;
}

void PathOperationsManager::pathUnion()
{
    performBooleanOperation(Union, "路径联合");
}

void PathOperationsManager::pathSubtract()
{
    performBooleanOperation(Subtract, "路径减去");
}

void PathOperationsManager::pathIntersect()
{
    performBooleanOperation(Intersect, "路径相交");
}

void PathOperationsManager::pathXor()
{
    performBooleanOperation(Xor, "路径异或");
}

void PathOperationsManager::pathSimplify()
{
    performPathOperation(Simplify, "路径简化");
}

void PathOperationsManager::pathSmooth()
{
    performPathOperation(Smooth, "路径平滑");
}

void PathOperationsManager::pathReverse()
{
    performPathOperation(Reverse, "路径反转");
}

void PathOperationsManager::convertTextToPath()
{
    // TODO: 实现文本转路径功能
    emit statusMessageChanged("文本转路径功能尚未实现");
}

void PathOperationsManager::convertSelectedTextToPath()
{
    if (!m_scene) return;
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    bool hasTextItems = false;
    
    for (QGraphicsItem *item : selected) {
        if (QGraphicsTextItem *textItem = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
            hasTextItems = true;
            break;
        }
    }
    
    if (!hasTextItems) {
        emit statusMessageChanged("请先选择文本对象");
        return;
    }
    
    // TODO: 实现选中文本转路径功能
    emit statusMessageChanged("选中文本转路径功能尚未实现");
}

void PathOperationsManager::performBooleanOperation(BooleanOperation op, const QString &opName)
{
    if (!validateSelectionForBoolean()) {
        return;
    }
    
    // 创建撤销命令
    class BooleanOperationCommand : public QUndoCommand {
    public:
        BooleanOperationCommand(DrawingScene *scene, PathOperationsManager::BooleanOperation op, 
                               const QString &opName, QUndoCommand *parent = nullptr)
            : QUndoCommand(opName, parent), m_scene(scene), m_operation(op)
        {
            // 保存原始状态
            if (m_scene) {
                m_selectedItems = m_scene->selectedItems();
            }
        }

        void undo() override {
            // TODO: 实现撤销逻辑
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            // TODO: 实现布尔运算逻辑
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        PathOperationsManager::BooleanOperation m_operation;
        QList<QGraphicsItem*> m_selectedItems;
    };

    if (m_scene->commandManager()) {
        m_scene->commandManager()->pushCommand(new BooleanOperationCommand(m_scene, op, opName));
    }
    emit pathOperationCompleted(opName);
    emit statusMessageChanged(QString("已执行 %1 操作").arg(opName));
}

void PathOperationsManager::performPathOperation(PathOperation op, const QString &opName)
{
    if (!validateSelectionForPathOperation()) {
        return;
    }
    
    // 创建撤销命令
    class PathOperationCommand : public QUndoCommand {
    public:
        PathOperationCommand(DrawingScene *scene, PathOperationsManager::PathOperation op, 
                           const QString &opName, QUndoCommand *parent = nullptr)
            : QUndoCommand(opName, parent), m_scene(scene), m_operation(op)
        {
            // 保存原始状态
            if (m_scene) {
                m_selectedItems = m_scene->selectedItems();
            }
        }

        void undo() override {
            // TODO: 实现撤销逻辑
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            // TODO: 实现路径操作逻辑
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        PathOperationsManager::PathOperation m_operation;
        QList<QGraphicsItem*> m_selectedItems;
    };

    if (m_scene->commandManager()) {
        m_scene->commandManager()->pushCommand(new PathOperationCommand(m_scene, op, opName));
    }
    emit pathOperationCompleted(opName);
    emit statusMessageChanged(QString("已执行 %1 操作").arg(opName));
}

bool PathOperationsManager::validateSelectionForBoolean()
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return false;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行布尔运算");
        return false;
    }
    
    // 检查是否都是DrawingShape对象
    for (QGraphicsItem *item : selected) {
        if (!qgraphicsitem_cast<DrawingShape*>(item)) {
            emit statusMessageChanged("布尔运算只能应用于图形对象");
            return false;
        }
    }
    
    return true;
}

bool PathOperationsManager::validateSelectionForPathOperation()
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return false;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        emit statusMessageChanged("请先选择要操作的对象");
        return false;
    }
    
    // 检查是否都是DrawingShape对象
    for (QGraphicsItem *item : selected) {
        if (!qgraphicsitem_cast<DrawingShape*>(item)) {
            emit statusMessageChanged("路径操作只能应用于图形对象");
            return false;
        }
    }
    
    return true;
}

// 从MainWindow移动过来的路径操作方法
void PathOperationsManager::performPathBooleanOperation(int op, const QString &opName)
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 2) {
        emit statusMessageChanged(QString("%1操作需要至少选中2个图形").arg(opName));
        return;
    }
    
    // 收集所有选中图形的路径
    QList<QPainterPath> paths;
    QList<DrawingShape*> shapes;
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape) {
            // 获取图形的路径
            QPainterPath shapePath;
            
            // 根据图形类型获取路径
            if (shape->shapeType() == DrawingShape::Path) {
                DrawingPath *drawingPath = dynamic_cast<DrawingPath*>(shape);
                if (drawingPath) {
                    shapePath = drawingPath->path();
                }
            } else {
                // 对于其他图形，创建对应的路径
                QRectF bounds = shape->boundingRect();
                if (shape->shapeType() == DrawingShape::Rectangle) {
                    shapePath.addRect(bounds);
                } else if (shape->shapeType() == DrawingShape::Ellipse) {
                    shapePath.addEllipse(bounds);
                }
                // 可以添加更多图形类型的支持
            }
            
            if (!shapePath.isEmpty()) {
                paths.append(shapePath);
                shapes.append(shape);
            }
        }
    }
    
    if (paths.size() < 2) {
        emit statusMessageChanged(QString("没有找到可进行%1操作的图形").arg(opName));
        return;
    }
    
    // 执行布尔运算
    QPainterPath resultPath = paths[0];
    for (int i = 1; i < paths.size(); ++i) {
        resultPath = PathEditor::booleanOperation(resultPath, paths[i], static_cast<PathEditor::BooleanOperation>(op));
    }
    
    if (resultPath.isEmpty()) {
        emit statusMessageChanged(QString("%1操作结果为空").arg(opName));
        return;
    }
    
    // 创建新的路径图形
    DrawingPath *newPath = new DrawingPath();
    newPath->setPath(resultPath);
    
    // 设置新图形的位置和样式
    if (!shapes.isEmpty()) {
        // 使用第一个图形的位置
        newPath->setPos(shapes.first()->pos());
        
        // 使用第一个图形的样式
        newPath->setFillBrush(shapes.first()->fillBrush());
        newPath->setStrokePen(shapes.first()->strokePen());
    }
    
    // 添加到场景
    m_scene->addItem(newPath);
    
    // 删除原始图形
    for (DrawingShape *shape : shapes) {
        m_scene->removeItem(shape);
        delete shape;
    }
    
    // 选中新创建的图形
    newPath->setSelected(true);
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    emit statusMessageChanged(QString("%1操作完成").arg(opName));
    emit pathOperationCompleted(opName);
}

void PathOperationsManager::executeBooleanOperation(int op)
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.size() < 2) {
        emit statusMessageChanged("需要选择至少两个路径进行布尔运算");
        return;
    }
    
    // 获取前两个选中的形状
    DrawingShape *shape1 = qgraphicsitem_cast<DrawingShape*>(selected[0]);
    DrawingShape *shape2 = qgraphicsitem_cast<DrawingShape*>(selected[1]);
    
    if (!shape1 || !shape2) {
        emit statusMessageChanged("选择的对象不是有效的路径");
        return;
    }
    
    // 执行布尔运算
    QPainterPath result;
    try {
        // 获取形状的实际位置
        QPointF pos1 = shape1->pos();
        QPointF pos2 = shape2->pos();
        
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
        
        result = PathEditor::booleanOperation(
            path1WithPos, 
            path2WithPos, 
            static_cast<PathEditor::BooleanOperation>(op)
        );
    } catch (...) {
        emit statusMessageChanged("布尔运算异常");
        return;
    }
    
    // 检查结果是否为空
    if (result.isEmpty()) {
        emit statusMessageChanged("布尔运算结果为空");
        return;
    }
    
    // 创建新的路径对象
    DrawingPath *newPath = new DrawingPath();
    
    // 计算结果路径的边界框
    QRectF resultBounds = result.boundingRect();
    
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
    
    // 从场景中移除原始形状
    m_scene->removeItem(shape1);
    m_scene->removeItem(shape2);
    
    // 添加新路径到场景
    m_scene->addItem(newPath);
    newPath->setSelected(true);
    
    // 删除原始形状
    delete shape1;
    delete shape2;
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    QString opName;
    switch (static_cast<PathEditor::BooleanOperation>(op)) {
        case PathEditor::Union: opName = "合并"; break;
        case PathEditor::Intersection: opName = "相交"; break;
        case PathEditor::Subtraction: opName = "减去"; break;
        case PathEditor::Xor: opName = "异或"; break;
        default: opName = "布尔运算"; break;
    }
    
    emit statusMessageChanged(QString("%1操作完成").arg(opName));
    emit pathOperationCompleted(opName);
}

void PathOperationsManager::executePathOperation(const QString &operation)
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        emit statusMessageChanged("需要选择一个路径进行操作");
        return;
    }
    
    DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(selected[0]);
    if (!shape) {
        emit statusMessageChanged("选择的对象不是有效的路径");
        return;
    }
    
    // 使用变换后的路径
    QPainterPath transformedPath = shape->transformedShape();
    QPainterPath resultPath;
    
    if (operation == "simplify") {
        resultPath = PathEditor::simplifyPath(transformedPath, 0.5);
    } else if (operation == "smooth") {
        resultPath = PathEditor::smoothPath(transformedPath, 0.5);
    } else if (operation == "curve") {
        resultPath = PathEditor::convertToCurve(transformedPath);
    } else if (operation == "offset") {
        resultPath = PathEditor::offsetPath(transformedPath, 5);
    } else if (operation == "clip") {
        // 使用图形的边界框作为裁剪区域
        QRectF bounds = transformedPath.boundingRect();
        QRectF clipRect = bounds.adjusted(10, 10, -10, -10); // 稍微缩小边界框
        resultPath = PathEditor::clipPath(transformedPath, clipRect);
    }
    
    if (resultPath.isEmpty()) {
        emit statusMessageChanged("路径操作失败");
        return;
    }
    
    // 创建一个新的DrawingPath来存储结果
    DrawingPath *newPath = new DrawingPath();
    
    // 重置路径的位置信息
    QRectF bounds = resultPath.boundingRect();
    QTransform offsetTransform;
    offsetTransform.translate(-bounds.left(), -bounds.top());
    QPainterPath adjustedPath = offsetTransform.map(resultPath);
    newPath->setPath(adjustedPath);
    newPath->setPos(shape->pos() + bounds.topLeft());
    newPath->setStrokePen(shape->strokePen());
    newPath->setFillBrush(shape->fillBrush());
    
    // 直接替换原路径（简化版本，暂时不使用撤销命令）
    m_scene->removeItem(shape);
    m_scene->addItem(newPath);
    newPath->setSelected(true);
    delete shape;
    
    // 标记场景已修改
    m_scene->setModified(true);
    
    QString opName;
    if (operation == "simplify") opName = "简化";
    else if (operation == "smooth") opName = "平滑";
    else if (operation == "curve") opName = "转换为曲线";
    else if (operation == "offset") opName = "偏移";
    else if (operation == "clip") opName = "裁剪";
    else opName = "路径操作";
    
    emit statusMessageChanged(QString("%1操作完成").arg(opName));
    emit pathOperationCompleted(opName);
}

void PathOperationsManager::addPathOperationsToMenu(QMenu *menu, const QPointF &pos)
{
    // TODO: 从MainWindow移动上下文菜单的路径操作部分
    Q_UNUSED(menu);
    Q_UNUSED(pos);
}

void PathOperationsManager::generateShape()
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        emit statusMessageChanged("请先选择要生成图形的路径");
        return;
    }
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape && shape->shapeType() == DrawingShape::Path) {
            DrawingPath *drawingPath = dynamic_cast<DrawingPath*>(shape);
            if (drawingPath) {
                QPainterPath originalPath = drawingPath->path();
                
                // 尝试生成标准图形：星形
                QPointF center = originalPath.boundingRect().center();
                qreal radius = qMax(originalPath.boundingRect().width(), originalPath.boundingRect().height()) / 2;
                QPainterPath starPath = PathEditor::createStar(center, radius, 5);
                
                drawingPath->setPath(starPath);
                emit statusMessageChanged("已生成星形");
                emit pathOperationCompleted("生成图形");
            }
        }
    }
    
    m_scene->update();
    m_scene->setModified(true);
}

void PathOperationsManager::createShapeAtPosition(const QString &shapeType, const QPointF &pos)
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QPainterPath shape;
    
    if (shapeType == "arrow") {
        shape = PathEditor::createArrow(
            QPointF(pos.x() - 50, pos.y()),
            QPointF(pos.x() + 50, pos.y())
        );
    } else if (shapeType == "star") {
        shape = PathEditor::createStar(pos, 50, 5);
    } else if (shapeType == "gear") {
        shape = PathEditor::createGear(pos, 50, 8);
    }
    
    if (shape.isEmpty()) {
        emit statusMessageChanged("创建形状失败");
        return;
    }
    
    // 创建新的路径对象
    DrawingPath *newPath = new DrawingPath();
    newPath->setPath(shape);
    newPath->setPos(pos);
    
    // 设置默认样式
    newPath->setFillBrush(QBrush(Qt::blue));
    newPath->setStrokePen(QPen(Qt::black, 2));
    
    // 添加到场景
    m_scene->addItem(newPath);
    newPath->setSelected(true);
    
    m_scene->setModified(true);
    emit statusMessageChanged(QString("已创建%1").arg(shapeType));
    emit pathOperationCompleted(QString("创建%1").arg(shapeType));
}

