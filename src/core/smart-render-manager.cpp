#include "smart-render-manager.h"
#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QDebug>
#include <algorithm>
#include <QPixmap>
#include <QList>

SmartRenderManager& SmartRenderManager::instance()
{
    static SmartRenderManager instance;
    return instance;
}

SmartRenderManager::SmartRenderManager(QObject* parent)
    : QObject(parent)
    , m_maxUpdateRate(60)
    , m_minUpdateInterval(1000 / 60)  // 16ms for 60fps
    , m_dirtyRegionMerging(true)
    , m_batchUpdateSize(10)
    , m_currentFPS(0)
    , m_frameCount(0)
    , m_lastFPSUpdate(0)
    , m_renderCacheEnabled(false)
{
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(m_minUpdateInterval);
    connect(m_updateTimer, &QTimer::timeout, this, &SmartRenderManager::processScheduledUpdates);
    
    m_fpsTimer.start();
    m_globalTimer.start();
    
    // 启动更新定时器
    m_updateTimer->start();
}

void SmartRenderManager::markDirty(const QRectF& rect)
{
    // QMutexLocker removed - Qt event loop is single-threaded
    
    if (rect.isValid()) {
        m_dirtyRegion += rect.toRect();
    }
}

void SmartRenderManager::markDirty(const QRegion& region)
{
    // QMutexLocker removed - Qt event loop is single-threaded
    m_dirtyRegion += region;
}

void SmartRenderManager::markDirtyGlobal()
{
    // QMutexLocker removed - Qt event loop is single-threaded
    m_dirtyRegion = QRegion();  // 清空，表示整个场景需要更新
}

void SmartRenderManager::scheduleUpdate(QWidget* widget, const QRectF& rect)
{
    if (!widget) return;
    
    qint64 currentTime = m_globalTimer.elapsed();
    
    UpdateRequest request;
    request.widget = widget;
    request.rect = rect;
    request.timestamp = currentTime;
    request.immediate = false;
    
    // QMutexLocker removed - Qt event loop is single-threaded
    
    // 检查是否已有相同的更新请求
    auto it = std::find_if(m_pendingUpdates.begin(), m_pendingUpdates.end(),
                          [&request](const UpdateRequest& existing) {
                              return existing.widget == request.widget;
                          });
    
    if (it != m_pendingUpdates.end()) {
        // 合并更新区域
        if (rect.isValid()) {
            if (it->rect.isValid()) {
                request.rect = it->rect.united(rect);
            }
        }
        m_pendingUpdates.erase(it);
    }
    
    m_pendingUpdates.append(request);
}

void SmartRenderManager::scheduleUpdateDeferred(QWidget* widget, const QRectF& rect)
{
    if (!widget) return;
    
    qint64 currentTime = m_globalTimer.elapsed();
    
    UpdateRequest request;
    request.widget = widget;
    request.rect = rect;
    request.timestamp = currentTime;
    request.immediate = false;
    
    // QMutexLocker removed - Qt event loop is single-threaded
    m_pendingUpdates.append(request);
}

void SmartRenderManager::flushUpdates()
{
    processScheduledUpdates();
}

void SmartRenderManager::cancelUpdates(QWidget* widget)
{
    if (!widget) return;
    
    // QMutexLocker removed - Qt event loop is single-threaded
    
    auto it = m_pendingUpdates.begin();
    while (it != m_pendingUpdates.end()) {
        if (it->widget == widget) {
            it = m_pendingUpdates.erase(it);
        } else {
            ++it;
        }
    }
}

void SmartRenderManager::setMaxUpdateRate(int fps)
{
    m_maxUpdateRate = qMax(1, qMin(fps, 120));
    m_minUpdateInterval = 1000 / m_maxUpdateRate;
    m_updateTimer->setInterval(m_minUpdateInterval);
}

void SmartRenderManager::setDirtyRegionMerging(bool enabled)
{
    m_dirtyRegionMerging = enabled;
}

void SmartRenderManager::setBatchUpdateSize(int size)
{
    m_batchUpdateSize = qMax(1, size);
}

int SmartRenderManager::getCurrentFPS() const
{
    return m_currentFPS;
}

void SmartRenderManager::forceUpdateFPS()
{
    updateFPSCounter();
}

int SmartRenderManager::getPendingUpdateCount() const
{
    // const方法中不能使用QMutexLocker，简化处理
    return m_pendingUpdates.size();
}

QRegion SmartRenderManager::getDirtyRegion() const
{
    // const方法中不能使用QMutexLocker，简化处理
    return m_dirtyRegion;
}

void SmartRenderManager::enableRenderCache(bool enabled)
{
    m_renderCacheEnabled = enabled;
    if (!enabled) {
        m_renderCache.clear();
    }
}

void SmartRenderManager::clearRenderCache()
{
    // QMutexLocker removed - Qt event loop is single-threaded
    m_renderCache.clear();
}

