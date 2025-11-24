#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>
#include <QProgressBar>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QMessageBox>
#include <QDebug>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDateTime>

#include "../core/object-pool.h"
#include "../core/smart-render-manager.h"
#include "../core/performance-monitor.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-throttle.h"

class PerformanceTestWidget : public QWidget {
    Q_OBJECT

public:
    PerformanceTestWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setupUI();
        setupPerformanceMonitoring();
    }

private slots:
    void runObjectPoolTest();
    void runRenderTest();
    void runThrottleTest();
    void runMemoryTest();
    void generatePerformanceReport();
    void updateStats();

private:
    void setupUI();
    void setupPerformanceMonitoring();
    
    // UI组件
    QLabel* m_statusLabel;
    QTextEdit* m_logText;
    QProgressBar* m_progressBar;
    QPushButton* m_objectPoolTestBtn;
    QPushButton* m_renderTestBtn;
    QPushButton* m_throttleTestBtn;
    QPushButton* m_memoryTestBtn;
    QPushButton* m_generateReportBtn;
    QSpinBox* m_testCountSpin;
    QCheckBox* m_enableMonitoringCheck;
    
    // 定时器
    QTimer* m_statsTimer;
    
    // 测试统计
    int m_testsRun;
    int m_testsPassed;
};

void PerformanceTestWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 状态栏
    m_statusLabel = new QLabel("准备就绪");
    mainLayout->addWidget(m_statusLabel);
    
    // 进度条
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    // 测试控制组
    QGroupBox* controlGroup = new QGroupBox("测试控制");
    QGridLayout* controlLayout = new QGridLayout(controlGroup);
    
    controlLayout->addWidget(new QLabel("测试次数:"), 0, 0);
    m_testCountSpin = new QSpinBox();
    m_testCountSpin->setRange(100, 100000);
    m_testCountSpin->setValue(1000);
    controlLayout->addWidget(m_testCountSpin, 0, 1);
    
    m_enableMonitoringCheck = new QCheckBox("启用性能监控");
    m_enableMonitoringCheck->setChecked(true);
    controlLayout->addWidget(m_enableMonitoringCheck, 1, 0, 1, 2);
    
    mainLayout->addWidget(controlGroup);
    
    // 测试按钮组
    QGroupBox* testGroup = new QGroupBox("性能测试");
    QVBoxLayout* testLayout = new QVBoxLayout(testGroup);
    
    m_objectPoolTestBtn = new QPushButton("对象池测试");
    connect(m_objectPoolTestBtn, &QPushButton::clicked, this, &PerformanceTestWidget::runObjectPoolTest);
    testLayout->addWidget(m_objectPoolTestBtn);
    
    m_renderTestBtn = new QPushButton("渲染性能测试");
    connect(m_renderTestBtn, &QPushButton::clicked, this, &PerformanceTestWidget::runRenderTest);
    testLayout->addWidget(m_renderTestBtn);
    
    m_throttleTestBtn = new QPushButton("节流器测试");
    connect(m_throttleTestBtn, &QPushButton::clicked, this, &PerformanceTestWidget::runThrottleTest);
    testLayout->addWidget(m_throttleTestBtn);
    
    m_memoryTestBtn = new QPushButton("内存管理测试");
    connect(m_memoryTestBtn, &QPushButton::clicked, this, &PerformanceTestWidget::runMemoryTest);
    testLayout->addWidget(m_memoryTestBtn);
    
    m_generateReportBtn = new QPushButton("生成性能报告");
    connect(m_generateReportBtn, &QPushButton::clicked, this, &PerformanceTestWidget::generatePerformanceReport);
    testLayout->addWidget(m_generateReportBtn);
    
    mainLayout->addWidget(testGroup);
    
    // 日志输出
    m_logText = new QTextEdit();
    m_logText->setMaximumHeight(200);
    m_logText->setReadOnly(true);
    mainLayout->addWidget(m_logText);
    
    setLayout(mainLayout);
    setWindowTitle("VectorQt 性能测试工具");
    resize(500, 600);
    
    m_testsRun = 0;
    m_testsPassed = 0;
}

void PerformanceTestWidget::setupPerformanceMonitoring() {
    // 设置性能监控
    PerformanceMonitor::instance().setEnabled(true);
    PerformanceMonitor::instance().setMaxHistorySize(1000);
    
    // 设置统计更新定时器
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &PerformanceTestWidget::updateStats);
    m_statsTimer->start(1000);  // 每秒更新一次
    
    // 连接性能警报信号
    connect(&PerformanceMonitor::instance(), &PerformanceMonitor::performanceAlert,
            [this](const QString& message, double value) {
                logMessage(QString("⚠️ 性能警报: %1 (值: %2)").arg(message).arg(value));
            });
}

