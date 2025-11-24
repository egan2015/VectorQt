#pragma once

#include <QObject>
#include <QRegion>
#include <QRectF>
#include <QTimer>
#include <QElapsedTimer>
#include <QSet>
#include <QMutex>
#include <QCache>
#include <QWidget>

/**
 * 智能渲染管理器
 * 优化重绘操作，减少不必要的渲染
 */
class SmartRenderManager : public QObject
{
    Q_OBJECT

public:
    static SmartRenderManager& instance();
    
    // 脏区域管理
    void markDirty(const QRectF& rect);
    void markDirty(const QRegion& region);
    void markDirtyGlobal();  // 标记整个场景为脏
    
    // 智能更新控制
    void scheduleUpdate(QWidget* widget, const QRectF& rect = QRectF());
    void scheduleUpdateDeferred(QWidget* widget, const QRectF& rect = QRectF());
    
    // 批量更新
    void flushUpdates();
    void cancelUpdates(QWidget* widget);
    
    // 渲染优化设置
    void setMaxUpdateRate(int fps);  // 最大更新帧率
    void setDirtyRegionMerging(bool enabled);  // 是否合并脏区域
    void setBatchUpdateSize(int size);  // 批量更新大小
    
    // 性能监控
    int getCurrentFPS() const;
    void forceUpdateFPS();  // 强制更新FPS计数器
    int getPendingUpdateCount() const;
    QRegion getDirtyRegion() const;
    
    // 缓存管理
    void enableRenderCache(bool enabled);
    void clearRenderCache();

public slots:
    void processScheduledUpdates();

private:
    SmartRenderManager(QObject* parent = nullptr);
    ~SmartRenderManager() = default;
    
    struct UpdateRequest {
        QWidget* widget;
        QRectF rect;
        qint64 timestamp;
        bool immediate;
        
        bool operator==(const UpdateRequest& other) const {
            return widget == other.widget;
        }
    };
    
    // 内部方法
    void mergeDirtyRegions();
    void optimizeUpdateRequests();
    void processUpdateRequest(const UpdateRequest& request);
    bool shouldUpdate(const QRectF& rect) const;
    void updateFPSCounter();
    
    // 成员变量
    QTimer* m_updateTimer;
    QElapsedTimer m_fpsTimer;
    QElapsedTimer m_globalTimer;
    
    QRegion m_dirtyRegion;
    QList<UpdateRequest> m_pendingUpdates;
    QMutex m_mutex;
    
    // 设置参数
    int m_maxUpdateRate;  // 最大更新帧率
    int m_minUpdateInterval;  // 最小更新间隔(ms)
    bool m_dirtyRegionMerging;  // 脏区域合并
    int m_batchUpdateSize;  // 批量更新大小
    
    // 性能监控
    int m_currentFPS;
    int m_frameCount;
    qint64 m_lastFPSUpdate;
    
    // 渲染缓存
    bool m_renderCacheEnabled;
    QCache<QByteArray, QPixmap> m_renderCache;
    
    // 禁止拷贝
    SmartRenderManager(const SmartRenderManager&) = delete;
    SmartRenderManager& operator=(const SmartRenderManager&) = delete;
};

/**
 * 智能更新助手类
 * RAII模式，自动管理更新请求
 */
class SmartUpdateGuard {
public:
    SmartUpdateGuard(QWidget* widget, const QRectF& rect = QRectF(), bool immediate = false);
    ~SmartUpdateGuard();
    
    // 禁止拷贝和移动
    SmartUpdateGuard(const SmartUpdateGuard&) = delete;
    SmartUpdateGuard& operator=(const SmartUpdateGuard&) = delete;
    SmartUpdateGuard(SmartUpdateGuard&&) = delete;
    SmartUpdateGuard& operator=(SmartUpdateGuard&&) = delete;
    
private:
    QWidget* m_widget;
    QRectF m_rect;
    bool m_immediate;
};

/**
 * 渲染性能分析器
 */
class RenderProfiler {
public:
    static RenderProfiler& instance();
    
    void beginFrame();
    void endFrame();
    void beginOperation(const QString& operation);
    void endOperation(const QString& operation);
    
    // 获取性能数据
    struct PerformanceData {
        double averageFrameTime;
        double averageFPS;
        QHash<QString, double> operationTimes;
        int totalFrames;
    };
    
    PerformanceData getPerformanceData() const;
    void reset();
    void setEnabled(bool enabled);

private:
    RenderProfiler() = default;
    ~RenderProfiler() = default;
    
    QElapsedTimer m_frameTimer;
    QHash<QString, QElapsedTimer> m_operationTimers;
    QHash<QString, QList<double>> m_operationTimes;
    
    QList<double> m_frameTimes;
    int m_frameCount;
    bool m_enabled;
    
    static RenderProfiler* s_instance;
};