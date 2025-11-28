#include "selection-manager.h"
#include "mainwindow.h"
#include "drawingscene.h"
#include "command-manager.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-group.h"
#include "../core/drawing-shape.h"
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QRectF>
#include <QByteArray>
#include <QDataStream>
#include <algorithm>

SelectionManager::SelectionManager(MainWindow *parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_scene(nullptr)
    , m_commandManager(nullptr)
{
}

SelectionManager::~SelectionManager()
{
}

void SelectionManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
}

DrawingScene *SelectionManager::scene() const
{
    return m_scene;
}

void SelectionManager::setCommandManager(CommandManager *commandManager)
{
    m_commandManager = commandManager;
}

CommandManager *SelectionManager::commandManager() const
{
    return m_commandManager;
}

QList<DrawingShape*> SelectionManager::selectedShapes() const
{
    QList<DrawingShape*> shapes;
    if (!m_scene) {
        qDebug() << "selectedShapes: scene is null";
        return shapes;
    }
    
    QList<QGraphicsItem*> items = m_scene->selectedItems();
    qDebug() << "selectedShapes: total selected items:" << items.count();
    
    for (QGraphicsItem *item : items) {
        DrawingShape *shape = dynamic_cast<DrawingShape*>(item);
        if (shape) {
            shapes.append(shape);
            qDebug() << "selectedShapes: found DrawingShape";
        } else {
            qDebug() << "selectedShapes: item is not DrawingShape";
        }
    }
    
    qDebug() << "selectedShapes: returning" << shapes.count() << "shapes";
    return shapes;
}

bool SelectionManager::hasSelection() const
{
    return selectionCount() > 0;
}

int SelectionManager::selectionCount() const
{
    return selectedShapes().count();
}

void SelectionManager::selectAll()
{
    if (!m_scene) return;
    
    // 使用QGraphicsScene的selectAll方法
    QPainterPath selectionArea;
    selectionArea.addRect(m_scene->sceneRect());
    m_scene->setSelectionArea(selectionArea);
    emit selectionChanged(selectionCount());
    emit statusMessageChanged("已选择所有对象");
}

void SelectionManager::deselectAll()
{
    if (!m_scene) return;
    
    m_scene->clearSelection();
    emit selectionChanged(0);
    emit statusMessageChanged("已取消选择");
}

void SelectionManager::deleteSelected()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        emit statusMessageChanged("请先选择要删除的对象");
        return;
    }

    if (m_commandManager) {
        // 使用统一的DeleteCommand
        m_commandManager->pushCommand(new DeleteCommand(m_commandManager, shapes));
    } else {
        emit statusMessageChanged("命令管理器未初始化");
    }
}

void SelectionManager::copySelected()
{
    qDebug() << "CopySelected called";
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        qDebug() << "No shapes selected for copy";
        emit statusMessageChanged("请先选择要复制的对象");
        return;
    }

    // 序列化所有选中的对象到剪贴板
    QByteArray clipboardData;
    QDataStream stream(&clipboardData, QIODevice::WriteOnly);
    
    // 写入对象数量
    stream << shapes.count();
    
    // 序列化每个对象
    for (DrawingShape *shape : shapes) {
        QByteArray shapeData = shape->serialize();
        stream << shapeData;
    }
    
    // 设置到剪贴板
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    mimeData->setData("application/x-vectorqt-shapes", clipboardData);
    clipboard->setMimeData(mimeData);
    
    emit statusMessageChanged(QString("已复制 %1 个对象到剪贴板").arg(shapes.count()));
}

