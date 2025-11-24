#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QQueue>
#include <QMutex>
#include <QTextStream>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QStringConverter>

/**
 * 性能监控和分析工具
 * 用于跟踪和分析应用程序性能
 */
class PerformanceMonitor : public QObject
{
    Q_OBJECT

public:
    static PerformanceMonitor& instance();
    
    // 性能计时开始
    void startTimer(const QString& category);
    
    // 性能计时结束
    qint64 endTimer(const QString& category);
    
    // 记录内存使用
    void recordMemoryUsage(const QString& tag);
    
    // 记录操作计数
    void incrementCounter(const QString& counter, int value = 1);
    
    // 记录渲染统计
    void recordRenderStats(int drawCalls, int vertices, int triangles);
    
    // 获取性能报告
    struct PerformanceReport {
        QHash<QString, double> averageTimes;      // 平均执行时间(ms)
        QHash<QString, qint64> totalTimes;        // 总执行时间(ms)
        QHash<QString, int> callCounts;           // 调用次数
        QHash<QString, int> counters;             // 各种计数器
        QHash<QString, qint64> memoryUsage;       // 内存使用记录
        int totalDrawCalls;                       // 总绘制调用数
        int totalVertices;                        // 总顶点数
        int totalTriangles;                       // 总三角形数
        double averageFPS;                        // 平均FPS
        qint64 monitoringDuration;                // 监控持续时间(ms)
        int recentDrawCalls;                      // 最近1秒的绘制调用数
        double recentFPS;                         // 最近1秒的FPS
    };
    
    PerformanceReport generateReport() const;
    
    // 导出报告到文件
    bool exportReport(const QString& filename) const;
    
    // 控制监控
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    
    // 重置所有统计数据
    void reset();
    
    // 清理旧数据（保留最近N秒的数据）
    void cleanupOldData(int keepSeconds = 5);
    
    // 设置监控参数
    void setMaxHistorySize(int size);
    void setReportInterval(int seconds);

public slots:
    void updateFrameStats();
    void exportPeriodicReport();

signals:
    void performanceAlert(const QString& message, double value);
    void reportGenerated(const PerformanceReport& report);

private:
    PerformanceMonitor(QObject* parent = nullptr);
    ~PerformanceMonitor() = default;
    
    struct TimerInfo {
        QElapsedTimer timer;
        QString category;
        qint64 startTime;
    };
    
    struct MemoryRecord {
        QString tag;
        qint64 timestamp;
        qint64 memoryUsage;
    };
    
    struct RenderStats {
        int drawCalls;
        int vertices;
        int triangles;
        qint64 timestamp;
    };
    
    // 内部方法
    void updateAverageTimes();
    void checkPerformanceThresholds();
    qint64 getCurrentMemoryUsage() const;
    
    // 成员变量
    QHash<QString, TimerInfo> m_activeTimers;
    QHash<QString, QQueue<qint64>> m_timeHistory;
    QHash<QString, qint64> m_totalTimes;
    QHash<QString, int> m_callCounts;
    QHash<QString, int> m_counters;
    
    QQueue<MemoryRecord> m_memoryHistory;
    QQueue<RenderStats> m_renderHistory;
    
    QElapsedTimer m_globalTimer;
    QTimer* m_frameTimer;
    QTimer* m_reportTimer;
    
    QMutex m_mutex;
    
    // 设置参数
    bool m_enabled;
    int m_maxHistorySize;
    int m_reportInterval;
    
    // 性能阈值
    double m_slowOperationThreshold;  // 慢操作阈值(ms)
    qint64 m_highMemoryThreshold;     // 高内存阈值(bytes)
    
    // 统计数据
    int m_frameCount;
    qint64 m_lastFrameTime;
    double m_currentFPS;
    double m_averageFPS;
    
    // 禁止拷贝
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
};

/**
 * 性能监控RAII助手类
 * 自动管理计时器的开始和结束
 */
class ScopedTimer {
public:
    ScopedTimer(const QString& category)
        : m_category(category)
    {
        PerformanceMonitor::instance().startTimer(m_category);
    }
    
    ~ScopedTimer()
    {
        PerformanceMonitor::instance().endTimer(m_category);
    }
    
    // 禁止拷贝和移动
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    QString m_category;
};

/**
 * 便利宏
 */
#define PERF_MONITOR_BEGIN(category) PerformanceMonitor::instance().startTimer(category)
#define PERF_MONITOR_END(category) PerformanceMonitor::instance().endTimer(category)
#define PERF_MONITOR_SCOPE(category) ScopedTimer __scopedTimer(category)
#define PERF_MONITOR_COUNTER(counter, value) PerformanceMonitor::instance().incrementCounter(counter, value)
#define PERF_MONITOR_MEMORY(tag) PerformanceMonitor::instance().recordMemoryUsage(tag)
#define PERF_MONITOR_RENDER(drawCalls, vertices, triangles) PerformanceMonitor::instance().recordRenderStats(drawCalls, vertices, triangles)