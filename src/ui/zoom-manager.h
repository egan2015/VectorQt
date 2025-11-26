#ifndef ZOOM_MANAGER_H
#define ZOOM_MANAGER_H

#include <QObject>

class DrawingCanvas;
class DrawingView;

class ZoomManager : public QObject
{
    Q_OBJECT

public:
    explicit ZoomManager(DrawingCanvas *canvas, QObject *parent = nullptr);
    ~ZoomManager();

    // 缩放操作
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWindow();
    
    // 缩放级别设置
    void setZoomLevel(double zoom);
    double zoomLevel() const;
    
    // 缩放步长设置
    void setZoomStep(double step);
    double zoomStep() const;
    
    // 缩放范围设置
    void setMinZoomLevel(double minZoom);
    void setMaxZoomLevel(double maxZoom);
    double minZoomLevel() const;
    double maxZoomLevel() const;
    
    // 视图操作
    void centerOn(const QPointF &point);
    void centerOnContent();
    void ensureVisible(const QRectF &rect);

signals:
    void zoomChanged(double zoom);
    void zoomLevelChanged(const QString &zoomText);
    void statusMessageChanged(const QString &message);

private slots:
    void onCanvasChanged();
    void onViewChanged();

private:
    void updateZoomLabel();
    void clampZoom();
    QString formatZoomText(double zoom) const;

private:
    DrawingCanvas *m_canvas;
    DrawingView *m_view;
    
    // 缩放参数
    double m_zoomLevel;
    double m_zoomStep;
    double m_minZoomLevel;
    double m_maxZoomLevel;
    
    // 缩放历史记录（用于缩放动画）
    QList<double> m_zoomHistory;
    static const int MAX_ZOOM_HISTORY = 20;
};

#endif // ZOOM_MANAGER_H