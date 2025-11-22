#include "../core/drawing-document.h"
#include "../core/drawing-shape.h"
#include <QDebug>

DrawingDocument::DrawingDocument(QObject *parent)
    : QObject(parent)
{
}

DrawingDocument::~DrawingDocument()
{
    clear();
}

void DrawingDocument::addItem(DrawingShape *item)
{
    if (!item) {
        return;
    }
    
    m_items.push_back(item);
    item->setDocument(this);
}
 

void DrawingDocument::removeItem(DrawingShape *item)
{
    if (!item) {
        return;
    }
    
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (*it == item) {
            // 然后清除文档引用和从列表中移除
            item->setDocument(nullptr);
            m_items.erase(it);
            break;
        }
    }
}

std::vector<DrawingShape*> DrawingDocument::items() const
{
    return m_items;
}

void DrawingDocument::clear()
{
    // 先发送所有移除信号
    for (DrawingShape *item : m_items) {
    }
    
    // 然后清除文档引用和列表
    for (DrawingShape *item : m_items) {
        if (item) {
            item->setDocument(nullptr);
        }
    }
    m_items.clear();
    
}

QRectF DrawingDocument::bounds() const
{
    QRectF result;
    for (DrawingShape *item : m_items) {
        if (item) {
            if (result.isEmpty()) {
                result = item->boundingRect();
            } else {
                result |= item->boundingRect();
            }
        }
    }
    return result;
}


