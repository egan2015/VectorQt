#ifndef TRANSFORM_DEMO_H
#define TRANSFORM_DEMO_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QSplitter>
#include <QDialog>
#include <QApplication>
#include <QWheelEvent>
#include <QCursor>
#include <QStatusBar>
#include <QDialog>
#include <QDebug>
#include <memory>
#include "transform-system.h"

/**
 * Qt图形界面演示 - 展示新变换系统与Qt的协作
 */

class DemoGraphicsItem : public QGraphicsRectItem
{
public:
    DemoGraphicsItem(const QRectF &rect, TransformObject *transformObj, QGraphicsItem *parent = nullptr);
    
    // 更新显示位置（根据变换系统计算）
    void updateDisplay();
    
    // 获取变换对象
    TransformObject* transformObject() const { return m_transformObj; }
    
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    
private:
    TransformObject *m_transformObj;
    QPointF m_dragStart;
    QPointF m_dragStartPos;  // 拖动开始时的位置
    bool m_isDragging;
};

class TransformScene : public QGraphicsScene
{
    Q_OBJECT
public:
    TransformScene(QObject *parent = nullptr);
    
    // 添加可变换的对象
    void addTransformItem(const QRectF &rect);
    
    // 获取选中的变换对象
    QList<TransformObject*> selectedTransformObjects();
    
    // 应用变换到选中对象
    void applyToSelection(std::shared_ptr<TransformOperation> op);
    
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    
private slots:
    void showContextMenu(QPointF pos);
    
private:
    QList<TransformObject*> m_transformObjects;
    TransformManager m_transformManager;
    QPointF m_lastScenePos;
    bool m_isPanning;
};

class TransformView : public QGraphicsView
{
    Q_OBJECT
public:
    TransformView(TransformScene *scene, QWidget *parent = nullptr);
    
protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    
private:
    bool m_isPanning;
    QPoint m_lastPanPoint;
};

class TransformDemoWindow : public QMainWindow
{
    Q_OBJECT
public:
    TransformDemoWindow(QWidget *parent = nullptr);
    
private slots:
    void addRectangle();
    void addCircle();
    void translateSelection();
    void rotateSelection();
    void scaleSelection();
    void shearSelection();
    void resetTransform();
    void showTransformInfo();
    void toggleShowOriginal(bool show);
    void toggleShowTransformChain(bool show);
    
private:
    void setupUI();
    void setupActions();
    void setupToolbar();
    void updateInfo();
    
    TransformScene *m_scene;
    TransformView *m_view;
    
    QAction *m_addRectAction;
    QAction *m_addCircleAction;
    QAction *m_translateAction;
    QAction *m_rotateAction;
    QAction *m_scaleAction;
    QAction *m_shearAction;
    QAction *m_resetAction;
    QAction *m_showInfoAction;
    QAction *m_showOriginalAction;
    QAction *m_showChainAction;
    
    bool m_showOriginal;
    bool m_showTransformChain;
};

#endif // TRANSFORM_DEMO_H
