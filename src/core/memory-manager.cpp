#include <iostream>
#include "memory-manager.h"

MemoryManager::MemoryManager()
{
}

MemoryManager::~MemoryManager()
{
    cleanup();
}

MemoryManager &MemoryManager::instance()
{
    static MemoryManager instance;
    return instance;
}

void *MemoryManager::allocate(std::size_t size)
{
    // 使用系统默认分配器
    void *ptr = std::malloc(size);
    if (!ptr)
    {
        throw std::bad_alloc();
    }

    return ptr;
}

void MemoryManager::deallocate(void *ptr)
{
    if (!ptr)
        return;

    // 直接释放到系统
    std::free(ptr);
}

MemoryManager::MemoryStats MemoryManager::getStats() const
{
    return MemoryStats{};
}

void MemoryManager::resetStats()
{
}

void MemoryManager::dumpMemoryLeaks()
{
    std::cout << "Memory leak detection disabled\n";
}
void MemoryManager::printExitReport()
{
    std::cout << "\n=== Memory Manager ===\n";
    std::cout << "Type: System default allocator\n";
    std::cout << "Monitoring: Disabled\n";
    std::cout << "======================\n\n";
}

void MemoryManager::cleanup()
{
}

// 全局 new 和 delete 操作符实现
void *operator new(std::size_t size)
{
    return MemoryManager::instance().allocate(size);
}

void operator delete(void *ptr) noexcept
{
    MemoryManager::instance().deallocate(ptr);
}

void *operator new[](std::size_t size)
{
    return MemoryManager::instance().allocate(size);
}

void operator delete[](void *ptr) noexcept
{
    MemoryManager::instance().deallocate(ptr);
}

// 带调试信息的版本
// 带调试信息的版本（已移除）