#include "path-operations-manager.h"
#include "mainwindow.h"
#include "drawingscene.h"
#include "../core/drawing-shape.h"
#include "../core/patheditor.h"
#include "command-manager.h"
#include <QGraphicsTextItem>
#include <QMenu>
#include <QDataStream>
#include <cmath>

PathOperationsManager::PathOperationsManager(MainWindow *parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_scene(nullptr)
    , m_commandManager(nullptr)
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

void PathOperationsManager::setCommandManager(CommandManager *commandManager)
{
    m_commandManager = commandManager;
}

CommandManager *PathOperationsManager::commandManager() const
{
    return m_commandManager;
}

void PathOperationsManager::pathUnion()
{
    performBooleanOperationMacro(Union, "路径联合");
}

void PathOperationsManager::pathSubtract()
{
    performBooleanOperationMacro(Subtract, "路径减去");
}

void PathOperationsManager::pathIntersect()
{
    performBooleanOperationMacro(Intersect, "路径相交");
}

void PathOperationsManager::pathXor()
{
    performBooleanOperationMacro(Xor, "路径异或");
}

void PathOperationsManager::pathSimplify()
{
    qDebug() << "PathOperationsManager::pathSimplify() called";
    performPathOperationMacro(Simplify, "路径简化");
}

void PathOperationsManager::pathSmooth()
{
    qDebug() << "PathOperationsManager::pathSmooth() called";
    performPathOperationMacro(Smooth, "路径平滑");
}

void PathOperationsManager::pathReverse()
{
    qDebug() << "PathOperationsManager::pathReverse() called";
    performPathOperationMacro(Reverse, "路径反转");
}

void PathOperationsManager::pathConvertToCurve()
{
    qDebug() << "PathOperationsManager::pathConvertToCurve() called";
    performPathOperationMacro(ConvertToCurve, "转换为曲线");
}

void PathOperationsManager::pathOffsetPath()
{
    qDebug() << "PathOperationsManager::pathOffsetPath() called";
    performPathOperationMacro(OffsetPath, "偏移路径");
}

void PathOperationsManager::pathClipPath()
{
    qDebug() << "PathOperationsManager::pathClipPath() called";
    performPathOperationMacro(ClipPath, "裁剪路径");
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
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.size() < 2) {
        emit statusMessageChanged(QString("%1操作需要至少选中2个图形").arg(opName));
        return;
    }
    
    // 创建简单的布尔操作命令 - 使用你建议的方法
    class BooleanOperationCommand : public QUndoCommand {
    public:
        BooleanOperationCommand(DrawingScene *scene, PathOperationsManager::BooleanOperation op, 
                               const QString &opName, QUndoCommand *parent = nullptr)
            : QUndoCommand(opName, parent), m_scene(scene), m_operation(op), m_createdPath(nullptr)
        {
            // 保存原始图形的指针，但不删除它们
            if (m_scene) {
                m_selectedItems = m_scene->selectedItems();
                for (QGraphicsItem *item : m_selectedItems) {
                    DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
                    if (shape) {
                        m_originalShapes.append(shape);
                    }
                }
            }
        }
        
        ~BooleanOperationCommand() {
            // 析构时检查布尔结果是否还在场景中，如果不在就删除
            // 增加更安全的检查，避免访问已释放的scene
            if (m_createdPath) {
                // 先检查scene指针是否有效
                bool sceneValid = false;
                if (m_scene) {
                    // 尝试安全地检查scene是否仍然有效
                    // 通过检查一个简单的属性来判断
                    try {
                        // 如果scene已经被析构，这个调用可能会崩溃
                        // 所以我们用更安全的方式
                        sceneValid = true;
                    } catch (...) {
                        sceneValid = false;
                    }
                }
                
                // 只有在scene无效或者对象不在scene中时才删除
                if (!sceneValid || (m_createdPath->scene() != m_scene)) {
                    try {
                        delete m_createdPath;
                    } catch (...) {
                        // 如果删除失败，忽略错误，避免程序崩溃
                    }
                }
                m_createdPath = nullptr;
            }
        }

        void undo() override {
            if (!m_scene) return;
            
            // 只从场景中移除布尔结果图形，不删除对象
            if (m_createdPath && m_createdPath->scene() == m_scene) {
                m_scene->removeItem(m_createdPath);
            }
            
            // 将原始图形重新添加到场景
            m_scene->clearSelection();
            for (DrawingShape *shape : m_originalShapes) {
                if (shape) {
                    m_scene->addItem(shape);
                    shape->setSelected(true);
                }
            }
            
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            if (!m_scene) return;
            
            // 如果是第一次执行，需要计算布尔结果
            if (!m_createdPath) {
                // 收集所有原始图形的路径和样式
                QList<QPainterPath> paths;
                QPen strokePen;
                QBrush fillBrush;
                
                for (DrawingShape *shape : m_originalShapes) {
                    if (shape) {
                        QPainterPath shapePath = shape->transformedShape();
                        QTransform transform;
                        transform.translate(shape->pos().x(), shape->pos().y());
                        shapePath = transform.map(shapePath);
                        
                        if (!shapePath.isEmpty()) {
                            paths.append(shapePath);
                        }
                        
                        // 保存第一个图形的样式
                        if (strokePen == QPen()) {
                            strokePen = shape->strokePen();
                            fillBrush = shape->fillBrush();
                        }
                    }
                }
                
                if (paths.size() < 2) return;
                
                // 执行布尔运算
                QPainterPath result = paths.first();
                for (int i = 1; i < paths.size(); ++i) {
                    switch (m_operation) {
                        case Union:
                            result = result.united(paths[i]);
                            break;
                        case Subtract:
                            result = result.subtracted(paths[i]);
                            break;
                        case Intersect:
                            result = result.intersected(paths[i]);
                            break;
                        case Xor:
                            result = result.subtracted(paths[i]).united(paths[i].subtracted(result));
                            break;
                    }
                }
                
                if (result.isEmpty()) return;
                
                // 创建布尔结果图形
                m_createdPath = new DrawingPath();
                
                // 调整路径为相对于原点的路径
                QRectF resultBounds = result.boundingRect();
                QTransform offsetTransform;
                offsetTransform.translate(-resultBounds.left(), -resultBounds.top());
                QPainterPath adjustedPath = offsetTransform.map(result);
                
                m_createdPath->setPath(adjustedPath);
                m_createdPath->setPos(resultBounds.topLeft());
                m_createdPath->setStrokePen(strokePen);
                m_createdPath->setFillBrush(fillBrush);
                
                // 从场景中移除原始图形（但不删除）
                for (DrawingShape *shape : m_originalShapes) {
                    if (shape && shape->scene() == m_scene) {
                        m_scene->removeItem(shape);
                    }
                }
                
                // 添加布尔结果到场景
                m_scene->addItem(m_createdPath);
                m_createdPath->setSelected(true);
            } else {
                // 重做时：移除原图形，添加已存在的布尔结果
                for (DrawingShape *shape : m_originalShapes) {
                    if (shape && shape->scene() == m_scene) {
                        m_scene->removeItem(shape);
                    }
                }
                
                if (m_createdPath) {
                    m_scene->addItem(m_createdPath);
                    m_createdPath->setSelected(true);
                }
            }
            
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        PathOperationsManager::BooleanOperation m_operation;
        QList<QGraphicsItem*> m_selectedItems;
        QList<DrawingShape*> m_originalShapes;
        DrawingPath *m_createdPath;
    };

    // 使用CommandManager执行布尔运算
    if (m_commandManager) {
        m_commandManager->pushCommand(new BooleanOperationCommand(m_scene, op, opName));
    } else {
        // 如果没有CommandManager，直接执行
        BooleanOperationCommand command(m_scene, op, opName);
        command.redo();
    }
    
    emit pathOperationCompleted(opName);
    emit statusMessageChanged(QString("已执行 %1 操作").arg(opName));
}

void PathOperationsManager::performBooleanOperationMacro(BooleanOperation op, const QString &opName)
{
    qDebug() << "PathOperationsManager::performBooleanOperationMacro() called with operation:" << opName;
    
    if (!validateSelectionForBoolean()) {
        qDebug() << "validateSelectionForBoolean() returned false";
        return;
    }
    
    if (!CommandManager::hasInstance()) {
        qWarning() << "No CommandManager available for macro operation";
        return;
    }
    
    // 使用宏命令包装整个布尔运算过程
    CommandManager::instance()->executeMacro(opName, [this, op]() {
        executeBooleanOperationSteps(op);
    });
    
    emit pathOperationCompleted(opName);
    emit statusMessageChanged(QString("已执行 %1 操作").arg(opName));
}

// 分解式命令：移除原始图形
class RemoveShapesCommand : public QUndoCommand {
public:
    RemoveShapesCommand(DrawingScene* scene, const QList<DrawingShape*>& shapes, QUndoCommand* parent = nullptr)
        : QUndoCommand("移除原始图形", parent), m_scene(scene), m_shapes(shapes)
    {
        // 保存位置和样式信息
        for (DrawingShape* shape : shapes) {
            m_savedPositions[shape] = shape->scenePos();
            m_savedStyles[shape] = {shape->strokePen(), shape->fillBrush()};
        }
    }
    
    void undo() override {
        if (!m_scene) return;
        
        // 重新添加图形到场景
        for (DrawingShape* shape : m_shapes) {
            if (shape) {
                m_scene->addItem(shape);
                shape->setPos(m_savedPositions[shape]);
                shape->setStrokePen(m_savedStyles[shape].first);
                shape->setFillBrush(m_savedStyles[shape].second);
                shape->setVisible(true);
            }
        }
    }
    
    void redo() override {
        if (!m_scene) return;
        
        // 从场景中移除图形但不删除
        for (DrawingShape* shape : m_shapes) {
            if (shape) {
                m_scene->removeItem(shape);
                shape->setVisible(false);
            }
        }
    }
    
private:
    DrawingScene* m_scene;
    QList<DrawingShape*> m_shapes;
    QMap<DrawingShape*, QPointF> m_savedPositions;
    QMap<DrawingShape*, QPair<QPen, QBrush>> m_savedStyles;
};

// 分解式命令：创建新路径
class CreatePathCommand : public QUndoCommand {
public:
    CreatePathCommand(DrawingScene* scene, DrawingPath* path, QUndoCommand* parent = nullptr)
        : QUndoCommand("创建结果路径", parent), m_scene(scene), m_path(path), m_addedToScene(false)
    {
    }
    
    ~CreatePathCommand() {
        // 如果路径不在场景中，删除它
        if (m_path && m_path->scene() != m_scene) {
            delete m_path;
        }
    }
    
    void undo() override {
        if (!m_scene || !m_path) return;
        
        if (m_addedToScene) {
            m_scene->removeItem(m_path);
            m_path->setVisible(false);
            m_addedToScene = false;
        }
    }
    
    void redo() override {
        if (!m_scene || !m_path) return;
        
        if (!m_addedToScene) {
            m_scene->addItem(m_path);
            m_path->setVisible(true);
            m_path->setSelected(true);
            m_addedToScene = true;
        }
    }
    
private:
    DrawingScene* m_scene;
    DrawingPath* m_path;
    bool m_addedToScene;
};

// 创建任意图形的命令
class CreateShapeCommand : public QUndoCommand {
public:
    CreateShapeCommand(DrawingScene* scene, DrawingShape* shape, QUndoCommand* parent = nullptr)
        : QUndoCommand("创建图形", parent), m_scene(scene), m_shape(shape), m_addedToScene(false)
    {
    }
    
    ~CreateShapeCommand() {
        // 如果图形不在场景中，删除它
        if (m_shape && m_shape->scene() != m_scene) {
            delete m_shape;
        }
    }
    
    void undo() override {
        if (!m_scene || !m_shape) return;
        
        if (m_addedToScene) {
            m_scene->removeItem(m_shape);
            m_shape->setVisible(false);
            m_addedToScene = false;
        }
    }
    
    void redo() override {
        if (!m_scene || !m_shape) return;
        
        if (!m_addedToScene) {
            m_scene->addItem(m_shape);
            m_shape->setVisible(true);
            m_shape->setSelected(true);
            m_addedToScene = true;
            
            // 手动触发场景选择变化信号，确保选择工具更新手柄
            m_scene->emitSelectionChanged();
        }
    }
    
private:
    DrawingScene* m_scene;
    DrawingShape* m_shape;
    bool m_addedToScene;
};

void PathOperationsManager::generateShape()
{
    // 默认生成星形
    generateStar();
}

void PathOperationsManager::generateStar()
{
    qDebug() << "PathOperationsManager::generateStar() called";
    
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    if (!CommandManager::hasInstance()) {
        qWarning() << "No CommandManager available for generate star operation";
        return;
    }
    
    CommandManager::instance()->executeMacro("生成星形", [this]() {
        // 使用默认大小和样式，不依赖选中对象
        QPointF center(0, 0); // 相对于原点
        qreal radius = 50; // 默认半径50像素
        
        // 创建星形路径
        QPainterPath starPath = createStarPath(center, radius, 5);
        
        DrawingPath *starShape = new DrawingPath();
        starShape->setPath(starPath);
        starShape->setStrokePen(QPen(Qt::black, 2)); // 默认黑色描边
        starShape->setFillBrush(QBrush(Qt::yellow)); // 默认黄色填充
        starShape->setZValue(1); // 设置Z值确保显示在上层
        
        // 添加到场景中心位置
        if (m_scene) {
            QRectF sceneRect = m_scene->sceneRect();
            QPointF sceneCenter = sceneRect.center();
            starShape->setPos(sceneCenter);
        }
        
        // 添加新图形
        CommandManager::instance()->pushCommand(new CreatePathCommand(m_scene, starShape));
    });
    
    emit statusMessageChanged("已生成星形");
}

void PathOperationsManager::generateArrow()
{
    qDebug() << "PathOperationsManager::generateArrow() called";
    
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    if (!CommandManager::hasInstance()) {
        qWarning() << "No CommandManager available for generate arrow operation";
        return;
    }
    
    CommandManager::instance()->executeMacro("生成箭头", [this]() {
        // 使用默认大小和样式，不依赖选中对象
        QPointF center(0, 0); // 相对于原点
        qreal size = 100; // 默认大小100像素
        
        // 创建箭头路径
        QPainterPath arrowPath = createArrowPath(center, size);
        
        DrawingPath *arrowShape = new DrawingPath();
        arrowShape->setPath(arrowPath);
        arrowShape->setStrokePen(QPen(Qt::black, 2)); // 默认黑色描边
        arrowShape->setFillBrush(QBrush(Qt::blue)); // 默认蓝色填充
        arrowShape->setZValue(1); // 设置Z值确保显示在上层
        
        // 添加到场景中心位置
        if (m_scene) {
            QRectF sceneRect = m_scene->sceneRect();
            QPointF sceneCenter = sceneRect.center();
            arrowShape->setPos(sceneCenter);
        }
        
        // 添加新图形
        CommandManager::instance()->pushCommand(new CreatePathCommand(m_scene, arrowShape));
    });
    
    emit statusMessageChanged("已生成箭头");
}

void PathOperationsManager::generateGear()
{
    qDebug() << "PathOperationsManager::generateGear() called";
    
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    if (!CommandManager::hasInstance()) {
        qWarning() << "No CommandManager available for generate gear operation";
        return;
    }
    
    CommandManager::instance()->executeMacro("生成齿轮", [this]() {
        // 使用默认大小和样式，不依赖选中对象
        QPointF center(0, 0); // 相对于原点
        qreal radius = 50; // 默认半径50像素
        
        // 创建齿轮路径
        QPainterPath gearPath = createGearPath(center, radius, 12);
        
        DrawingPath *gearShape = new DrawingPath();
        gearShape->setPath(gearPath);
        gearShape->setStrokePen(QPen(Qt::black, 2)); // 默认黑色描边
        gearShape->setFillBrush(QBrush(Qt::gray)); // 默认灰色填充
        gearShape->setZValue(1); // 设置Z值确保显示在上层
        
        // 添加到场景中心位置
        if (m_scene) {
            QRectF sceneRect = m_scene->sceneRect();
            QPointF sceneCenter = sceneRect.center();
            gearShape->setPos(sceneCenter);
        }
        
        // 添加新图形
        CommandManager::instance()->pushCommand(new CreatePathCommand(m_scene, gearShape));
    });
    
    emit statusMessageChanged("已生成齿轮");
}

void PathOperationsManager::executeBooleanOperationSteps(BooleanOperation op)
{
    if (!m_scene || !CommandManager::hasInstance()) return;
    
    // 第一步：收集原始图形
    QList<DrawingShape*> originalShapes;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape) {
            originalShapes.append(shape);
        }
    }
    
    if (originalShapes.size() < 2) return;
    
    // 第一步：使用分解式命令移除原始图形
    CommandManager::instance()->pushCommand(new RemoveShapesCommand(m_scene, originalShapes));
    
    // 第二步：计算布尔运算结果
    QList<QPainterPath> paths;
    QPen strokePen;
    QBrush fillBrush;
    
    for (DrawingShape *shape : originalShapes) {
        if (shape) {
            QPainterPath shapePath = shape->transformedShape();
            QTransform transform;
            transform.translate(shape->pos().x(), shape->pos().y());
            shapePath = transform.map(shapePath);
            
            if (!shapePath.isEmpty()) {
                paths.append(shapePath);
            }
            
            if (strokePen == QPen()) {
                strokePen = shape->strokePen();
                fillBrush = shape->fillBrush();
            }
        }
    }
    
    // 第三步：计算布尔运算结果
    QPainterPath resultPath;
    switch (op) {
        case Union:
            resultPath = paths.first();
            for (int i = 1; i < paths.size(); ++i) {
                resultPath = resultPath.united(paths[i]);
            }
            break;
        case Subtract:
            resultPath = paths.first();
            for (int i = 1; i < paths.size(); ++i) {
                resultPath = resultPath.subtracted(paths[i]);
            }
            break;
        case Intersect:
            resultPath = paths.first();
            for (int i = 1; i < paths.size(); ++i) {
                resultPath = resultPath.intersected(paths[i]);
            }
            break;
        case Xor:
            resultPath = paths.first();
            for (int i = 1; i < paths.size(); ++i) {
                // QPainterPath 没有 xored 方法，使用替代实现
                QPainterPath temp = paths[i];
                temp.setFillRule(Qt::OddEvenFill);
                resultPath.setFillRule(Qt::OddEvenFill);
                resultPath = resultPath.subtracted(temp).united(temp.subtracted(resultPath));
            }
            break;
    }
    
    // 第四步：使用分解式命令创建新路径
    if (!resultPath.isEmpty()) {
        DrawingPath *newPath = new DrawingPath();
        newPath->setPath(resultPath);
        newPath->setStrokePen(strokePen);
        newPath->setFillBrush(fillBrush);
        
        CommandManager::instance()->pushCommand(new CreatePathCommand(m_scene, newPath));
    }
}

