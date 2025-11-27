#include <QGraphicsScene>
#include <QMouseEvent>
#include "../tools/drawing-tool-polyline.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"

DrawingToolPolyline::DrawingToolPolyline(QObject *parent)
    : ToolBase(parent)
    , m_currentPolyline(nullptr)
    , m_drawing(false)
{
}

void DrawingToolPolyline::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    m_currentPolyline = nullptr;
    m_drawing = false;
}

void DrawingToolPolyline::deactivate()
{
    if (m_currentPolyline) {
        if (m_scene) {
            m_scene->removeItem(m_currentPolyline);
        }
        delete m_currentPolyline;
        m_currentPolyline = nullptr;
    }
    ToolBase::deactivate();
}

bool DrawingToolPolyline::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        if (!m_drawing) {
            // 开始新的折线
            m_drawing = true;
            m_currentPolyline = new DrawingPolyline();
            m_currentPolyline->setPos(0, 0);
            m_currentPolyline->setFillBrush(Qt::NoBrush);
            m_currentPolyline->setStrokePen(QPen(Qt::black));
            
            // 添加第一个点和预览点
            m_currentPolyline->addPoint(scenePos);
            m_currentPolyline->addPoint(scenePos); // 第二个点用于预览
            m_scene->addItem(m_currentPolyline);
        } else {
            // 确认上一个点的位置，然后添加新点
            if (m_currentPolyline && m_currentPolyline->pointCount() > 1) {
                // 先确认上一个点的位置（此时已经被mouseMove更新到正确位置）
                // 然后添加新的预览点
                m_currentPolyline->addPoint(scenePos);
            }
        }
        
        return true;
    }
    
    return false;
}

bool DrawingToolPolyline::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentPolyline) {
        // 更新最后一个预览点的位置
        if (m_currentPolyline->pointCount() > 1) {
            m_currentPolyline->setPoint(m_currentPolyline->pointCount() - 1, scenePos);
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolyline::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        // 确保最后一个点在正确位置
        if (m_currentPolyline && m_currentPolyline->pointCount() > 0) {
            m_currentPolyline->setPoint(m_currentPolyline->pointCount() - 1, scenePos);
        }
        return true;
    }
    
    return false;
}

bool DrawingToolPolyline::mouseDoubleClickEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        // 双击完成折线绘制
        m_drawing = false;
        if (m_currentPolyline && m_currentPolyline->pointCount() > 1) {
            // 移除最后一个预览点
            m_currentPolyline->removePoint(m_currentPolyline->pointCount() - 1);
            
            // 添加到撤销栈
            m_scene->setModified(true);
            
            // 使用DrawingScene中的AddItemCommand
            class AddItemCommand : public QUndoCommand
            {
            public:
                AddItemCommand(DrawingScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
                    : QUndoCommand("添加折线", parent), m_scene(scene), m_item(item) {}
                
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
            AddItemCommand *command = new AddItemCommand(m_scene, m_currentPolyline);
            m_scene->executeCommand(command);
            
            m_currentPolyline = nullptr;
        }
        return true;
    }
    
    return false;
}

