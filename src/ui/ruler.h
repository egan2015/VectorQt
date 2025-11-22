#ifndef RULER_H
#define RULER_H

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QGraphicsView>
#include <QActionGroup>

class Ruler : public QWidget
{
    Q_OBJECT

public:
    enum Orientation {
        Horizontal,
        Vertical
    };
    
    enum Unit {
        Pixels,
        Millimeters,
        Centimeters,
        Inches,
        Points
    };

signals:
    void unitChanged(Unit unit);
    void guideRequested(const QPointF &position, Qt::Orientation orientation);
    void unitChangedForAll(Unit unit); // 新增信号，用于同步所有标尺的单位

public:
    explicit Ruler(Orientation orientation, QWidget *parent = nullptr);
    
    void setOrigin(qreal origin);
    void setScale(qreal scale);
    void setMousePos(const QPointF &pos);
    void setView(QGraphicsView *view);
    void setUnit(Unit unit);
    Unit unit() const { return m_unit; }
    
    // 🌟 选中对象边界显示
    void setSelectedBounds(const QRectF &bounds);
    void clearSelectedBounds();
    QRectF selectedBounds() const { return m_selectedBounds; }
    
    QSize sizeHint() const override;
    
    static int rulerSize() { return RULER_SIZE; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void setUnitPixels();
    void setUnitMillimeters();
    void setUnitCentimeters();
    void setUnitInches();
    void setUnitPoints();

private:
    void drawSelectionBounds(QPainter *painter);

private:
    void drawHorizontalRuler(QPainter *painter);
    void drawVerticalRuler(QPainter *painter);
    void drawTicks(QPainter *painter, const QRectF &rect);
    void drawBackground(QPainter *painter);
    void drawMouseIndicator(QPainter *painter);
    void createContextMenu();
    void updateMenuCheckState();
    qreal getTickSize(int level) const;
    qreal convertToUnit(qreal pixels) const;
    QString formatNumber(qreal value) const;
    qreal getUnitScale() const;
    
    Orientation m_orientation;
    Unit m_unit;
    qreal m_origin;
    qreal m_scale;
    QPointF m_mousePos;
    bool m_mouseTracking;
    bool m_hovered;
    QMenu *m_contextMenu;
    QAction *m_pixelsAction;
    QAction *m_millimetersAction;
    QAction *m_centimetersAction;
    QAction *m_inchesAction;
    QAction *m_pointsAction;
    QGraphicsView *m_view;
    
    // 🌟 选中对象边界相关
    QRectF m_selectedBounds;  // 选中对象的场景边界
    bool m_hasSelection;      // 是否有选中对象
    
    // Inkscape-inspired constants
    static const int RULER_SIZE = 24;  // Slightly larger for better readability
    static const int MAJOR_TICK_LENGTH = 12;
    static const int MINOR_TICK_LENGTH = 6;
    static const int TEXT_MARGIN = 4;
    static const int GRADIENT_HEIGHT = 8;
};

#endif // RULER_H