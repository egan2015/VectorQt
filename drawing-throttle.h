#ifndef DRAWING_THROTTLE_H
#define DRAWING_THROTTLE_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointF>
#include <QQueue>
#include <functional>

/**
 * 绘图事件节流器
 * 用于优化鼠标移动事件的处理频率，提高绘图流畅度
 */
class DrawingThrottle : public QObject
{
    Q_OBJECT

public:
    explicit DrawingThrottle(QObject *parent = nullptr);
    ~DrawingThrottle();

    // 设置节流参数
    void setThrottleInterval(int milliseconds);  // 节流间隔（毫秒）
    void setMaxPendingEvents(int maxEvents);     // 最大待处理事件数
    void setDistanceThreshold(qreal threshold);  // 距离阈值

    // 节流处理鼠标移动事件
    void throttleMouseMove(const QPointF& scenePos, std::function<void(const QPointF&)> callback);

    // 立即处理所有待处理事件（用于鼠标释放时）
    void flushPendingEvents();

    // 清除所有待处理事件
    void clearPendingEvents();

    // 获取当前状态
    bool hasPendingEvents() const;
    int pendingEventCount() const;

private slots:
    void processPendingEvents();

private:
    struct PendingEvent {
        QPointF position;
        std::function<void(const QPointF&)> callback;
        qint64 timestamp;
    };

    QTimer* m_throttleTimer;
    QElapsedTimer m_elapsedTimer;
    QQueue<PendingEvent> m_pendingEvents;
    
    int m_throttleInterval;      // 节流间隔（毫秒）
    int m_maxPendingEvents;      // 最大待处理事件数
    qreal m_distanceThreshold;   // 距离阈值
    QPointF m_lastProcessedPos;  // 最后处理的位置
    qint64 m_lastProcessTime;    // 最后处理时间
    bool m_isProcessing;         // 是否正在处理事件

    // 智能合并相近的事件点
    void mergeNearbyEvents();
    
    // 检查距离是否超过阈值
    bool shouldProcessPoint(const QPointF& point) const;
};

#endif // DRAWING_THROTTLE_H