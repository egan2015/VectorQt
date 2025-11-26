#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstddef>
#include <memory>

class MemoryManager {
public:
    static MemoryManager& instance();
    
    void* allocate(std::size_t size);
    void deallocate(void* ptr);
    
    // 内存统计信息
    struct MemoryStats {
        std::size_t totalAllocated = 0;
        std::size_t totalDeallocated = 0;
        std::size_t currentUsage = 0;
        std::size_t peakUsage = 0;
        std::size_t allocationCount = 0;
        std::size_t deallocationCount = 0;
        
        
    };
    
    MemoryStats getStats() const;
    void resetStats();
    
    // 内存泄漏检测
    void dumpMemoryLeaks();
    void printExitReport();  // 程序退出时的友好报告
    void cleanup();  // 清理所有分配
    

    
    
    
private:
    MemoryManager();
    ~MemoryManager();
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    
    
};

// 全局重载的 new 和 delete 操作符
void* operator new(std::size_t size);
void operator delete(void* ptr) noexcept;

void* operator new[](std::size_t size);
void operator delete[](void* ptr) noexcept;

#endif // MEMORY_MANAGER_H