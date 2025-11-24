#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTimer>
#include <QColor>

class DrawingScene;

/**
 * 简化的性能监控面板 - 专门用于属性面板中的tab页
 */
class PerformancePanelTab : public QWidget
{
    Q_OBJECT

public:
    explicit PerformancePanelTab(QWidget *parent = nullptr);
    ~PerformancePanelTab();
    
    void setScene(DrawingScene *scene);

private slots:
    void updatePerformanceStats();

private:
    void setupUI();
    
    // 性能显示组件
    QLabel *m_fpsLabel;
    QLabel *m_memoryLabel;
    QLabel *m_drawCallsLabel;
    QLabel *m_updateTimeLabel;
    QLabel *m_shapesCountLabel;
    
    // 性能统计
    QTimer *m_updateTimer;
    int m_frameCount;
    qint64 m_totalUpdateTime;
    int m_drawCallCount;
    
    // 场景引用
    DrawingScene *m_scene;
};