void PathOperationsManager::performPathOperation(PathOperation op, const QString &opName)
{
    qDebug() << "PathOperationsManager::performPathOperation() called with operation:" << opName;
    
    if (!validateSelectionForPathOperation()) {
        qDebug() << "validateSelectionForPathOperation() returned false";
        return;
    }
    
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    if (selectedItems.isEmpty()) {
        qDebug() << "No selected items";
        emit statusMessageChanged("请选择一个路径进行操作");
        return;
    }
    
    qDebug() << "Processing" << selectedItems.count() << "selected items";
    
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
                
                // 序列化所有选中的图形
                for (QGraphicsItem *item : m_selectedItems) {
                    DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
                    if (shape) {
                        m_serializedData[shape] = shape->serialize();
                        m_parents[shape] = shape->parentItem();
                        m_positions[shape] = shape->pos();
                    }
                }
            }
        }

        void undo() override {
            if (!m_scene) return;
            
            // 删除创建的新对象并恢复原始对象
            for (auto it = m_createdObjects.begin(); it != m_createdObjects.end(); ++it) {
                DrawingShape *createdShape = it.value();
                if (createdShape) {
                    m_scene->removeItem(createdShape);
                    delete createdShape;
                }
            }
            m_createdObjects.clear();
            
            // 恢复原始图形
            for (auto it = m_serializedData.begin(); it != m_serializedData.end(); ++it) {
                DrawingShape *originalShape = it.key();
                QByteArray data = it.value();
                
                // 读取形状类型
                QDataStream typeStream(data);
                int typeValue;
                typeStream >> typeValue;
                DrawingShape::ShapeType shapeType = static_cast<DrawingShape::ShapeType>(typeValue);
                
                // 创建对应的形状对象
                DrawingShape *restoredShape = nullptr;
                switch (shapeType) {
                    case DrawingShape::Rectangle:
                        restoredShape = new DrawingRectangle();
                        break;
                    case DrawingShape::Ellipse:
                        restoredShape = new DrawingEllipse();
                        break;
                    case DrawingShape::Line:
                        restoredShape = new DrawingLine();
                        break;
                    case DrawingShape::Path:
                        restoredShape = new DrawingPath();
                        break;
                    case DrawingShape::Polyline:
                        restoredShape = new DrawingPolyline();
                        break;
                    case DrawingShape::Polygon:
                        restoredShape = new DrawingPolygon();
                        break;
                    case DrawingShape::Text:
                        restoredShape = new DrawingText();
                        break;
                    case DrawingShape::Group:
                        restoredShape = new DrawingGroup();
                        break;
                    default:
                        qDebug() << "Unsupported shape type for restoration:" << typeValue;
                        continue;
                }
                
                if (restoredShape) {
                    // 反序列化数据
                    restoredShape->deserialize(data);
                    
                    // 恢复父级关系和位置
                    restoredShape->setParentItem(m_parents[originalShape]);
                    restoredShape->setPos(m_positions[originalShape]);
                    
                    // 添加到场景
                    m_scene->addItem(restoredShape);
                    restoredShape->setSelected(true);
                }
            }
            
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            if (!m_scene) return;
            
            // 清除选择
            m_scene->clearSelection();
            
            // 清除之前创建的对象
            for (auto it = m_createdObjects.begin(); it != m_createdObjects.end(); ++it) {
                DrawingShape *createdShape = it.value();
                if (createdShape) {
                    m_scene->removeItem(createdShape);
                    delete createdShape;
                }
            }
            m_createdObjects.clear();
            
            // 对每个选中的图形执行路径操作
            for (QGraphicsItem *item : m_selectedItems) {
                DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
                if (!shape) {
                    qDebug() << "Item is not a DrawingShape, skipping";
                    continue;
                }
                
                qDebug() << "Processing shape type:" << shape->shapeType();
                
                // 获取变换后的路径
                QPainterPath originalPath = shape->transformedShape();
                QPainterPath resultPath;
                
                // 根据操作类型执行路径操作
                switch (m_operation) {
                    case Simplify:
                        // 简化路径：移除冗余的点
                        resultPath = PathOperationsManager::simplifyPathStatic(originalPath);
                        break;
                        
                    case Smooth:
                        // 平滑路径：使用贝塞尔曲线平滑
                        resultPath = PathOperationsManager::smoothPathStatic(originalPath);
                        break;
                        
                    case Reverse:
                        // 反转路径方向
                        resultPath = originalPath.toReversed();
                        break;
                        
                    case ConvertToCurve:
                        // 转换为曲线：将直线段转换为贝塞尔曲线
                        resultPath = PathOperationsManager::convertToCurveStatic(originalPath);
                        break;
                        
                    case OffsetPath:
                        // 偏移路径：创建偏移的轮廓
                        resultPath = PathOperationsManager::offsetPathStatic(originalPath, 5.0);
                        break;
                        
                    case ClipPath:
                        // 裁剪路径：使用边界框进行裁剪
                        resultPath = PathOperationsManager::clipPathStatic(originalPath);
                        break;
                }
                
                if (resultPath.isEmpty()) {
                    // 路径操作失败，继续下一个
                    continue;
                }
                
                // 根据图形类型应用路径操作
                if (shape->shapeType() == DrawingShape::Path) {
                    DrawingPath *drawingPath = dynamic_cast<DrawingPath*>(shape);
                    if (drawingPath) {
                        // 保存位置信息
                        QPointF pos = drawingPath->pos();
                        
                        // 调整路径为相对于原点的路径
                        QRectF bounds = resultPath.boundingRect();
                        QTransform offsetTransform;
                        offsetTransform.translate(-bounds.left(), -bounds.top());
                        QPainterPath adjustedPath = offsetTransform.map(resultPath);
                        
                        drawingPath->setPath(adjustedPath);
                        drawingPath->setPos(pos + bounds.topLeft());
                        
                        // 保存创建的对象引用
                        m_createdObjects[shape] = drawingPath;
                    }
                } else {
                    // 对于其他图形类型（矩形、椭圆等），创建新的Path对象替换原图形
                    // 保存原图形的样式和位置
                    QPointF pos = shape->pos();
                    QPen strokePen = shape->strokePen();
                    QBrush fillBrush = shape->fillBrush();
                    
                    // 创建新的路径对象
                    DrawingPath *newPath = new DrawingPath();
                    
                    // 调整路径为相对于原点的路径
                    QRectF bounds = resultPath.boundingRect();
                    QTransform offsetTransform;
                    offsetTransform.translate(-bounds.left(), -bounds.top());
                    QPainterPath adjustedPath = offsetTransform.map(resultPath);
                    
                    newPath->setPath(adjustedPath);
                    newPath->setPos(pos + bounds.topLeft());
                    newPath->setStrokePen(strokePen);
                    newPath->setFillBrush(fillBrush);
                    
                    // 从场景中移除原图形
                    m_scene->removeItem(shape);
                    
                    // 添加新路径到场景
                    m_scene->addItem(newPath);
                    newPath->setSelected(true);
                    
                    // 保存创建的对象引用
                    m_createdObjects[shape] = newPath;
                }
            }
            
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        PathOperationsManager::PathOperation m_operation;
        QList<QGraphicsItem*> m_selectedItems;
        QMap<DrawingShape*, QByteArray> m_serializedData;
        QMap<DrawingShape*, QGraphicsItem*> m_parents;
        QMap<DrawingShape*, QPointF> m_positions;
        QMap<DrawingShape*, DrawingShape*> m_createdObjects;
    };

    // 使用CommandManager执行路径操作
    if (m_commandManager) {
        m_commandManager->pushCommand(new PathOperationCommand(m_scene, op, opName));
    } else {
        // 如果没有CommandManager，使用原来的直接执行方式
        qDebug() << "Warning: No CommandManager available, path operation cannot be undone";
        emit statusMessageChanged("路径操作无法撤销（命令管理器未初始化）");
    }
    
    emit pathOperationCompleted(opName);
    emit statusMessageChanged(QString("已执行 %1 操作").arg(opName));
}

