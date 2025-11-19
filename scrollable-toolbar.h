#ifndef SCROLLABLE_TOOLBAR_H
#define SCROLLABLE_TOOLBAR_H

#include <QToolBar>
#include <QWheelEvent>
#include <QScrollArea>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>

class ScrollableToolBar : public QToolBar
{
    Q_OBJECT

public:
    explicit ScrollableToolBar(const QString &title, QWidget *parent = nullptr);
    ~ScrollableToolBar();
    
    // 设置是否启用滚动
    void setScrollingEnabled(bool enabled);
    
    // 重写addAction方法，将action添加到容器中
    QAction *addAction(QAction *action);
    void addAction(QAction *action, const QString &text);
    QAction *addAction(const QString &text);
    QAction *addAction(const QIcon &icon, const QString &text);
    
    // 添加分隔符
    void addSeparator();
    
    // 强制更新布局（用于调试）
    void forceUpdateLayout();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    bool m_scrollingEnabled;
    
    QScrollArea *m_scrollArea;
    QWidget *m_containerWidget;
    QBoxLayout *m_containerLayout; // 改为基类，可以是QVBoxLayout或QHBoxLayout
    QList<QToolButton*> m_toolButtons; // 保存按钮引用
    
    void updateLayoutDirection();
};

#endif // SCROLLABLE_TOOLBAR_H