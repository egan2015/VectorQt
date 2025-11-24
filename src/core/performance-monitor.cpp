#include "performance-monitor.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#elif defined(Q_OS_LINUX) || defined(Q_OS_MAC)
#include <sys/resource.h>
#include <unistd.h>
#endif

PerformanceMonitor& PerformanceMonitor::instance()
{
    static PerformanceMonitor instance;
    return instance;
}

PerformanceMonitor::PerformanceMonitor(QObject* parent)
    : QObject(parent)
    , m_enabled(true)
    , m_maxHistorySize(1000)
    , m_reportInterval(60)  // 60秒
    , m_slowOperationThreshold(16.67)  // 60fps = 16.67ms per frame
    , m_highMemoryThreshold(500 * 1024 * 1024)  // 500MB
    , m_frameCount(0)
    , m_lastFrameTime(0)
    , m_currentFPS(0)
    , m_averageFPS(0)
{
    m_globalTimer.start();
    
    // 设置帧统计定时器
    m_frameTimer = new QTimer(this);
    m_frameTimer->setInterval(1000);  // 每秒更新一次FPS
    connect(m_frameTimer, &QTimer::timeout, this, &PerformanceMonitor::updateFrameStats);
    m_frameTimer->start();
    
    // 设置定期报告定时器
    m_reportTimer = new QTimer(this);
    m_reportTimer->setInterval(m_reportInterval * 1000);
    connect(m_reportTimer, &QTimer::timeout, this, &PerformanceMonitor::exportPeriodicReport);
    // m_reportTimer->start();  // 可选启用
}

void PerformanceMonitor::startTimer(const QString& category)
{
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    
    TimerInfo info;
    info.category = category;
    info.startTime = m_globalTimer.elapsed();
    info.timer.start();
    
    m_activeTimers[category] = info;
}

qint64 PerformanceMonitor::endTimer(const QString& category)
{
    if (!m_enabled) return 0;
    
    QMutexLocker locker(&m_mutex);
    
    auto it = m_activeTimers.find(category);
    if (it == m_activeTimers.end()) {
        return 0;
    }
    
    qint64 elapsed = it->timer.elapsed();
    m_activeTimers.erase(it);
    
    // 更新统计数据
    m_totalTimes[category] += elapsed;
    m_callCounts[category]++;
    
    // 添加到历史记录
    m_timeHistory[category].enqueue(elapsed);
    
    // 限制历史记录大小
    while (m_timeHistory[category].size() > m_maxHistorySize) {
        m_timeHistory[category].dequeue();
    }
    
    // 检查性能阈值
    if (elapsed > m_slowOperationThreshold) {
        emit performanceAlert(QString("Slow operation detected: %1 took %2ms").arg(category).arg(elapsed), elapsed);
    }
    
    return elapsed;
}

void PerformanceMonitor::recordMemoryUsage(const QString& tag)
{
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    
    MemoryRecord record;
    record.tag = tag;
    record.timestamp = m_globalTimer.elapsed();
    record.memoryUsage = getCurrentMemoryUsage();
    
    m_memoryHistory.enqueue(record);
    
    // 限制历史记录大小
    while (m_memoryHistory.size() > m_maxHistorySize) {
        m_memoryHistory.dequeue();
    }
    
    // 检查内存阈值
    if (record.memoryUsage > m_highMemoryThreshold) {
        emit performanceAlert(QString("High memory usage detected: %1 - %2MB").arg(tag).arg(record.memoryUsage / (1024.0 * 1024.0)), record.memoryUsage);
    }
}

void PerformanceMonitor::incrementCounter(const QString& counter, int value)
{
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    m_counters[counter] += value;
}

void PerformanceMonitor::recordRenderStats(int drawCalls, int vertices, int triangles)
{
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    
    RenderStats stats;
    stats.drawCalls = drawCalls;
    stats.vertices = vertices;
    stats.triangles = triangles;
    stats.timestamp = m_globalTimer.elapsed();
    
    m_renderHistory.enqueue(stats);
    
    // 限制历史记录大小
    while (m_renderHistory.size() > m_maxHistorySize) {
        m_renderHistory.dequeue();
    }
}

PerformanceMonitor::PerformanceReport PerformanceMonitor::generateReport() const
{
    // const方法中不能使用QMutexLocker，简化处理
    
    PerformanceReport report;
    report.monitoringDuration = m_globalTimer.elapsed();
    
    // 计算平均时间
    for (auto it = m_timeHistory.constBegin(); it != m_timeHistory.constEnd(); ++it) {
        const QString& category = it.key();
        const QQueue<qint64>& times = it.value();
        
        if (!times.isEmpty()) {
            qint64 total = 0;
            for (qint64 time : times) {
                total += time;
            }
            report.averageTimes[category] = static_cast<double>(total) / times.size();
        }
    }
    
    report.totalTimes = m_totalTimes;
    report.callCounts = m_callCounts;
    report.counters = m_counters;
    report.averageFPS = m_averageFPS;
    
    // 计算渲染统计
    int totalDrawCalls = 0, totalVertices = 0, totalTriangles = 0;
    qint64 currentTime = m_globalTimer.elapsed();
    qint64 oneSecondAgo = currentTime - 1000;  // 1秒前的时间点
    
    int recentDrawCalls = 0;
    for (const RenderStats& stats : m_renderHistory) {
        totalDrawCalls += stats.drawCalls;
        totalVertices += stats.vertices;
        totalTriangles += stats.triangles;
        
        // 计算最近1秒的绘制调用
        if (stats.timestamp >= oneSecondAgo) {
            recentDrawCalls += stats.drawCalls;
        }
    }
    report.totalDrawCalls = totalDrawCalls;
    report.totalVertices = totalVertices;
    report.totalTriangles = totalTriangles;
    report.recentDrawCalls = recentDrawCalls;
    report.recentFPS = m_currentFPS;  // 使用当前的FPS
    
    // 内存使用统计
    for (const MemoryRecord& record : m_memoryHistory) {
        report.memoryUsage[record.tag] = record.memoryUsage;
    }
    
    return report;
}