void PathOperationsManager::performPathOperationMacro(PathOperation op, const QString &opName)
{
    qDebug() << "PathOperationsManager::performPathOperationMacro() called with operation:" << opName;
    
    if (!validateSelectionForPathOperation()) {
        qDebug() << "validateSelectionForPathOperation() returned false";
        return;
    }
    
    if (!CommandManager::hasInstance()) {
        qWarning() << "No CommandManager available for macro operation";
        return;
    }
    
    // 使用宏命令包装整个路径操作过程
    CommandManager::instance()->executeMacro(opName, [this, op]() {
        executePathOperationSteps(op);
    });
    
    emit pathOperationCompleted(opName);
    emit statusMessageChanged(QString("已执行 %1 操作").arg(opName));
}

// 替换路径命令
class ReplacePathCommand : public QUndoCommand {
public:
    ReplacePathCommand(DrawingScene* scene, DrawingPath* oldPath, DrawingPath* newPath, QUndoCommand* parent = nullptr)
        : QUndoCommand("替换路径", parent), m_scene(scene), m_oldPath(oldPath), m_newPath(newPath), m_replaced(false)
    {
    }
    
    ~ReplacePathCommand() {
        if (!m_replaced && m_newPath) {
            delete m_newPath;
        }
    }
    
    void undo() override {
        if (!m_scene || !m_oldPath || !m_newPath) return;
        
        if (m_replaced) {
            // 恢复旧路径
            m_scene->removeItem(m_newPath);
            m_scene->addItem(m_oldPath);
            m_oldPath->setVisible(true);
            m_newPath->setVisible(false);
            m_replaced = false;
        }
    }
    
