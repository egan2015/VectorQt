#include "object-pool.h"

GlobalObjectPoolManager& GlobalObjectPoolManager::instance()
{
    static GlobalObjectPoolManager instance;
    return instance;
}

void GlobalObjectPoolManager::clearAllPools()
{
    QMutexLocker locker(&m_mutex);
    // 注意：这里简化处理，实际使用中需要更好的类型管理
    m_pools.clear();
}