bool PerformanceMonitor::exportReport(const QString& filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open performance report file:" << filename;
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    PerformanceReport report = generateReport();
    
    // 写入报告头
    out << "VectorQt Performance Report\n";
    out << "Generated: " << QDateTime::currentDateTime().toString() << "\n";
    out << "Monitoring Duration: " << report.monitoringDuration / 1000.0 << " seconds\n";
    out << "Average FPS: " << QString::number(report.averageFPS, 'f', 2) << "\n\n";
    
    // 写入性能统计
    out << "=== Performance Statistics ===\n";
    out << "Category\t\tAverage Time(ms)\tTotal Time(ms)\tCall Count\n";
    for (auto it = report.averageTimes.constBegin(); it != report.averageTimes.constEnd(); ++it) {
        const QString& category = it.key();
        out << category << "\t\t" 
            << QString::number(it.value(), 'f', 3) << "\t\t"
            << report.totalTimes.value(category) << "\t\t"
            << report.callCounts.value(category) << "\n";
    }
    
    // 写入计数器统计
    out << "\n=== Counter Statistics ===\n";
    for (auto it = report.counters.constBegin(); it != report.counters.constEnd(); ++it) {
        out << it.key() << ": " << it.value() << "\n";
    }
    
    // 写入渲染统计
    out << "\n=== Render Statistics ===\n";
    out << "Total Draw Calls: " << report.totalDrawCalls << "\n";
    out << "Total Vertices: " << report.totalVertices << "\n";
    out << "Total Triangles: " << report.totalTriangles << "\n";
    
    // 写入内存使用
    out << "\n=== Memory Usage ===\n";
    for (auto it = report.memoryUsage.constBegin(); it != report.memoryUsage.constEnd(); ++it) {
        out << it.key() << ": " << it.value() / (1024.0 * 1024.0) << " MB\n";
    }
    
    file.close();
    qDebug() << "Performance report exported to:" << filename;
    return true;
}

void PerformanceMonitor::setEnabled(bool enabled)
{
    m_enabled = enabled;
    
    if (!enabled) {
        // 清理所有活动计时器
        m_activeTimers.clear();
    }
}

void PerformanceMonitor::reset()
{
    QMutexLocker locker(&m_mutex);
    
    m_activeTimers.clear();
    m_timeHistory.clear();
    m_totalTimes.clear();
    m_callCounts.clear();
    m_counters.clear();
    m_memoryHistory.clear();
    m_renderHistory.clear();
    
    m_frameCount = 0;
    m_lastFrameTime = 0;
    m_currentFPS = 0.0;
    m_averageFPS = 0.0;
    
    m_globalTimer.restart();
}

void PerformanceMonitor::cleanupOldData(int keepSeconds)
{
    QMutexLocker locker(&m_mutex);
    
    qint64 cutoffTime = m_globalTimer.elapsed() - (keepSeconds * 1000);
    
    // 清理旧的渲染统计
    auto it = m_renderHistory.begin();
    while (it != m_renderHistory.end()) {
        if (it->timestamp < cutoffTime) {
            it = m_renderHistory.erase(it);
        } else {
            ++it;
        }
    }
    
    // 清理旧的内存记录
    auto memIt = m_memoryHistory.begin();
    while (memIt != m_memoryHistory.end()) {
        if (memIt->timestamp < cutoffTime) {
            memIt = m_memoryHistory.erase(memIt);
        } else {
            ++memIt;
        }
    }
}

void PerformanceMonitor::setMaxHistorySize(int size)
{
    m_maxHistorySize = qMax(100, size);
}

void PerformanceMonitor::setReportInterval(int seconds)
{
    m_reportInterval = qMax(10, seconds);
    m_reportTimer->setInterval(m_reportInterval * 1000);
}

void PerformanceMonitor::updateFrameStats()
{
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    
    qint64 currentTime = m_globalTimer.elapsed();
    if (m_lastFrameTime > 0) {
        qint64 frameDelta = currentTime - m_lastFrameTime;
        if (frameDelta > 0) {
            m_currentFPS = 1000.0 / frameDelta;
            
            // 计算平均FPS
            static const int FPS_HISTORY_SIZE = 60;
            static QQueue<double> fpsHistory;
            
            fpsHistory.enqueue(m_currentFPS);
            while (fpsHistory.size() > FPS_HISTORY_SIZE) {
                fpsHistory.dequeue();
            }
            
            double totalFPS = 0;
            for (double fps : fpsHistory) {
                totalFPS += fps;
            }
            m_averageFPS = totalFPS / fpsHistory.size();
        }
    }
    m_lastFrameTime = currentTime;
}

void PerformanceMonitor::exportPeriodicReport()
{
    if (!m_enabled) return;
    
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("%1/VectorQt_Performance_%2.txt").arg(documentsPath).arg(timestamp);
    
    if (exportReport(filename)) {
        PerformanceReport report = generateReport();
        emit reportGenerated(report);
    }
}

qint64 PerformanceMonitor::getCurrentMemoryUsage() const
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#elif defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024;  // Linux: KB, macOS: bytes
    }
#endif
    return 0;
}