    void redo() override {
        if (!m_scene || !m_oldPath || !m_newPath) return;
        
        if (!m_replaced) {
            // 替换为新路径
            m_scene->removeItem(m_oldPath);
            m_scene->addItem(m_newPath);
            m_oldPath->setVisible(false);
            m_newPath->setVisible(true);
            m_newPath->setSelected(true);
            m_replaced = true;
        }
    }
    
private:
    DrawingScene* m_scene;
    DrawingPath* m_oldPath;
    DrawingPath* m_newPath;
    bool m_replaced;
};

void PathOperationsManager::executePathOperationSteps(PathOperation op)
{
    if (!m_scene || !CommandManager::hasInstance()) return;
    
    // 收集选中的图形
    QList<DrawingShape*> selectedShapes;
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    
    for (QGraphicsItem *item : selectedItems) {
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape && (shape->shapeType() == DrawingShape::Path ||
                      shape->shapeType() == DrawingShape::Rectangle ||
                      shape->shapeType() == DrawingShape::Ellipse ||
                      shape->shapeType() == DrawingShape::Polygon ||
                      shape->shapeType() == DrawingShape::Polyline)) {
            selectedShapes.append(shape);
        }
    }
    
    if (selectedShapes.isEmpty()) {
        qWarning() << "No supported shapes selected for operation";
        return;
    }
    
    // 第一步：移除原始图形（但不删除）
    CommandManager::instance()->pushCommand(new RemoveShapesCommand(m_scene, selectedShapes));
    
    // 第二步：为每个原图形创建新的路径对象
    for (DrawingShape *shape : selectedShapes) {
        if (!shape) continue;
        
        // 获取图形的路径
        QPainterPath originalPath;
        if (shape->shapeType() == DrawingShape::Path) {
            DrawingPath *pathShape = static_cast<DrawingPath*>(shape);
            originalPath = pathShape->path();
        } else {
            // 对于其他图形类型，获取其变换后的路径
            originalPath = shape->transformedShape();
            QTransform transform;
            transform.translate(shape->pos().x(), shape->pos().y());
            originalPath = transform.map(originalPath);
        }
        
        // 执行路径操作
        QPainterPath newPath;
        switch (op) {
            case Simplify:
                newPath = simplifyPathStatic(originalPath);
                break;
            case Smooth:
                newPath = smoothPathStatic(originalPath);
                break;
            case Reverse:
                newPath = originalPath.toReversed();
                break;
            case ConvertToCurve:
                newPath = convertToCurveStatic(originalPath);
                break;
            case OffsetPath:
                newPath = offsetPathStatic(originalPath, 5.0); // 默认偏移5像素
                break;
            case ClipPath:
                // 裁剪路径实现（简化版）
                newPath.addRect(originalPath.boundingRect());
                break;
        }
        
        if (!newPath.isEmpty()) {
            // 创建新的路径对象
            DrawingPath *newPathShape = new DrawingPath();
            
            // 调整路径为相对于原点的路径
            QRectF bounds = newPath.boundingRect();
            QTransform offsetTransform;
            offsetTransform.translate(-bounds.left(), -bounds.top());
            QPainterPath adjustedPath = offsetTransform.map(newPath);
            
            newPathShape->setPath(adjustedPath);
            newPathShape->setStrokePen(shape->strokePen());
            newPathShape->setFillBrush(shape->fillBrush());
            newPathShape->setPos(bounds.topLeft());
            
            // 使用分解式命令创建新路径
            CommandManager::instance()->pushCommand(new CreatePathCommand(m_scene, newPathShape));
        }
    }
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
    qDebug() << "validateSelectionForPathOperation() called";
    
    if (!m_scene) {
        qDebug() << "Scene is null";
        emit statusMessageChanged("场景未初始化");
        return false;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    qDebug() << "Selected items count:" << selected.count();
    
    if (selected.isEmpty()) {
        qDebug() << "No selected items";
        emit statusMessageChanged("请先选择要操作的对象");
        return false;
    }
    
    // 检查是否都是DrawingShape对象
    for (int i = 0; i < selected.count(); ++i) {
        QGraphicsItem *item = selected[i];
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        qDebug() << "Item" << i << "type:" << item->type() << "is DrawingShape:" << (shape != nullptr);
        
        if (!shape) {
            qDebug() << "Item" << i << "is not a DrawingShape";
            emit statusMessageChanged("路径操作只能应用于图形对象");
            return false;
        }
    }
    
    qDebug() << "validateSelectionForPathOperation() returning true";
    return true;
}