void SelectionManager::paste()
{
    qDebug() << "Paste called";
    if (!m_scene) {
        qDebug() << "Scene not initialized for paste";
        emit statusMessageChanged("场景未初始化");
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    
    qDebug() << "Paste: checking clipboard formats";
    QStringList formats = mimeData->formats();
    qDebug() << "Paste: available formats:" << formats;
    
    if (!mimeData->hasFormat("application/x-vectorqt-shapes")) {
        qDebug() << "Paste: no vectorqt format in clipboard";
        emit statusMessageChanged("剪贴板中没有可粘贴的对象");
        return;
    }
    
    qDebug() << "Paste: found vectorqt format in clipboard";

    QByteArray clipboardData = mimeData->data("application/x-vectorqt-shapes");
    QDataStream stream(&clipboardData, QIODevice::ReadOnly);
    
    int shapeCount;
    stream >> shapeCount;
    
    if (shapeCount <= 0) {
        emit statusMessageChanged("剪贴板数据无效");
        return;
    }

    // 收集所有形状数据
    QList<QByteArray> shapeDataList;
    
    // 反序列化每个对象的数据
    for (int i = 0; i < shapeCount; ++i) {
        QByteArray shapeData;
        stream >> shapeData;
        shapeDataList.append(shapeData);
    }
    
    // 使用CommandManager执行粘贴操作以支持撤销
    if (m_commandManager) {
        qDebug() << "Paste: pushing PasteCommand with" << shapeDataList.count() << "shapes";
        m_commandManager->pushCommand(new PasteCommand(m_commandManager, shapeDataList, QPointF(20, 20)));
        qDebug() << "Paste: PasteCommand pushed successfully";
    } else {
        qDebug() << "CommandManager not initialized for paste";
        emit statusMessageChanged("命令管理器未初始化");
    }
}

void SelectionManager::duplicate()
{
    qDebug() << "Duplicate called";
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        qDebug() << "No shapes selected for duplicate";
        emit statusMessageChanged("请先选择要复制的对象");
        return;
    }

    if (m_commandManager) {
        // 使用统一的DuplicateCommand，偏移量为(10, 10)
        m_commandManager->pushCommand(new DuplicateCommand(m_commandManager, shapes, QPointF(10, 10)));
    } else {
        emit statusMessageChanged("命令管理器未初始化");
    }
}

void SelectionManager::alignLeft()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行对齐");
        return;
    }

    qreal leftEdge = getLeftmostEdge();
    
    // 创建撤销命令
    class AlignCommand : public QUndoCommand {
    public:
        AlignCommand(DrawingScene *scene, const QList<DrawingShape*>& shapes, qreal targetPosition, 
                    void (DrawingShape::*setter)(qreal), QUndoCommand *parent = nullptr)
            : QUndoCommand("左对齐", parent), m_scene(scene), m_shapes(shapes), 
              m_targetPosition(targetPosition), m_setter(setter)
        {
            // 保存原始位置
            for (DrawingShape *shape : shapes) {
                m_originalPositions[shape] = shape->pos();
            }
        }

        void undo() override {
            for (DrawingShape *shape : m_shapes) {
                if (m_originalPositions.contains(shape)) {
                    shape->setPos(m_originalPositions[shape]);
                }
            }
            if (m_scene) m_scene->setModified(true);
        }

        void redo() override {
            for (DrawingShape *shape : m_shapes) {
                QPointF pos = shape->pos();
                pos.setX(m_targetPosition);
                shape->setPos(pos);
            }
            if (m_scene) m_scene->setModified(true);
        }

    private:
        DrawingScene *m_scene;
        QList<DrawingShape*> m_shapes;
        qreal m_targetPosition;
        void (DrawingShape::*m_setter)(qreal);
        QMap<DrawingShape*, QPointF> m_originalPositions;
    };

    if (CommandManager::hasInstance()) {
        CommandManager::instance()->pushCommand(new AlignCommand(m_scene, shapes, leftEdge, nullptr));
    }
    emit alignmentCompleted("左对齐");
    emit statusMessageChanged("已左对齐选中的对象");
}

void SelectionManager::alignCenter()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行对齐");
        return;
    }

    qreal center = getHorizontalCenter();
    
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        QPointF pos = shape->pos();
        pos.setX(center - bounds.width() / 2);
        shape->setPos(pos);
    }
    
    emit alignmentCompleted("水平居中对齐");
    emit statusMessageChanged("已水平居中对齐选中的对象");
}

void SelectionManager::alignRight()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行对齐");
        return;
    }

    qreal rightEdge = getRightmostEdge();
    
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        QPointF pos = shape->pos();
        pos.setX(rightEdge - bounds.width());
        shape->setPos(pos);
    }
    
    emit alignmentCompleted("右对齐");
    emit statusMessageChanged("已右对齐选中的对象");
}

void SelectionManager::alignTop()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行对齐");
        return;
    }

    qreal topEdge = getTopmostEdge();
    
    for (DrawingShape *shape : shapes) {
        QPointF pos = shape->pos();
        pos.setY(topEdge);
        shape->setPos(pos);
    }
    
    emit alignmentCompleted("顶对齐");
    emit statusMessageChanged("已顶对齐选中的对象");
}

