#include "../core/drawing-throttle.h"
#include <QDebug>
#include <QtMath>
#include <QLineF>

DrawingThrottle::DrawingThrottle(QObject *parent)
    : QObject(parent)
    , m_throttleInterval(16)  // 默认16ms (约60fps)
    , m_maxPendingEvents(10)  // 最多缓存10个事件
    , m_distanceThreshold(1.0) // 默认1像素阈值
    , m_lastProcessTime(0)
    , m_isProcessing(false)
{
    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setSingleShot(true);
    connect(m_throttleTimer, &QTimer::timeout, this, &DrawingThrottle::processPendingEvents);
    
    m_elapsedTimer.start();
}

DrawingThrottle::~DrawingThrottle()
{
    clearPendingEvents();
}

void DrawingThrottle::setThrottleInterval(int milliseconds)
{
    m_throttleInterval = qMax(1, milliseconds);
}

void DrawingThrottle::setMaxPendingEvents(int maxEvents)
{
    m_maxPendingEvents = qMax(1, maxEvents);
}

void DrawingThrottle::setDistanceThreshold(qreal threshold)
{
    m_distanceThreshold = qMax(0.0, threshold);
}

void DrawingThrottle::throttleMouseMove(const QPointF& scenePos, std::function<void(const QPointF&)> callback)
{
    qint64 currentTime = m_elapsedTimer.elapsed();
    
    // 检查是否需要处理这个点
    if (!shouldProcessPoint(scenePos)) {
        return; // 跳过太近的点
    }
    
    // 创建待处理事件
    PendingEvent event;
    event.position = scenePos;
    event.callback = callback;
    event.timestamp = currentTime;
    
    // 添加到队列
    m_pendingEvents.enqueue(event);
    
    // 限制队列大小，移除最旧的事件
    while (m_pendingEvents.size() > m_maxPendingEvents) {
        m_pendingEvents.dequeue();
    }
    
    // 如果定时器未运行，启动它
    if (!m_throttleTimer->isActive()) {
        m_throttleTimer->start(m_throttleInterval);
    }
}

void DrawingThrottle::flushPendingEvents()
{
    if (m_throttleTimer->isActive()) {
        m_throttleTimer->stop();
    }
    processPendingEvents();
}

void DrawingThrottle::clearPendingEvents()
{
    m_pendingEvents.clear();
    if (m_throttleTimer->isActive()) {
        m_throttleTimer->stop();
    }
    m_isProcessing = false;
}

bool DrawingThrottle::hasPendingEvents() const
{
    return !m_pendingEvents.isEmpty();
}

int DrawingThrottle::pendingEventCount() const
{
    return m_pendingEvents.size();
}

void DrawingThrottle::processPendingEvents()
{
    if (m_isProcessing || m_pendingEvents.isEmpty()) {
        return;
    }
    
    m_isProcessing = true;
    
    // 合并相近的事件点
    mergeNearbyEvents();
    
    // 处理所有待处理事件
    while (!m_pendingEvents.isEmpty()) {
        PendingEvent event = m_pendingEvents.dequeue();
        
        // 更新最后处理的位置和时间
        m_lastProcessedPos = event.position;
        m_lastProcessTime = event.timestamp;
        
        // 执行回调
        if (event.callback) {
            event.callback(event.position);
        }
    }
    
    m_isProcessing = false;
}

void DrawingThrottle::mergeNearbyEvents()
{
    if (m_pendingEvents.size() <= 1) {
        return;
    }
    
    QQueue<PendingEvent> mergedEvents;
    QPointF lastPos = m_pendingEvents.head().position;
    qint64 lastTime = m_pendingEvents.head().timestamp;
    std::function<void(const QPointF&)> lastCallback = m_pendingEvents.head().callback;
    
    for (int i = 1; i < m_pendingEvents.size(); ++i) {
        const PendingEvent& current = m_pendingEvents[i];
        qreal distance = QLineF(lastPos, current.position).length();
        
        // 如果距离超过阈值，保存前一个点
        if (distance > m_distanceThreshold * 2.0) {
            PendingEvent merged;
            merged.position = lastPos;
            merged.callback = lastCallback;
            merged.timestamp = lastTime;
            mergedEvents.enqueue(merged);
            
            lastPos = current.position;
            lastTime = current.timestamp;
            lastCallback = current.callback;
        } else {
            // 合并到最新的位置
            lastPos = current.position;
            lastTime = current.timestamp;
            lastCallback = current.callback;
        }
    }
    
    // 添加最后一个点
    PendingEvent last;
    last.position = lastPos;
    last.callback = lastCallback;
    last.timestamp = lastTime;
    mergedEvents.enqueue(last);
    
    m_pendingEvents = mergedEvents;
}

bool DrawingThrottle::shouldProcessPoint(const QPointF& point) const
{
    // 第一个点总是要处理
    if (m_lastProcessTime == 0) {
        return true;
    }
    
    // 检查时间间隔
    qint64 currentTime = m_elapsedTimer.elapsed();
    if (currentTime - m_lastProcessTime < m_throttleInterval / 2) {
        // 如果时间间隔太短，检查距离
        qreal distance = QLineF(m_lastProcessedPos, point).length();
        return distance > m_distanceThreshold * 3.0; // 更严格的距离检查
    }
    
    // 检查距离阈值
    qreal distance = QLineF(m_lastProcessedPos, point).length();
    return distance > m_distanceThreshold;
}