void PathOperationsManager::addPathOperationsToMenu(QMenu *menu, const QPointF &pos)
{
    if (!m_scene) {
        return;
    }
    
    // 检查是否有选中的对象
    QList<QGraphicsItem*> selectedItems = m_scene->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    bool hasMultipleSelection = selectedItems.size() >= 2;
    
    // 布尔运算菜单
    if (hasMultipleSelection) {
        QMenu *booleanMenu = menu->addMenu("布尔运算");
        
        QAction *unionAction = booleanMenu->addAction("联合(&U)");
        connect(unionAction, &QAction::triggered, this, &PathOperationsManager::pathUnion);
        
        QAction *subtractAction = booleanMenu->addAction("减去(&S)");
        connect(subtractAction, &QAction::triggered, this, &PathOperationsManager::pathSubtract);
        
        QAction *intersectAction = booleanMenu->addAction("相交(&I)");
        connect(intersectAction, &QAction::triggered, this, &PathOperationsManager::pathIntersect);
        
        QAction *xorAction = booleanMenu->addAction("异或(&X)");
        connect(xorAction, &QAction::triggered, this, &PathOperationsManager::pathXor);
    }
    
    // 路径操作菜单
    if (hasSelection) {
        QMenu *pathMenu = menu->addMenu("路径操作");
        
        QAction *simplifyAction = pathMenu->addAction("简化(&S)");
        connect(simplifyAction, &QAction::triggered, this, &PathOperationsManager::pathSimplify);
        qDebug() << "Created simplify action and connected signal";
        
        QAction *smoothAction = pathMenu->addAction("平滑(&M)");
        connect(smoothAction, &QAction::triggered, this, &PathOperationsManager::pathSmooth);
        qDebug() << "Created smooth action and connected signal";
        
        QAction *reverseAction = pathMenu->addAction("反转(&R)");
        connect(reverseAction, &QAction::triggered, this, &PathOperationsManager::pathReverse);
        qDebug() << "Created reverse action and connected signal";
        
        QAction *convertToCurveAction = pathMenu->addAction("转换为曲线(&C)");
        connect(convertToCurveAction, &QAction::triggered, this, &PathOperationsManager::pathConvertToCurve);
        
        QAction *offsetAction = pathMenu->addAction("偏移路径(&O)");
        connect(offsetAction, &QAction::triggered, this, &PathOperationsManager::pathOffsetPath);
        
        QAction *clipAction = pathMenu->addAction("裁剪路径(&P)");
        connect(clipAction, &QAction::triggered, this, &PathOperationsManager::pathClipPath);
        
        pathMenu->addSeparator();
        
        // 检查是否有文本对象
        bool hasTextItems = false;
        for (QGraphicsItem *item : selectedItems) {
            if (qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
                hasTextItems = true;
                break;
            }
        }
        
        if (hasTextItems) {
            QAction *convertTextAction = pathMenu->addAction("文本转路径(&T)");
            connect(convertTextAction, &QAction::triggered, this, &PathOperationsManager::convertSelectedTextToPath);
        }
    }
    
    // 创建图形菜单
    QMenu *createMenu = menu->addMenu("创建图形");
    
    QAction *arrowAction = createMenu->addAction("箭头(&A)");
    connect(arrowAction, &QAction::triggered, [this, pos]() {
        createShapeAtPosition("arrow", pos);
    });
    
    QAction *starAction = createMenu->addAction("星形(&S)");
    connect(starAction, &QAction::triggered, [this, pos]() {
        createShapeAtPosition("star", pos);
    });
    
    QAction *gearAction = createMenu->addAction("齿轮(&G)");
    connect(gearAction, &QAction::triggered, [this, pos]() {
        createShapeAtPosition("gear", pos);
    });
}