void SelectionManager::alignMiddle()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行对齐");
        return;
    }

    qreal middle = getVerticalCenter();
    
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        QPointF pos = shape->pos();
        pos.setY(middle - bounds.height() / 2);
        shape->setPos(pos);
    }
    
    emit alignmentCompleted("垂直居中对齐");
    emit statusMessageChanged("已垂直居中对齐选中的对象");
}

void SelectionManager::alignBottom()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行对齐");
        return;
    }

    qreal bottomEdge = getBottommostEdge();
    
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        QPointF pos = shape->pos();
        pos.setY(bottomEdge - bounds.height());
        shape->setPos(pos);
    }
    
    emit alignmentCompleted("底对齐");
    emit statusMessageChanged("已底对齐选中的对象");
}

void SelectionManager::distributeHorizontal()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 3) {
        emit statusMessageChanged("请至少选择3个对象进行分布");
        return;
    }

    QList<qreal> positions = getHorizontalPositions();
    std::sort(positions.begin(), positions.end());
    
    qreal start = positions.first();
    qreal end = positions.last();
    distributePositions(positions, start, end);
    
    // 应用分布
    for (int i = 0; i < shapes.count(); ++i) {
        QPointF pos = shapes[i]->pos();
        pos.setX(positions[i]);
        shapes[i]->setPos(pos);
    }
    
    emit distributionCompleted("水平分布");
    emit statusMessageChanged("已水平分布选中的对象");
}

void SelectionManager::distributeVertical()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 3) {
        emit statusMessageChanged("请至少选择3个对象进行分布");
        return;
    }

    QList<qreal> positions = getVerticalPositions();
    std::sort(positions.begin(), positions.end());
    
    qreal start = positions.first();
    qreal end = positions.last();
    distributePositions(positions, start, end);
    
    // 应用分布
    for (int i = 0; i < shapes.count(); ++i) {
        QPointF pos = shapes[i]->pos();
        pos.setY(positions[i]);
        shapes[i]->setPos(pos);
    }
    
    emit distributionCompleted("垂直分布");
    emit statusMessageChanged("已垂直分布选中的对象");
}

void SelectionManager::bringToFront()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        emit statusMessageChanged("请先选择要调整层级的对象");
        return;
    }

    for (DrawingShape *shape : shapes) {
        shape->setZValue(shape->zValue() + 1);
    }
    
    emit statusMessageChanged("已将对象置于顶层");
}

void SelectionManager::sendToBack()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        emit statusMessageChanged("请先选择要调整层级的对象");
        return;
    }

    for (DrawingShape *shape : shapes) {
        shape->setZValue(shape->zValue() - 1);
    }
    
    emit statusMessageChanged("已将对象置于底层");
}

void SelectionManager::bringForward()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        emit statusMessageChanged("请先选择要调整层级的对象");
        return;
    }

    for (DrawingShape *shape : shapes) {
        shape->setZValue(shape->zValue() + 0.1);
    }
    
    emit statusMessageChanged("已将对象上移一层");
}

void SelectionManager::sendBackward()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        emit statusMessageChanged("请先选择要调整层级的对象");
        return;
    }

    for (DrawingShape *shape : shapes) {
        shape->setZValue(shape->zValue() - 0.1);
    }
    
    emit statusMessageChanged("已将对象下移一层");
}

void SelectionManager::groupSelected()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.count() < 2) {
        emit statusMessageChanged("请至少选择2个对象进行分组");
        return;
    }

    // TODO: 实现分组功能
    emit statusMessageChanged("分组功能尚未实现");
}

void SelectionManager::ungroupSelected()
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) {
        emit statusMessageChanged("请先选择要解除分组的对象");
        return;
    }

    // TODO: 实现取消分组功能
    emit statusMessageChanged("取消分组功能尚未实现");
}

// 私有辅助函数实现
QRectF SelectionManager::getSelectionBounds() const
{
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.isEmpty()) return QRectF();
    
    QRectF bounds = shapes.first()->boundingRect();
    bounds.translate(shapes.first()->pos());
    
    for (DrawingShape *shape : shapes) {
        QRectF shapeBounds = shape->boundingRect();
        shapeBounds.translate(shape->pos());
        bounds = bounds.united(shapeBounds);
    }
    
    return bounds;
}

qreal SelectionManager::getLeftmostEdge() const
{
    return getSelectionBounds().left();
}

qreal SelectionManager::getRightmostEdge() const
{
    return getSelectionBounds().right();
}

qreal SelectionManager::getTopmostEdge() const
{
    return getSelectionBounds().top();
}

qreal SelectionManager::getBottommostEdge() const
{
    return getSelectionBounds().bottom();
}

