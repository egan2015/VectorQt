#include <QGraphicsScene>
#include <QMouseEvent>
#include "../tools/drawing-tool-polygon.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"

DrawingToolPolygon::DrawingToolPolygon(QObject *parent)
    : ToolBase(parent)
    , m_currentPolygon(nullptr)
    , m_drawing(false)
{
}

void DrawingToolPolygon::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    m_currentPolygon = nullptr;
    m_drawing = false;
}

void DrawingToolPolygon::deactivate()
{
    if (m_currentPolygon) {
        if (m_scene) {
            m_scene->removeItem(m_currentPolygon);
        }
        delete m_currentPolygon;
        m_currentPolygon = nullptr;
    }
    ToolBase::deactivate();
}

bool DrawingToolPolygon::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        if (!m_drawing) {
            // 开始新的多边形
            m_drawing = true;
            m_currentPolygon = new DrawingPolygon();
            m_currentPolygon->setPos(0, 0);
            m_currentPolygon->setFillBrush(Qt::lightGray);
            m_currentPolygon->setStrokePen(QPen(Qt::black));
            
            // 添加第一个点和预览点
            m_currentPolygon->addPoint(scenePos);
            m_currentPolygon->addPoint(scenePos); // 第二个点用于预览
            m_scene->addItem(m_currentPolygon);
        } else {
            // 确认上一个点的位置，然后添加新点
            if (m_currentPolygon && m_currentPolygon->pointCount() > 0) {
                // 先确认上一个点的位置（此时已经被mouseMove更新到正确位置）
                // 然后添加新的预览点
                m_currentPolygon->addPoint(scenePos);
            }
        }
        
        return true;
    }
    
    return false;
}

bool DrawingToolPolygon::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentPolygon) {
        // 更新最后一个预览点的位置
        if (m_currentPolygon->pointCount() > 1) {
            m_currentPolygon->setPoint(m_currentPolygon->pointCount()-1, scenePos);
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolygon::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    // 不需要特殊处理，mousePressEvent已经处理了点的添加
    return false;
}

bool DrawingToolPolygon::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        // 双击完成多边形绘制
        m_drawing = false;
        
        if (m_currentPolygon) {
            // 移除最后一个预览点
            if (m_currentPolygon->pointCount() > 2) {
                m_currentPolygon->removePoint(m_currentPolygon->pointCount() - 1);
            }
            
            // 添加到撤销栈
            m_scene->setModified(true);
            
            // 使用DrawingScene中的AddItemCommand
            class AddItemCommand : public QUndoCommand
            {
            public:
                AddItemCommand(DrawingScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
                    : QUndoCommand("添加多边形", parent), m_scene(scene), m_item(item) {}
                
                void undo() override {
                    m_scene->removeItem(m_item);
                    m_item->setVisible(false);
                }
                
                void redo() override {
                    m_scene->addItem(m_item);
                    m_item->setVisible(true);
                    
                    // 自动选中新创建的图形
                    m_item->setSelected(true);
                    
                    // 清除其他选中项
                    for (QGraphicsItem *item : m_scene->selectedItems()) {
                        if (item != m_item) {
                            item->setSelected(false);
                        }
                    }
                }
                
            private:
                DrawingScene *m_scene;
                QGraphicsItem *m_item;
            };
            
            // 创建并推送撤销命令
            AddItemCommand *command = new AddItemCommand(m_scene, m_currentPolygon);
            m_scene->undoStack()->push(command);
            
            m_currentPolygon = nullptr;
        }
        return true;
    }
    
    return false;
}

