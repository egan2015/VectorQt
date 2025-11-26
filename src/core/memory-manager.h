#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

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
    
    // 调试功能
    void enableDebugMode(bool enable);
    void dumpMemoryLeaks();
    void dumpAllocations();
    void printExitReport();  // 程序退出时的友好报告
    void cleanup();  // 清理所有分配，避免死锁
    
private:
    std::string formatBytes(std::size_t bytes) const;  // 格式化字节数
    
    // 简单的分配大小映射，用于所有分配（不仅仅是调试模式）
    std::unordered_map<void*, std::size_t> m_sizeMap;
    
private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    
public:
    mutable std::mutex m_mutex;
    MemoryStats m_stats;
    bool m_debugMode = false;
    
private:
    
    // 调试信息
    struct AllocationInfo {
        std::size_t size;
        std::string file;
        int line;
        std::size_t sequence;
    };
    
public:
    std::unordered_map<void*, AllocationInfo> m_allocations;
    std::size_t m_sequenceCounter = 0;
    
private:
};

// 全局重载的 new 和 delete 操作符
void* operator new(std::size_t size);
void operator delete(void* ptr) noexcept;

void* operator new[](std::size_t size);
void operator delete[](void* ptr) noexcept;

// 带调试信息的 new 和 delete
void* operator new(std::size_t size, const char* file, int line);
void operator delete(void* ptr, const char* file, int line) noexcept;

void* operator new[](std::size_t size, const char* file, int line);
void operator delete[](void* ptr, const char* file, int line) noexcept;

// 调试宏
#ifdef DEBUG_MEMORY
#define NEW new(__FILE__, __LINE__)
#define DELETE delete(__FILE__, __LINE__)
#else
#define NEW new
#define DELETE delete
#endif

#endif // MEMORY_MANAGER_H