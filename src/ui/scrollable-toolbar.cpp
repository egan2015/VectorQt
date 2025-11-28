#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QDebug>
#include <QScrollArea>
#include <QWidget>
#include <QToolButton>
#include <QApplication>
#include <QFrame>
#include <QScrollBar>
#include "scrollable-toolbar.h"

ScrollableToolBar::ScrollableToolBar(const QString &title, QWidget *parent)
    : QToolBar(title, parent), m_scrollingEnabled(true)
{
    // 创建一个容器widget来包含所有工具
    m_containerWidget = new QWidget();
    m_containerLayout = new QVBoxLayout(m_containerWidget);
    m_containerLayout->setContentsMargins(1, 1, 1, 1);
    m_containerLayout->setSpacing(0);
    m_containerLayout->setAlignment(Qt::AlignTop); // 顶对齐
    // 创建滚动区域
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidget(m_containerWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setMinimumWidth(30);
    m_scrollArea->setMaximumWidth(32);
    m_scrollArea->setMinimumHeight(0);
    m_scrollArea->setMaximumHeight(QWIDGETSIZE_MAX);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    // 将滚动区域添加到工具栏
    addWidget(m_scrollArea);

    // 设置样式
    setStyleSheet(R"(
        QToolBar {
            spacing: 0px;
            padding: 1px;
            background: palette(window);
            border: 1px solid palette(mid);
            border-radius: 4px;
            margin: 1px;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 2px;
            margin: 0px;
        }
        QToolButton:hover {
            background: palette(highlight);
            color: palette(highlighted-text);
        }
        QToolButton:pressed {
            background: palette(dark);
            color: palette(bright-text);
        }
        QToolButton:checked {
            background: palette(highlight);
            color: palette(highlighted-text);
        }
    )");

    // 安装事件过滤器来处理滚轮事件
    installEventFilter(this);
    
    // 连接topLevelChanged信号来检测停靠区域变化
    connect(this, &QToolBar::topLevelChanged, this, [this](bool topLevel) {
        // 当工具栏停靠状态改变时，检查方向
        QTimer::singleShot(100, this, [this]() {
            updateLayoutDirection();
        });
    });
}

ScrollableToolBar::~ScrollableToolBar()
{
    // Qt会自动清理子对象
}

void ScrollableToolBar::setScrollingEnabled(bool enabled)
{
    m_scrollingEnabled = enabled;
}

QAction *ScrollableToolBar::addAction(QAction *action)
{
    // 创建工具按钮并添加到容器布局
    QToolButton *button = new QToolButton();
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(QSize(24, 24));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(28, 28);           // 固定大小，减少额外空间
    button->setContentsMargins(0, 0, 0, 0); // 移除内容边距

    // 保存按钮引用
    m_toolButtons.append(button);
    m_containerLayout->addWidget(button);

    return action;
}

void ScrollableToolBar::addAction(QAction *action, const QString &text)
{
    action->setText(text);
    addAction(action);
}

QAction *ScrollableToolBar::addAction(const QString &text)
{
    QAction *action = new QAction(text, this);
    addAction(action);
    return action;
}

QAction *ScrollableToolBar::addAction(const QIcon &icon, const QString &text)
{
    QAction *action = new QAction(icon, text, this);
    addAction(action);
    return action;
}

void ScrollableToolBar::addSeparator()
{
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setMaximumHeight(1);
    m_containerLayout->addWidget(separator);
}

bool ScrollableToolBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scrollArea && event->type() == QEvent::Wheel)
    {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
        if (m_scrollingEnabled)
        {
            // 直接操作滚动条的值
            int delta = wheelEvent->angleDelta().y();
            int currentValue = m_scrollArea->verticalScrollBar()->value();
            int step = qAbs(delta) / 8 * 10; // 每次滚动10个像素
            int newValue = currentValue - (delta > 0 ? step : -step);

            // 确保新值在有效范围内
            int maxValue = m_scrollArea->verticalScrollBar()->maximum();
            int minValue = m_scrollArea->verticalScrollBar()->minimum();
            newValue = qBound(minValue, newValue, maxValue);

            m_scrollArea->verticalScrollBar()->setValue(newValue);
            return true;
        }
    }

    return QToolBar::eventFilter(watched, event);
}

void ScrollableToolBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::OrientationChange)
    {
        updateLayoutDirection();
    }
    QToolBar::changeEvent(event);
}

bool ScrollableToolBar::event(QEvent *event)
{
    if (event->type() == QEvent::Resize)
    {
        // 检查方向是否发生变化
        static Qt::Orientation lastOrientation = orientation();
        if (lastOrientation != orientation())
        {
            lastOrientation = orientation();
            updateLayoutDirection();
        }
    }
    return QToolBar::event(event);
}

void ScrollableToolBar::updateLayoutDirection()
{
    if (!m_containerWidget) return;
    
    // 调试输出
    qDebug() << "Updating layout direction, current orientation:" << (orientation() == Qt::Vertical ? "Vertical" : "Horizontal");
    qDebug() << "Total buttons to add:" << m_toolButtons.size();
    
    // 保存所有按钮的引用
    QList<QToolButton*> buttons = m_toolButtons;
    
    // 从布局中移除所有按钮
    for (QToolButton *button : buttons) {
        m_containerLayout->removeWidget(button);
    }
    
    // 删除现有布局
    delete m_containerLayout;
    
    if (orientation() == Qt::Vertical) {
        // 垂直布局
        m_containerLayout = new QVBoxLayout(m_containerWidget);
        m_containerLayout->setContentsMargins(1, 1, 1, 1);
        m_containerLayout->setSpacing(0);
        m_containerLayout->setAlignment(Qt::AlignTop); // 顶对齐
        
        // 调整滚动区域设置
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        // 水平布局
        m_containerLayout = new QHBoxLayout(m_containerWidget);
        m_containerLayout->setContentsMargins(1, 1, 1, 1);
        m_containerLayout->setSpacing(0);
        m_containerLayout->setAlignment(Qt::AlignLeft); // 左对齐
        
        // 调整滚动区域设置
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    
    // 重新添加所有按钮到新布局
    for (QToolButton *button : buttons) {
        qDebug() << "Adding button to layout";
        m_containerLayout->addWidget(button);
    }
    
    // 强制更新容器大小
    if (orientation() == Qt::Horizontal) {
        // 水平布局：计算总宽度
        int totalWidth = buttons.size() * 36; // 每个按钮36px
        m_containerWidget->setMinimumSize(totalWidth, 36);
        m_containerWidget->setMaximumSize(totalWidth, 36);
        qDebug() << "Setting container size to:" << totalWidth << "x" << 36;
    } else {
        // 垂直布局：计算总高度
        int totalHeight = buttons.size() * 36;
        m_containerWidget->setMinimumSize(36, totalHeight);
        m_containerWidget->setMaximumSize(36, totalHeight);
    }
    
    m_containerWidget->adjustSize();
    m_containerWidget->updateGeometry();
    m_scrollArea->updateGeometry();
    
    // 简化布局设置，让Qt自动管理
    if (orientation() == Qt::Vertical) {
        // 垂直布局 - 只限制宽度
        m_scrollArea->setMinimumWidth(38);
        m_scrollArea->setMaximumWidth(38);
        m_scrollArea->setMinimumHeight(0);
        m_scrollArea->setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        // 水平布局 - 确保能显示所有内容
        int totalWidth = buttons.size() * 36;
        m_scrollArea->setMinimumHeight(38);
        m_scrollArea->setMaximumHeight(38);
        m_scrollArea->setMinimumWidth(totalWidth);
        m_scrollArea->setMaximumWidth(totalWidth);
        qDebug() << "Setting scroll area size to:" << totalWidth << "x" << 38;
    }
}

// 添加一个公共方法供外部调用
void ScrollableToolBar::forceUpdateLayout()
{
    qDebug() << "Force updating layout direction";
    updateLayoutDirection();
}