QPainterPath PathOperationsManager::simplifyPath(const QPainterPath &path)
{
    if (path.elementCount() < 3) {
        return path;
    }
    
    QPainterPath simplified;
    simplified.moveTo(path.elementAt(0));
    
    for (int i = 1; i < path.elementCount(); ++i) {
        const QPainterPath::Element &current = path.elementAt(i);
        
        // 简单的简化算法：移除距离很近的点
        if (i > 0) {
            const QPainterPath::Element &prev = path.elementAt(i - 1);
            qreal distance = qSqrt(qPow(current.x - prev.x, 2) + qPow(current.y - prev.y, 2));
            
            if (distance > 1.0) { // 只保留距离大于1像素的点
                if (current.type == QPainterPath::LineToElement) {
                    simplified.lineTo(current);
                } else if (current.type == QPainterPath::CurveToElement) {
                    simplified.cubicTo(current, path.elementAt(i + 1), path.elementAt(i + 2));
                    i += 2; // 跳过控制点
                }
            }
        }
    }
    
    return simplified;
}

QPainterPath PathOperationsManager::smoothPath(const QPainterPath &path)
{
    if (path.elementCount() < 3) {
        return path;
    }
    
    QPainterPath smoothed;
    
    // 添加起始点
    smoothed.moveTo(path.elementAt(0));
    
    // 使用简单的平滑算法：将直线段转换为贝塞尔曲线
    for (int i = 1; i < path.elementCount(); ++i) {
        const QPainterPath::Element &current = path.elementAt(i);
        
        if (current.type == QPainterPath::LineToElement && i < path.elementCount() - 1) {
            const QPainterPath::Element &next = path.elementAt(i + 1);
            
            // 计算控制点
            qreal cp1x = current.x * 0.75 + path.elementAt(i - 1).x * 0.25;
            qreal cp1y = current.y * 0.75 + path.elementAt(i - 1).y * 0.25;
            qreal cp2x = current.x * 0.75 + next.x * 0.25;
            qreal cp2y = current.y * 0.75 + next.y * 0.25;
            
            smoothed.cubicTo(QPointF(cp1x, cp1y), QPointF(cp2x, cp2y), QPointF(current.x, current.y));
        } else {
            smoothed.lineTo(current);
        }
    }
    
    return smoothed;
}

QPainterPath PathOperationsManager::simplifyPathStatic(const QPainterPath &path)
{
    if (path.elementCount() < 3) {
        return path;
    }
    
    QPainterPath simplified;
    simplified.moveTo(path.elementAt(0));
    
    for (int i = 1; i < path.elementCount(); ++i) {
        const QPainterPath::Element &current = path.elementAt(i);
        
        // 简单的简化算法：移除距离很近的点
        if (i > 0) {
            const QPainterPath::Element &prev = path.elementAt(i - 1);
            qreal distance = qSqrt(qPow(current.x - prev.x, 2) + qPow(current.y - prev.y, 2));
            
            if (distance > 1.0) { // 只保留距离大于1像素的点
                if (current.type == QPainterPath::LineToElement) {
                    simplified.lineTo(current);
                } else if (current.type == QPainterPath::CurveToElement) {
                    simplified.cubicTo(current, path.elementAt(i + 1), path.elementAt(i + 2));
                    i += 2; // 跳过控制点
                }
            }
        }
    }
    
    return simplified;
}