void SmartRenderManager::processScheduledUpdates()
{
    // QMutexLocker removed - Qt event loop is single-threaded
    
    if (m_pendingUpdates.isEmpty()) {
        return;
    }
    
    // 优化更新请求
    optimizeUpdateRequests();
    
    // 限制每帧处理的更新数量
    int processedCount = 0;
    auto it = m_pendingUpdates.begin();
    
    while (it != m_pendingUpdates.end() && processedCount < m_batchUpdateSize) {
        UpdateRequest request = *it;
        it = m_pendingUpdates.erase(it);
        
        // 处理更新请求
        processUpdateRequest(request);
        processedCount++;
    }
    
    // 更新FPS计数器
    updateFPSCounter();
}

void SmartRenderManager::mergeDirtyRegions()
{
    if (!m_dirtyRegionMerging || m_dirtyRegion.isEmpty()) {
        return;
    }
    
    // 简单的区域合并逻辑
    // Qt的QRegion已经自动处理了合并
}

void SmartRenderManager::optimizeUpdateRequests()
{
    // 合并脏区域
    mergeDirtyRegions();
    
    // 这里可以添加更复杂的优化逻辑
    // 比如按Z-order排序、优先级排序等
}

void SmartRenderManager::processUpdateRequest(const UpdateRequest& request)
{
    if (!request.widget) return;
    
    // 检查是否需要更新
    if (!shouldUpdate(request.rect)) {
        return;
    }
    
    // 执行更新
    if (request.rect.isValid()) {
        request.widget->update(request.rect.toRect());
    } else {
        request.widget->update();
    }
}

bool SmartRenderManager::shouldUpdate(const QRectF& rect) const
{
    // 检查是否在脏区域内
    if (m_dirtyRegion.isEmpty()) {
        return true;  // 全局更新
    }
    
    if (rect.isValid()) {
        return m_dirtyRegion.intersects(rect.toRect());
    }
    
    return true;
}

void SmartRenderManager::updateFPSCounter()
{
    m_frameCount++;
    qint64 currentTime = m_fpsTimer.elapsed();
    
    if (currentTime - m_lastFPSUpdate >= 1000) {  // 每秒更新一次
        m_currentFPS = m_frameCount;
        m_frameCount = 0;
        m_lastFPSUpdate = currentTime;
    }
}

// SmartUpdateGuard 实现
SmartUpdateGuard::SmartUpdateGuard(QWidget* widget, const QRectF& rect, bool immediate)
    : m_widget(widget)
    , m_rect(rect)
    , m_immediate(immediate)
{
    if (m_immediate) {
        SmartRenderManager::instance().scheduleUpdate(m_widget, m_rect);
    } else {
        SmartRenderManager::instance().scheduleUpdateDeferred(m_widget, m_rect);
    }
}

SmartUpdateGuard::~SmartUpdateGuard()
{
    if (!m_immediate) {
        SmartRenderManager::instance().scheduleUpdate(m_widget, m_rect);
    }
}

// RenderProfiler 实现
RenderProfiler* RenderProfiler::s_instance = nullptr;

RenderProfiler& RenderProfiler::instance()
{
    if (!s_instance) {
        s_instance = new RenderProfiler();
    }
    return *s_instance;
}

void RenderProfiler::beginFrame()
{
    if (!m_enabled) return;
    m_frameTimer.start();
}

void RenderProfiler::endFrame()
{
    if (!m_enabled) return;
    
    double frameTime = m_frameTimer.elapsed();
    m_frameTimes.append(frameTime);
    m_frameCount++;
    
    // 保持最近100帧的数据
    if (m_frameTimes.size() > 100) {
        m_frameTimes.removeFirst();
    }
}

void RenderProfiler::beginOperation(const QString& operation)
{
    if (!m_enabled) return;
    m_operationTimers[operation].start();
}

void RenderProfiler::endOperation(const QString& operation)
{
    if (!m_enabled) return;
    
    auto it = m_operationTimers.find(operation);
    if (it != m_operationTimers.end()) {
        double time = it.value().elapsed();
        m_operationTimes[operation].append(time);
        
        // 保持最近50次的数据
        if (m_operationTimes[operation].size() > 50) {
            m_operationTimes[operation].removeFirst();
        }
    }
}

RenderProfiler::PerformanceData RenderProfiler::getPerformanceData() const
{
    PerformanceData data;
    
    if (m_frameTimes.isEmpty()) {
        return data;
    }
    
    // 计算平均帧时间
    double totalFrameTime = 0;
    for (double time : m_frameTimes) {
        totalFrameTime += time;
    }
    data.averageFrameTime = totalFrameTime / m_frameTimes.size();
    data.averageFPS = 1000.0 / data.averageFrameTime;
    data.totalFrames = m_frameCount;
    
    // 计算操作平均时间
    for (auto it = m_operationTimes.constBegin(); it != m_operationTimes.constEnd(); ++it) {
        const QList<double>& times = it.value();
        if (!times.isEmpty()) {
            double totalTime = 0;
            for (double time : times) {
                totalTime += time;
            }
            data.operationTimes[it.key()] = totalTime / times.size();
        }
    }
    
    return data;
}

void RenderProfiler::reset()
{
    m_frameTimes.clear();
    m_frameCount = 0;
    m_operationTimes.clear();
    m_operationTimers.clear();
}

void RenderProfiler::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        reset();
    }
}