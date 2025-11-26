#ifndef GRID_MANAGER_H
#define GRID_MANAGER_H

#include <QObject>
#include <QPointF>
#include <QColor>
#include <QPixmap>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>

class DrawingScene;

class GridManager : public QObject
{
    Q_OBJECT

public:
    explicit GridManager(DrawingScene *scene, QObject *parent = nullptr);
    ~GridManager();
    
    // 场景设置
    void setScene(DrawingScene *scene);
    DrawingScene* scene() const;

    // 网格显示控制
    void setGridVisible(bool visible);
    bool isGridVisible() const;
    void toggleGrid();
    
    // 网格参数设置
    void setGridSize(qreal size);
    qreal gridSize() const;
    
    void setGridColor(const QColor &color);
    QColor gridColor() const;
    
    void setGridStyle(Qt::PenStyle style);
    Qt::PenStyle gridStyle() const;
    
    void setGridOpacity(qreal opacity);
    qreal gridOpacity() const;
    
    // 对齐控制
    void setGridAlignmentEnabled(bool enabled);
    bool isGridAlignmentEnabled() const;
    void toggleGridAlignment();
    
    void setSnapEnabled(bool enabled);
    bool isSnapEnabled() const;
    void toggleSnap();
    
    void setObjectSnapEnabled(bool enabled);
    bool isObjectSnapEnabled() const;
    void toggleObjectSnap();
    
    // 吸附容差设置
    void setSnapTolerance(qreal tolerance);
    qreal snapTolerance() const;
    
    void setObjectSnapTolerance(qreal tolerance);
    qreal objectSnapTolerance() const;
    
    // 吸附操作
    QPointF snapToGrid(const QPointF &point) const;
    QPointF snapToNearest(const QPointF &point, const QList<QPointF>& snapPoints) const;
    QPointF snapToGridAlignment(const QPointF &point) const;
    
    // 网格设置对话框
    void showGridSettings();

signals:
    void gridVisibilityChanged(bool visible);
    void gridAlignmentChanged(bool enabled);
    void snapChanged(bool enabled);
    void gridSettingsChanged();
    void statusMessageChanged(const QString &message);

private slots:
    void onSceneChanged();

private:
    void updateGrid();
    QPointF calculateGridPoint(const QPointF &point) const;
    bool isNearGridLine(const QPointF &point, qreal tolerance, QPointF &gridPoint) const;
    QPointF findNearestSnapPoint(const QPointF &point, const QList<QPointF>& snapPoints, qreal tolerance) const;

private:
    DrawingScene *m_scene;
    
    // 网格显示参数
    bool m_gridVisible;
    qreal m_gridSize;
    QColor m_gridColor;
    Qt::PenStyle m_gridStyle;
    qreal m_gridOpacity;
    
    // 对齐参数
    bool m_gridAlignmentEnabled;
    bool m_snapEnabled;
    bool m_objectSnapEnabled;
    qreal m_snapTolerance;
    qreal m_objectSnapTolerance;
    
    // 网格渲染缓存
    mutable QPixmap m_gridCache;
    mutable bool m_gridCacheValid;
};

#endif // GRID_MANAGER_H