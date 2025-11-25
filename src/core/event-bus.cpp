#include "event-bus.h"
#include <QMutexLocker>
#include <QCoreApplication>

EventBus& EventBus::instance()
{
    static EventBus instance;
    return instance;
}

void EventBus::clear()
{
    QMutexLocker locker(&m_mutex);
    m_handlers.clear();
}