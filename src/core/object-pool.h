#pragma once

#include <QQueue>
#include <QMutex>
#include <QAtomicInt>
#include <QDebug>
#include <QDateTime>
#include <memory>
#include <functional>

/**
 * 简化的高性能对象池模板
 * 用于减少频繁的对象创建和销毁开销
 */
template<typename T>
class ObjectPool {
public:
    ObjectPool(int maxPoolSize = 50, int initialSize = 10) 
        : m_maxPoolSize(maxPoolSize)
        , m_currentSize(0)
        , m_hitCount(0)
        , m_missCount(0)
        , m_lastAdjustTime(QDateTime::currentMSecsSinceEpoch())
    {
        // 预分配初始对象
        for (int i = 0; i < initialSize && i < maxPoolSize; ++i) {
            T* obj = new T();
            resetObject(obj); // 重置对象状态
            m_pool.enqueue(obj);
            m_currentSize.ref();
        }
    }
    
    ~ObjectPool() {
        clear();
    }
    
    // 获取对象
    T* acquire() {
        QMutexLocker locker(&m_mutex);
        
        if (!m_pool.isEmpty()) {
            T* obj = m_pool.dequeue();
            m_currentSize.deref();
            m_hitCount.ref();
            
            // 定期调整池大小
            adjustPoolSize();
            
            return obj;
        }
        
        // 池为空时创建新对象
        m_missCount.ref();
        adjustPoolSize();
        return new T();
    }
    
    // 归还对象
    void release(T* obj) {
        if (!obj) return;
        
        QMutexLocker locker(&m_mutex);
        
        if (m_currentSize.loadRelaxed() < m_maxPoolSize) {
            resetObject(obj); // 重置对象状态
            m_pool.enqueue(obj);
            m_currentSize.ref();
        } else {
            // 超过池大小限制时删除对象
            delete obj;
        }
    }
    
    // 获取当前池大小
    int size() const {
        return m_currentSize.loadRelaxed();
    }
    
    // 清空对象池
    void clear() {
        QMutexLocker locker(&m_mutex);
        while (!m_pool.isEmpty()) {
            T* obj = m_pool.dequeue();
            delete obj;
            m_currentSize.deref();
        }
    }
    
    // 预热对象池
    void warmUp(int count) {
        QMutexLocker locker(&m_mutex);
        int toCreate = qMin(count, m_maxPoolSize - m_currentSize.loadRelaxed());
        
        for (int i = 0; i < toCreate; ++i) {
            T* obj = new T();
            resetObject(obj);
            m_pool.enqueue(obj);
            m_currentSize.ref();
        }
    }
    
    // 获取池命中率
    double getHitRate() const {
        qint64 total = m_hitCount.loadRelaxed() + m_missCount.loadRelaxed();
        return total > 0 ? static_cast<double>(m_hitCount.loadRelaxed()) / total : 0.0;
    }
    
    // 设置对象重置函数
    void setResetFunction(std::function<void(T*)> resetFunc) {
        m_resetFunction = resetFunc;
    }

private:
    // 重置对象状态
    void resetObject(T* obj) {
        if (m_resetFunction) {
            m_resetFunction(obj);
        }
    }
    
    // 动态调整池大小
    void adjustPoolSize() {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - m_lastAdjustTime < 5000) return; // 5秒才调整一次
        
        m_lastAdjustTime = currentTime;
        double hitRate = getHitRate();
        
        // 如果命中率低于70%，增加池大小
        if (hitRate < 0.7 && m_maxPoolSize < 200) {
            m_maxPoolSize = qMin(200, static_cast<int>(m_maxPoolSize * 1.5));
            warmUp(5); // 预热5个对象
        }
        // 如果命中率高于95%且池很大，可以适当减小
        else if (hitRate > 0.95 && m_maxPoolSize > 20) {
            m_maxPoolSize = qMax(20, static_cast<int>(m_maxPoolSize * 0.8));
        }
    }
    
    QQueue<T*> m_pool;
    QMutex m_mutex;
    int m_maxPoolSize;
    QAtomicInt m_currentSize;
    QAtomicInt m_hitCount;
    QAtomicInt m_missCount;
    qint64 m_lastAdjustTime;
    std::function<void(T*)> m_resetFunction;
};

/**
 * 智能指针包装器，自动归还到对象池
 */
template<typename T>
class PooledObject {
public:
    PooledObject(ObjectPool<T>* pool, T* obj)
        : m_pool(pool), m_obj(obj)
    {
    }
    
    ~PooledObject() {
        if (m_pool && m_obj) {
            m_pool->release(m_obj);
        }
    }
    
    // 移动构造函数
    PooledObject(PooledObject&& other) noexcept
        : m_pool(other.m_pool), m_obj(other.m_obj)
    {
        other.m_pool = nullptr;
        other.m_obj = nullptr;
    }
    
    // 移动赋值操作符
    PooledObject& operator=(PooledObject&& other) noexcept {
        if (this != &other) {
            if (m_obj && m_pool) {
                m_pool->release(m_obj);
            }
            m_pool = other.m_pool;
            m_obj = other.m_obj;
            other.m_pool = nullptr;
            other.m_obj = nullptr;
        }
        return *this;
    }
    
    // 禁止拷贝
    PooledObject(const PooledObject&) = delete;
    PooledObject& operator=(const PooledObject&) = delete;
    
    // 获取原始指针
    T* get() const { return m_obj; }
    T* operator->() const { return m_obj; }
    T& operator*() const { return *m_obj; }
    
    // 检查是否有效
    bool isValid() const { return m_obj != nullptr; }
    
    // 释放所有权（不归还到池中）
    T* release() {
        T* obj = m_obj;
        m_obj = nullptr;
        return obj;
    }

private:
    ObjectPool<T>* m_pool;
    T* m_obj;
};

/**
 * 全局对象池管理器
 */
class GlobalObjectPoolManager {
public:
    static GlobalObjectPoolManager& instance();
    
    // 获取特定类型的对象池
    template<typename T>
    ObjectPool<T>* getPool(const QString& poolName = QString()) {
        QString key = poolName.isEmpty() ? QString::fromUtf8(typeid(T).name()) : poolName;
        
        QMutexLocker locker(&m_mutex);
        
        auto it = m_pools.find(key);
        if (it == m_pools.end()) {
            // 创建新的对象池
            ObjectPool<T>* pool = new ObjectPool<T>();
            m_pools[key] = static_cast<void*>(pool);
            return pool;
        }
        
        return static_cast<ObjectPool<T>*>(it.value());
    }
    
    // 清理所有对象池
    void clearAllPools();

private:
    GlobalObjectPoolManager() = default;
    ~GlobalObjectPoolManager() = default;
    
    QHash<QString, void*> m_pools;  // 使用void*存储不同类型的池
    QMutex m_mutex;
};

// 便利宏
#define ACQUIRE_FROM_POOL(Type) \
    PooledObject<Type> __pooledObj(GlobalObjectPoolManager::instance().getPool<Type>()->acquire()); \
    Type& obj = *__pooledObj