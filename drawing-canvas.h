#ifndef DRAWING_CANVAS_H
#define DRAWING_CANVAS_H

#include <QWidget>
#include <QGraphicsScene>
#include <QGraphicsView>

class DrawingView;

class DrawingCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit DrawingCanvas(QWidget *parent = nullptr);
    
    QGraphicsScene* scene() const;
    DrawingView* view() const;
    
    void setScene(QGraphicsScene *scene);
    
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWindow();
    void centerOnContent();
    void setZoomLevel(double zoom);
    
    double zoomLevel() const;
    
    

signals:
    void zoomChanged(double zoom);
    void mousePositionChanged(const QPointF &pos);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUI();
    
    QGraphicsScene *m_scene;  // 引用外部传入的scene，不负责创建
    DrawingView *m_view;      // 使用具体的DrawingView类型
    double m_zoomLevel;
};

#endif // DRAWING_CANVAS_H