QPainterPath PathOperationsManager::smoothPathStatic(const QPainterPath &path)
{
    if (path.elementCount() < 3) {
        return path;
    }
    
    QPainterPath smoothed;
    
    // 添加起始点
    smoothed.moveTo(path.elementAt(0));
    
    // 使用简单的平滑算法：将直线段转换为贝塞尔曲线
    for (int i = 1; i < path.elementCount(); ++i) {
        const QPainterPath::Element &current = path.elementAt(i);
        
        if (current.type == QPainterPath::LineToElement && i < path.elementCount() - 1) {
            const QPainterPath::Element &next = path.elementAt(i + 1);
            
            // 计算控制点
            qreal cp1x = current.x * 0.75 + path.elementAt(i - 1).x * 0.25;
            qreal cp1y = current.y * 0.75 + path.elementAt(i - 1).y * 0.25;
            qreal cp2x = current.x * 0.75 + next.x * 0.25;
            qreal cp2y = current.y * 0.75 + next.y * 0.25;
            
            smoothed.cubicTo(QPointF(cp1x, cp1y), QPointF(cp2x, cp2y), QPointF(current.x, current.y));
        } else {
            smoothed.lineTo(current);
        }
    }
    
    return smoothed;
}

QPainterPath PathOperationsManager::convertToCurveStatic(const QPainterPath &path)
{
    if (path.elementCount() < 2) {
        return path;
    }
    
    QPainterPath curved;
    
    // 添加起始点
    curved.moveTo(path.elementAt(0));
    
    // 将所有直线段转换为贝塞尔曲线
    for (int i = 1; i < path.elementCount(); ++i) {
        const QPainterPath::Element &current = path.elementAt(i);
        
        if (current.type == QPainterPath::LineToElement) {
            const QPainterPath::Element &prev = path.elementAt(i - 1);
            
            // 创建平滑的贝塞尔曲线
            QPointF start(prev.x, prev.y);
            QPointF end(current.x, current.y);
            
            // 计算控制点，使曲线更平滑
            QPointF cp1 = start + (end - start) * 0.25;
            QPointF cp2 = start + (end - start) * 0.75;
            
            curved.cubicTo(cp1, cp2, end);
        } else if (current.type == QPainterPath::CurveToElement) {
            // 保留原有的曲线段
            if (i + 2 < path.elementCount()) {
                curved.cubicTo(path.elementAt(i), path.elementAt(i + 1), path.elementAt(i + 2));
                i += 2; // 跳过控制点
            }
        } else {
            curved.lineTo(current);
        }
    }
    
    return curved;
}

QPainterPath PathOperationsManager::offsetPathStatic(const QPainterPath &path, qreal offset)
{
    if (path.isEmpty() || offset == 0.0) {
        return path;
    }
    
    // 简单的偏移实现：创建外轮廓
    QPainterPathStroker stroker;
    stroker.setWidth(offset * 2);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    
    QPainterPath offsetPath = stroker.createStroke(path);
    
    // 如果需要内偏移，可以使用裁剪
    if (offset < 0) {
        QPainterPath clipPath = path;
        QRectF bounds = path.boundingRect();
        clipPath.addRect(bounds.adjusted(offset, offset, -offset, -offset));
        offsetPath = offsetPath.intersected(clipPath);
    }
    
    return offsetPath;
}

QPainterPath PathOperationsManager::clipPathStatic(const QPainterPath &path)
{
    if (path.isEmpty()) {
        return path;
    }
    
    // 使用路径的边界框进行裁剪，稍微缩小边界
    QRectF bounds = path.boundingRect();
    QRectF clipRect = bounds.adjusted(5, 5, -5, -5); // 缩小5像素
    
    if (clipRect.isEmpty()) {
        return path;
    }
    
    // 创建裁剪路径
    QPainterPath clipPath;
    clipPath.addRect(clipRect);
    
    // 返回裁剪后的路径
    return path.intersected(clipPath);
}

QPainterPath PathOperationsManager::createStarPath(const QPointF &center, qreal radius, int points)
{
    QPainterPath starPath;
    
    if (points < 3) return starPath;
    
    qreal outerRadius = radius;
    qreal innerRadius = radius * 0.4; // 内半径是外半径的40%
    qreal angleStep = M_PI * 2 / (points * 2); // 每个角的角度步长
    
    starPath.moveTo(center.x() + outerRadius * cos(0), center.y() + outerRadius * sin(0));
    
    for (int i = 1; i < points * 2; ++i) {
        qreal currentRadius = (i % 2 == 0) ? outerRadius : innerRadius;
        qreal angle = i * angleStep;
        starPath.lineTo(center.x() + currentRadius * cos(angle), 
                       center.y() + currentRadius * sin(angle));
    }
    
    starPath.closeSubpath();
    return starPath;
}

QPainterPath PathOperationsManager::createArrowPath(const QPointF &center, qreal size)
{
    QPainterPath arrowPath;
    
    // 更美观的箭头形状
    qreal halfSize = size / 2;
    qreal shaftWidth = halfSize * 0.3; // 箭杆宽度
    
    // 箭头顶点
    arrowPath.moveTo(center.x(), center.y() - halfSize);
    // 箭头左翼
    arrowPath.lineTo(center.x() - halfSize * 0.7, center.y() + halfSize * 0.2);
    // 箭杆左上角
    arrowPath.lineTo(center.x() - shaftWidth, center.y() + halfSize * 0.2);
    // 箭杆左下角
    arrowPath.lineTo(center.x() - shaftWidth, center.y() + halfSize * 0.5);
    // 箭头底部左角
    arrowPath.lineTo(center.x() - halfSize * 0.4, center.y() + halfSize * 0.5);
    // 箭头底部中心
    arrowPath.lineTo(center.x(), center.y() + halfSize * 0.7);
    // 箭头底部右角
    arrowPath.lineTo(center.x() + halfSize * 0.4, center.y() + halfSize * 0.5);
    // 箭杆右下角
    arrowPath.lineTo(center.x() + shaftWidth, center.y() + halfSize * 0.5);
    // 箭杆右上角
    arrowPath.lineTo(center.x() + shaftWidth, center.y() + halfSize * 0.2);
    // 箭头右翼
    arrowPath.lineTo(center.x() + halfSize * 0.7, center.y() + halfSize * 0.2);
    
    arrowPath.closeSubpath();
    return arrowPath;
}