void PerformanceTestWidget::logMessage(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logText->append(QString("[%1] %2").arg(timestamp).arg(message));
}

void PerformanceTestWidget::runObjectPoolTest() {
    logMessage("🚀 开始对象池性能测试...");
    m_statusLabel->setText("运行对象池测试...");
    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(m_testCountSpin->value());
    
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_BEGIN("ObjectPoolTest");
    }
    
    // 测试对象池性能
    int testCount = m_testCountSpin->value();
    auto& poolManager = GlobalObjectPoolManager::instance();
    ObjectPool<DrawingPath>* pathPool = poolManager.getPool<DrawingPath>();
    
    // 预热对象池
    pathPool->warmUp(50);
    
    // 测试获取和释放对象
    QList<std::unique_ptr<DrawingPath>> acquiredObjects;
    
    for (int i = 0; i < testCount; ++i) {
        // 获取对象
        auto pathObj = pathPool->acquire();
        acquiredObjects.append(std::move(pathObj));
        
        // 模拟使用对象
        if (acquiredObjects.last()) {
            QPainterPath testPath;
            testPath.lineTo(i % 100, i % 100);
            acquiredObjects.last()->setPath(testPath);
        }
        
        // 更新进度
        if (i % 100 == 0) {
            m_progressBar->setValue(i);
            QApplication::processEvents();
        }
        
        // 随机释放一些对象
        if (i > 10 && i % 3 == 0 && !acquiredObjects.isEmpty()) {
            int releaseIndex = QRandomGenerator::global()->bounded(acquiredObjects.size());
            pathPool->release(std::move(acquiredObjects[releaseIndex]));
            acquiredObjects.removeAt(releaseIndex);
        }
    }
    
    // 释放所有剩余对象
    for (auto& obj : acquiredObjects) {
        pathPool->release(std::move(obj));
    }
    acquiredObjects.clear();
    
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_END("ObjectPoolTest");
    }
    
    m_progressBar->setVisible(false);
    m_statusLabel->setText("对象池测试完成");
    logMessage(QString("✅ 对象池测试完成，处理了 %1 个对象").arg(testCount));
    
    m_testsRun++;
    m_testsPassed++;
}

void PerformanceTestWidget::runRenderTest() {
    logMessage("🚀 开始渲染性能测试...");
    m_statusLabel->setText("运行渲染测试...");
    
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_BEGIN("RenderTest");
    }
    
    auto& renderManager = SmartRenderManager::instance();
    renderManager.setMaxUpdateRate(120);  // 设置高帧率
    
    // 模拟大量更新请求
    int updateCount = m_testCountSpin->value();
    
    for (int i = 0; i < updateCount; ++i) {
        QRectF updateRect(i % 800, i % 600, 50 + (i % 100), 50 + (i % 100));
        renderManager.scheduleUpdate(this, updateRect);
        
        PERF_MONITOR_COUNTER("UpdateRequests", 1);
        
        if (i % 100 == 0) {
            QApplication::processEvents();
        }
    }
    
    // 强制处理所有更新
    renderManager.flushUpdates();
    
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_END("RenderTest");
    }
    
    m_statusLabel->setText("渲染测试完成");
    logMessage(QString("✅ 渲染测试完成，处理了 %1 个更新请求").arg(updateCount));
    logMessage(QString("📊 当前FPS: %1").arg(renderManager.getCurrentFPS()));
    
    m_testsRun++;
    m_testsPassed++;
}

void PerformanceTestWidget::runThrottleTest() {
    logMessage("🚀 开始节流器性能测试...");
    m_statusLabel->setText("运行节流器测试...");
    
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_BEGIN("ThrottleTest");
    }
    
    DrawingThrottle throttle;
    throttle.setThrottleInterval(16);  // 60fps
    throttle.setDistanceThreshold(2.0);
    
    int pointCount = m_testCountSpin->value();
    int processedPoints = 0;
    
    // 模拟大量鼠标移动事件
    for (int i = 0; i < pointCount; ++i) {
        QPointF point(i % 800, i % 600);
        
        throttle.throttleMouseMove(point, [&processedPoints, i](const QPointF& pos) {
            processedPoints++;
            // 模拟处理逻辑
            Q_UNUSED(pos)
        });
        
        PERF_MONITOR_COUNTER("MouseMoveEvents", 1);
        
        if (i % 100 == 0) {
            QApplication::processEvents();
        }
    }
    
    // 处理剩余事件
    throttle.flushPendingEvents();
    
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_END("ThrottleTest");
    }
    
    m_statusLabel->setText("节流器测试完成");
    logMessage(QString("✅ 节流器测试完成，输入点数: %1，处理点数: %2").arg(pointCount).arg(processedPoints));
    logMessage(QString("📊 节流效率: %1%").arg((double)processedPoints / pointCount * 100, 0, 'f', 1));
    
    m_testsRun++;
    m_testsPassed++;
}

