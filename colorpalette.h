#ifndef COLORPALETTE_H
#define COLORPALETTE_H

#include <QWidget>
#include <QToolBar>
#include <QLabel>
#include <QPushButton>
#include <QColor>
#include <QList>
#include <QScrollArea>
#include <QMouseEvent>
#include <QScrollBar>

class DrawingScene;

// 自定义滚动区域，支持鼠标拖动
class DraggableScrollArea : public QScrollArea
{
    Q_OBJECT
    
public:
    DraggableScrollArea(QWidget *parent = nullptr) : QScrollArea(parent), m_dragging(false) {}
    
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragStartPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragging = false;
            setCursor(Qt::ArrowCursor);
        }
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_dragging) {
            int deltaX = event->pos().x() - m_dragStartPos.x();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - deltaX);
            m_dragStartPos = event->pos();
        }
    }
    
private:
    bool m_dragging;
    QPoint m_dragStartPos;
};

class ColorPalette : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPalette(QWidget *parent = nullptr);
    
    void setScene(DrawingScene *scene);
    
    // W3C标准颜色名称和对应的QColor
    struct NamedColor {
        QString name;
        QColor color;
    };

signals:
    void fillColorChanged(const QColor &color);
    void strokeColorChanged(const QColor &color);
    void applyColorToSelection(const QColor &color, bool isFill); // 新增：应用颜色到选中对象

public slots:
    void selectFillColor(const QColor &color);
    void selectStrokeColor(const QColor &color);

public:
    // 获取当前填充颜色
    QColor getCurrentFillColor() const { return m_currentFillColor; }
    QColor getCurrentStrokeColor() const { return m_currentStrokeColor; }

private slots:
    void onFillColorButtonClicked();
    void onStrokeColorButtonClicked();

private:
    void setupUI();
    void createColorActions();
    void updateColorButton(QPushButton *button, const QColor &color);
    QList<NamedColor> getW3CColors() const;
    
    DrawingScene *m_scene;
    
    // 颜色区域容器
    QWidget *m_fillColorArea;
    QWidget *m_strokeColorArea;
    
    // 当前颜色显示
    QPushButton *m_fillColorButton;
    QPushButton *m_strokeColorButton;
    QLabel *m_fillColorLabel;
    QLabel *m_strokeColorLabel;
    
    // 当前选中的颜色
    QColor m_currentFillColor;
    QColor m_currentStrokeColor;
    
    // W3C标准颜色
    QList<NamedColor> m_w3cColors;
};

#endif // COLORPALETTE_H