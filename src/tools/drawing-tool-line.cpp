#include "../tools/drawing-tool-line.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include <QGraphicsScene>
#include <QMouseEvent>

DrawingToolLine::DrawingToolLine(QObject *parent)
    : ToolBase(parent)
    , m_currentLine(nullptr)
    , m_drawing(false)
{
}

void DrawingToolLine::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    m_currentLine = nullptr;
    m_drawing = false;
}

void DrawingToolLine::deactivate()
{
    if (m_currentLine) {
        if (m_scene) {
            m_scene->removeItem(m_currentLine);
        }
        delete m_currentLine;
        m_currentLine = nullptr;
    }
    ToolBase::deactivate();
}

bool DrawingToolLine::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        m_startPoint = scenePos;
        m_drawing = true;
        
        // 创建新的直线
        m_currentLine = new DrawingLine(QLineF(m_startPoint, m_startPoint));
        m_currentLine->setPos(0, 0);
        m_currentLine->setFillBrush(Qt::NoBrush);
        m_currentLine->setStrokePen(QPen(Qt::black));
        m_scene->addItem(m_currentLine);
        
        return true;
    }
    
    return false;
}

bool DrawingToolLine::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentLine) {
        // 更新直线的终点
        m_currentLine->setLine(QLineF(m_startPoint, scenePos));
        return true;
    }
    
    return false;
}

bool DrawingToolLine::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        
        // 完成直线绘制
        if (m_currentLine) {
            m_currentLine->setLine(QLineF(m_startPoint, scenePos));
            
            // 添加到撤销栈
            if (m_scene) {
                m_scene->setModified(true);
                
                // 使用DrawingScene中的AddItemCommand
                class AddItemCommand : public QUndoCommand
                {
                public:
                    AddItemCommand(DrawingScene *scene, QGraphicsItem *item, QUndoCommand *parent = nullptr)
                        : QUndoCommand("添加直线", parent), m_scene(scene), m_item(item) {}
                    
                    void undo() override {
                        if (m_scene && m_item) {
                            m_scene->removeItem(m_item);
                        }
                    }
                    
                    void redo() override {
                        if (m_scene && m_item) {
                            m_scene->addItem(m_item);
                        }
                    }
                    
                    ~AddItemCommand() {
                        // 在撤销命令被销毁时，如果item不在场景中，则删除它
                        if (m_item && m_scene && !m_scene->items().contains(m_item)) {
                            delete m_item;
                        }
                    }
                    
                private:
                    DrawingScene *m_scene;
                    QGraphicsItem *m_item;
                };
                
                // 创建并推送撤销命令
                AddItemCommand *command = new AddItemCommand(m_scene, m_currentLine);
                m_scene->undoStack()->push(command);
            }
            
            m_currentLine = nullptr; // 不删除，让场景管理
        }
        
        return true;
    }
    
    return false;
}

// 实现基类的纯虚函数
DrawingShape* DrawingToolLine::createShape(const QPointF &pos)
{
    m_startPoint = pos;
    m_currentLine = new DrawingLine();
    m_currentLine->setLine(QLineF(pos, pos));
    m_currentLine->setPos(0, 0);
    m_currentLine->setFillBrush(Qt::NoBrush);
    m_currentLine->setStrokePen(QPen(Qt::black));
    return m_currentLine;
}

void DrawingToolLine::updateShape(const QPointF &startPos, const QPointF &currentPos)
{
    if (m_currentLine) {
        m_currentLine->setLine(QLineF(startPos, currentPos));
    }
}