qreal SelectionManager::getHorizontalCenter() const
{
    QRectF bounds = getSelectionBounds();
    return bounds.left() + bounds.width() / 2;
}

qreal SelectionManager::getVerticalCenter() const
{
    QRectF bounds = getSelectionBounds();
    return bounds.top() + bounds.height() / 2;
}

QList<qreal> SelectionManager::getHorizontalPositions() const
{
    QList<DrawingShape*> shapes = selectedShapes();
    QList<qreal> positions;
    
    for (DrawingShape *shape : shapes) {
        positions.append(shape->pos().x());
    }
    
    return positions;
}

QList<qreal> SelectionManager::getVerticalPositions() const
{
    QList<DrawingShape*> shapes = selectedShapes();
    QList<qreal> positions;
    
    for (DrawingShape *shape : shapes) {
        positions.append(shape->pos().y());
    }
    
    return positions;
}

void SelectionManager::distributePositions(QList<qreal> &positions, qreal start, qreal end)
{
    if (positions.count() < 2) return;
    
    qreal interval = (end - start) / (positions.count() - 1);
    
    for (int i = 0; i < positions.count(); ++i) {
        positions[i] = start + i * interval;
    }
}

void SelectionManager::sameWidth()
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.size() < 2) {
        emit statusMessageChanged("需要选择至少2个对象");
        return;
    }
    
    // 计算平均宽度
    qreal totalWidth = 0;
    for (DrawingShape *shape : shapes) {
        totalWidth += shape->boundingRect().width();
    }
    qreal averageWidth = totalWidth / shapes.size();
    
    // 设置所有对象为相同宽度
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        QRectF newBounds(bounds.left(), bounds.top(), averageWidth, bounds.height());
        
        // 根据形状类型设置新尺寸
        if (shape->shapeType() == DrawingShape::Rectangle) {
            DrawingRectangle *rect = qgraphicsitem_cast<DrawingRectangle*>(shape);
            if (rect) {
                rect->setRectangle(newBounds);
            }
        }
        // 可以添加其他形状类型的支持
    }
    
    emit statusMessageChanged("统一宽度完成");
    emit alignmentCompleted("统一宽度");
}

void SelectionManager::sameHeight()
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.size() < 2) {
        emit statusMessageChanged("需要选择至少2个对象");
        return;
    }
    
    // 计算平均高度
    qreal totalHeight = 0;
    for (DrawingShape *shape : shapes) {
        totalHeight += shape->boundingRect().height();
    }
    qreal averageHeight = totalHeight / shapes.size();
    
    // 设置所有对象为相同高度
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        QRectF newBounds(bounds.left(), bounds.top(), bounds.width(), averageHeight);
        
        // 根据形状类型设置新尺寸
        if (shape->shapeType() == DrawingShape::Rectangle) {
            DrawingRectangle *rect = qgraphicsitem_cast<DrawingRectangle*>(shape);
            if (rect) {
                rect->setRectangle(newBounds);
            }
        }
        // 可以添加其他形状类型的支持
    }
    
    emit statusMessageChanged("统一高度完成");
    emit alignmentCompleted("统一高度");
}

void SelectionManager::sameSize()
{
    if (!m_scene) {
        emit statusMessageChanged("场景未初始化");
        return;
    }
    
    QList<DrawingShape*> shapes = selectedShapes();
    if (shapes.size() < 2) {
        emit statusMessageChanged("需要选择至少2个对象");
        return;
    }
    
    // 计算平均尺寸
    qreal totalWidth = 0, totalHeight = 0;
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        totalWidth += bounds.width();
        totalHeight += bounds.height();
    }
    qreal averageWidth = totalWidth / shapes.size();
    qreal averageHeight = totalHeight / shapes.size();
    
    // 设置所有对象为相同尺寸
    for (DrawingShape *shape : shapes) {
        QRectF bounds = shape->boundingRect();
        QRectF newBounds(bounds.left(), bounds.top(), averageWidth, averageHeight);
        
        // 根据形状类型设置新尺寸
        if (shape->shapeType() == DrawingShape::Rectangle) {
            DrawingRectangle *rect = qgraphicsitem_cast<DrawingRectangle*>(shape);
            if (rect) {
                rect->setRectangle(newBounds);
            }
        }
        // 可以添加其他形状类型的支持
    }
    
    emit statusMessageChanged("统一尺寸完成");
    emit alignmentCompleted("统一尺寸");
}