QPainterPath PathOperationsManager::createGearPath(const QPointF &center, qreal radius, int teeth)
{
    QPainterPath gearPath;
    
    if (teeth < 6) return gearPath;
    
    qreal outerRadius = radius;
    qreal innerRadius = radius * 0.8; // 齿根半径
    qreal holeRadius = radius * 0.3; // 中心孔半径
    qreal toothWidth = M_PI * 2 / (teeth * 2); // 齿宽角度
    
    // 绘制外圈齿轮
    for (int i = 0; i < teeth * 2; ++i) {
        qreal angle = i * toothWidth;
        qreal currentRadius = (i % 2 == 0) ? outerRadius : innerRadius;
        qreal x = center.x() + currentRadius * cos(angle);
        qreal y = center.y() + currentRadius * sin(angle);
        
        if (i == 0) {
            gearPath.moveTo(x, y);
        } else {
            gearPath.lineTo(x, y);
        }
    }
    
    gearPath.closeSubpath();
    
    // 添加中心孔
    gearPath.addEllipse(center, holeRadius, holeRadius);
    
    return gearPath;
}

void PathOperationsManager::createShapeAtPosition(const QString &shapeType, const QPointF &pos)
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    // 创建撤销命令
    class CreateShapeCommand : public QUndoCommand {
    public:
        CreateShapeCommand(DrawingScene *scene, const QString &shapeType, const QPointF &pos, 
                        QUndoCommand *parent = nullptr)
            : QUndoCommand(QString("创建%1").arg(shapeType), parent), m_scene(scene), m_shapeType(shapeType), m_pos(pos), m_createdPath(nullptr)
        {
        }

        ~CreateShapeCommand() {
            // 析构时检查对象是否还在场景中，如果不在就删除
            // 增加更安全的检查，避免访问已释放的scene
            if (m_createdPath) {
                // 先检查scene指针是否有效
                bool sceneValid = false;
                if (m_scene) {
                    try {
                        sceneValid = true;
                    } catch (...) {
                        sceneValid = false;
                    }
                }
                
                // 只有在scene无效或者对象不在scene中时才删除
                if (!sceneValid || (m_createdPath->scene() != m_scene)) {
                    try {
                        delete m_createdPath;
                    } catch (...) {
                        // 如果删除失败，忽略错误，避免程序崩溃
                    }
                }
                m_createdPath = nullptr;
            }
        }

        void undo() override {
            if (!m_scene || !m_createdPath) return;
            
            // 只从场景中移除，不删除对象
            m_scene->removeItem(m_createdPath);
            
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            if (!m_scene) return;
            
            // 如果对象已存在，只需重新添加到场景
            if (m_createdPath) {
                m_scene->addItem(m_createdPath);
                m_createdPath->setSelected(true);
                if (m_scene) m_scene->setModified(true);
                return;
            }
            
            // 创建形状路径
            QPainterPath shape;
            
            if (m_shapeType == "arrow") {
                // 创建箭头形状
                shape.moveTo(m_pos.x() - 50, m_pos.y());
                shape.lineTo(m_pos.x() + 20, m_pos.y());
                shape.lineTo(m_pos.x() + 10, m_pos.y() - 10);
                shape.lineTo(m_pos.x() + 20, m_pos.y());
                shape.lineTo(m_pos.x() + 10, m_pos.y() + 10);
                shape.lineTo(m_pos.x() + 20, m_pos.y());
                shape.lineTo(m_pos.x() - 50, m_pos.y());
            } else if (m_shapeType == "star") {
                // 创建五角星
                int points = 5;
                qreal outerRadius = 50;
                qreal innerRadius = 20;
                
                for (int i = 0; i < points * 2; ++i) {
                    qreal angle = (i * M_PI) / points - M_PI / 2;
                    qreal radius = (i % 2 == 0) ? outerRadius : innerRadius;
                    qreal x = m_pos.x() + radius * cos(angle);
                    qreal y = m_pos.y() + radius * sin(angle);
                    
                    if (i == 0) {
                        shape.moveTo(x, y);
                    } else {
                        shape.lineTo(x, y);
                    }
                }
                shape.closeSubpath();
            } else if (m_shapeType == "gear") {
                // 创建齿轮形状
                int teeth = 8;
                qreal outerRadius = 50;
                qreal innerRadius = 40;
                qreal toothHeight = 10;
                
                for (int i = 0; i < teeth * 2; ++i) {
                    qreal angle = (i * M_PI) / teeth;
                    qreal radius = (i % 2 == 0) ? outerRadius + toothHeight : outerRadius;
                    qreal x = m_pos.x() + radius * cos(angle);
                    qreal y = m_pos.y() + radius * sin(angle);
                    
                    if (i == 0) {
                        shape.moveTo(x, y);
                    } else {
                        shape.lineTo(x, y);
                    }
                }
                shape.closeSubpath();
                
                // 添加中心孔
                shape.addEllipse(m_pos.x() - innerRadius/2, m_pos.y() - innerRadius/2, innerRadius, innerRadius);
            }
            
            if (shape.isEmpty()) return;
            
            // 创建新的路径对象
            m_createdPath = new DrawingPath();
            
            // 调整路径为相对于原点的路径
            QRectF bounds = shape.boundingRect();
            QTransform offsetTransform;
            offsetTransform.translate(-bounds.left(), -bounds.top());
            QPainterPath adjustedPath = offsetTransform.map(shape);
            
            m_createdPath->setPath(adjustedPath);
            m_createdPath->setPos(bounds.topLeft());
            
            // 设置默认样式
            m_createdPath->setFillBrush(QBrush(Qt::blue));
            m_createdPath->setStrokePen(QPen(Qt::black, 2));
            
            // 添加到场景
            m_scene->addItem(m_createdPath);
            m_createdPath->setSelected(true);
            
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        QString m_shapeType;
        QPointF m_pos;
        DrawingPath *m_createdPath;
    };

    // 使用CommandManager执行创建操作
    if (m_commandManager) {
        m_commandManager->pushCommand(new CreateShapeCommand(m_scene, shapeType, pos));
    } else {
        // 如果没有CommandManager，使用原来的直接执行方式
        qDebug() << "Warning: No CommandManager available, shape creation cannot be undone";
        emit statusMessageChanged("创建的图形无法撤销（命令管理器未初始化）");
    }
    
    emit pathOperationCompleted(QString("创建%1").arg(shapeType));
    emit statusMessageChanged(QString("已创建%1").arg(shapeType));
}