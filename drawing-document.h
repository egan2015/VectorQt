#ifndef DRAWING_DOCUMENT_H
#define DRAWING_DOCUMENT_H

#include <QObject>
#include <vector>
#include <QRectF>



class DrawingShape;


class DrawingDocument : public QObject
{
    Q_OBJECT

    friend class CommandBase;
    friend class RemoveItemCommand;

public:
    explicit DrawingDocument(QObject *parent = nullptr);
    ~DrawingDocument();

    void addItem(DrawingShape *item);
    void removeItem(DrawingShape *item);
    std::vector<DrawingShape*> items() const;
    
    void clear();
    QRectF bounds() const;
    

private:
    std::vector<DrawingShape*> m_items;
};


#endif // DRAWING_DOCUMENT_H