void PerformanceTestWidget::runMemoryTest() {
    logMessage("🚀 开始内存管理测试...");
    m_statusLabel->setText("运行内存测试...");
    
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_BEGIN("MemoryTest");
        PERF_MONITOR_MEMORY("MemoryTest_Start");
    }
    
    // 测试内存分配和释放
    QList<DrawingPath*> paths;
    int objectCount = m_testCountSpin->value() / 10;  // 减少对象数量以避免内存不足
    
    // 分配大量对象
    for (int i = 0; i < objectCount; ++i) {
        DrawingPath* path = new DrawingPath();
        QPainterPath testPath;
        testPath.addRect(i % 100, i % 100, 50, 50);
        path->setPath(testPath);
        paths.append(path);
        
        PERF_MONITOR_COUNTER("ObjectsAllocated", 1);
        
        if (i % 100 == 0) {
            PERF_MONITOR_MEMORY(QString("MemoryTest_%1").arg(i));
            QApplication::processEvents();
        }
    }
    
    // 记录峰值内存
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_MEMORY("MemoryTest_Peak");
    }
    
    // 释放所有对象
    for (DrawingPath* path : paths) {
        delete path;
        PERF_MONITOR_COUNTER("ObjectsDeallocated", 1);
    }
    paths.clear();
    
    // 记录释放后内存
    if (m_enableMonitoringCheck->isChecked()) {
        PERF_MONITOR_MEMORY("MemoryTest_End");
        PERF_MONITOR_END("MemoryTest");
    }
    
    m_statusLabel->setText("内存测试完成");
    logMessage(QString("✅ 内存测试完成，分配和释放了 %1 个对象").arg(objectCount));
    
    m_testsRun++;
    m_testsPassed++;
}

void PerformanceTestWidget::generatePerformanceReport() {
    logMessage("📊 生成性能报告...");
    
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("%1/VectorQt_PerformanceTest_%2.txt").arg(documentsPath).arg(timestamp);
    
    bool success = PerformanceMonitor::instance().exportReport(filename);
    
    if (success) {
        auto report = PerformanceMonitor::instance().generateReport();
        
        logMessage(QString("✅ 性能报告已生成: %1").arg(filename));
        logMessage(QString("📈 监控时长: %1 秒").arg(report.monitoringDuration / 1000.0));
        logMessage(QString("🎯 平均FPS: %1").arg(report.averageFPS, 0, 'f', 2));
        logMessage(QString("🔧 总绘制调用: %1").arg(report.totalDrawCalls));
        
        // 显示性能统计摘要
        for (auto it = report.averageTimes.constBegin(); it != report.averageTimes.constEnd(); ++it) {
            logMessage(QString("⏱️ %1: 平均 %2ms (调用 %3 次)")
                      .arg(it.key())
                      .arg(it.value(), 0, 'f', 3)
                      .arg(report.callCounts.value(it.key())));
        }
        
        QMessageBox::information(this, "报告生成成功", 
                                QString("性能报告已保存到:\n%1").arg(filename));
    } else {
        logMessage("❌ 性能报告生成失败");
        QMessageBox::warning(this, "报告生成失败", "无法生成性能报告文件。");
    }
}

void PerformanceTestWidget::updateStats() {
    if (!m_enableMonitoringCheck->isChecked()) {
        return;
    }
    
    auto& renderManager = SmartRenderManager::instance();
    int pendingUpdates = renderManager.getPendingUpdateCount();
    int currentFPS = renderManager.getCurrentFPS();
    
    // 更新状态栏显示实时统计
    if (m_testsRun > 0) {
        m_statusLabel->setText(QString("测试: %1/%2 通过 | FPS: %3 | 待更新: %4")
                              .arg(m_testsPassed)
                              .arg(m_testsRun)
                              .arg(currentFPS)
                              .arg(pendingUpdates));
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    PerformanceTestWidget widget;
    widget.show();
    
    return app.exec();
}

#include "performance-test.moc"