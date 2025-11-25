#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QVariant>
#include <QRectF>
#include <QStringList>
#include <QVariantMap>
#include <functional>
#include <memory>
#include <typeindex>

// 前向声明
enum class ToolType : int;

/**
 * 事件总线 - 实现组件间的解耦通信
 * 提供发布-订阅模式的事件分发机制
 */
class EventBus : public QObject
{
    Q_OBJECT

public:
    static EventBus& instance();
    
    // 事件订阅
    template<typename EventType>
    void subscribe(const std::function<void(const EventType&)>& handler)
    {
        std::type_index typeIndex(typeid(EventType));
        auto handlers = m_handlers[typeIndex];
        
        // 创建新的处理器包装器
        auto wrappedHandler = [handler](const QVariant& event) {
            EventType typedEvent = event.value<EventType>();
            handler(typedEvent);
        };
        
        handlers.append(wrappedHandler);
        m_handlers[typeIndex] = handlers;
    }
    
    // 事件发布
    template<typename EventType>
    void publish(const EventType& event)
    {
        std::type_index typeIndex(typeid(EventType));
        
        QMutexLocker locker(&m_mutex);
        
        auto it = m_handlers.find(typeIndex);
        if (it != m_handlers.end()) {
            QVariant eventVariant = QVariant::fromValue(event);
            for (const auto& handler : it.value()) {
                handler(eventVariant);
            }
        }
    }
    
    // 取消订阅
    template<typename EventType>
    void unsubscribe()
    {
        std::type_index typeIndex(typeid(EventType));
        QMutexLocker locker(&m_mutex);
        m_handlers.remove(typeIndex);
    }
    
    // 清除所有订阅
    void clear();

private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    using EventHandler = std::function<void(const QVariant&)>;
    QHash<std::type_index, QList<EventHandler>> m_handlers;
    QMutex m_mutex;
};

/**
 * 工具切换事件
 */
struct ToolSwitchEvent {
    ToolType oldType;
    ToolType newType;
    QString toolName;
    qint64 switchTime;
};

/**
 * 工具激活事件
 */
struct ToolActivatedEvent {
    ToolType toolType;
    QString toolName;
    QVariantMap properties;
};

/**
 * 工具停用事件
 */
struct ToolDeactivatedEvent {
    ToolType toolType;
    QString toolName;
    QVariantMap savedState;
};

/**
 * 选择变化事件
 */
struct SelectionChangedEvent {
    int selectedCount;
    QStringList selectedTypes;
    QRectF boundingRect;
};

/**
 * 性能警告事件
 */
struct PerformanceWarningEvent {
    QString operation;
    qint64 duration;
    QString threshold;
};

Q_DECLARE_METATYPE(ToolSwitchEvent)
Q_DECLARE_METATYPE(ToolActivatedEvent)
Q_DECLARE_METATYPE(ToolDeactivatedEvent)
Q_DECLARE_METATYPE(SelectionChangedEvent)
Q_DECLARE_METATYPE(PerformanceWarningEvent)

#endif // EVENT_BUS_H