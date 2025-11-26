#include "memory-manager.h"
#include <iostream>
#include <iomanip>

MemoryManager& MemoryManager::instance() {
    static MemoryManager instance;
    return instance;
}

void* MemoryManager::allocate(std::size_t size) {
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    
    return ptr;
}

void MemoryManager::deallocate(void* ptr) {
    if (!ptr) return;
    std::free(ptr);
}

MemoryManager::MemoryStats MemoryManager::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void MemoryManager::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = MemoryStats{};
    m_sequenceCounter = 0;
}

void MemoryManager::enableDebugMode(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_debugMode = enable;
}

void MemoryManager::dumpMemoryLeaks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_allocations.empty()) {
        std::cout << "No memory leaks detected\n";
        return;
    }
    
    std::cout << "=== Memory Leak Report ===\n";
    std::cout << "Number of leaked memory blocks: " << m_allocations.size() << "\n";
    std::cout << "Total leaked memory: " << m_stats.currentUsage << " bytes\n\n";
    
    for (const auto& pair : m_allocations) {
        const auto& info = pair.second;
        std::cout << "Address: " << pair.first 
                  << ", Size: " << info.size 
                  << " bytes"
                  << ", Sequence: " << info.sequence;
        
        if (!info.file.empty()) {
            std::cout << ", File: " << info.file << ":" << info.line;
        }
        std::cout << "\n";
    }
    std::cout << "==========================\n";
}

void MemoryManager::dumpAllocations() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cout << "=== Memory Allocation Statistics ===\n";
    std::cout << "Total allocated: " << m_stats.totalAllocated << " bytes\n";
    std::cout << "Total deallocated: " << m_stats.totalDeallocated << " bytes\n";
    std::cout << "Current usage: " << m_stats.currentUsage << " bytes\n";
    std::cout << "Peak usage: " << m_stats.peakUsage << " bytes\n";
    std::cout << "Allocation count: " << m_stats.allocationCount << "\n";
    std::cout << "Deallocation count: " << m_stats.deallocationCount << "\n";
    std::cout << "Active allocations: " << m_allocations.size() << "\n";
    std::cout << "====================================\n";
}

void MemoryManager::printExitReport() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cout << "\n=== Memory Statistics ===\n";
    
    // Memory usage statistics
    std::cout << "Memory Usage:\n";
    std::cout << "  Total allocated: " << formatBytes(m_stats.totalAllocated) << "\n";
    std::cout << "  Total deallocated: " << formatBytes(m_stats.totalDeallocated) << "\n";
    std::cout << "  Current usage: " << formatBytes(m_stats.currentUsage) << "\n";
    std::cout << "  Peak usage: " << formatBytes(m_stats.peakUsage) << "\n";
    
    // Operation statistics
    std::cout << "Operations:\n";
    std::cout << "  Allocations: " << m_stats.allocationCount << "\n";
    std::cout << "  Deallocations: " << m_stats.deallocationCount << "\n";
    std::cout << "  Active allocations: " << m_allocations.size() << "\n";
    
    // Memory health status
    bool hasLeaks = m_stats.currentUsage > 0 || !m_allocations.empty();
    if (hasLeaks) {
        std::cout << "Status: Memory leaks detected\n";
        std::cout << "  Leaked memory: " << formatBytes(m_stats.currentUsage) << "\n";
        std::cout << "  Leaked blocks: " << m_allocations.size() << "\n";
        
        // Leak details
        if (!m_allocations.empty()) {
            std::cout << "Leak Details:\n";
            int leakCount = 0;
            for (const auto& pair : m_allocations) {
                const auto& info = pair.second;
                leakCount++;
                std::cout << "  [" << leakCount << "] ";
                std::cout << "Address: " << pair.first;
                std::cout << ", Size: " << formatBytes(info.size);
                
                if (m_debugMode && !info.file.empty()) {
                    std::cout << ", Location: " << info.file << ":" << info.line;
                }
                std::cout << "\n";
            }
        }
    } else {
        std::cout << "Status: No memory leaks\n";
    }
    
    std::cout << "========================\n\n";
}

void MemoryManager::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 清理所有未释放的内存
    for (auto& pair : m_sizeMap) {
        std::free(pair.first);
    }
    
    // 清空所有映射
    m_sizeMap.clear();
    m_allocations.clear();
    
    // 重置统计
    m_stats = MemoryStats{};
}

// 辅助函数：格式化字节数为人类可读格式
std::string MemoryManager::formatBytes(std::size_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 3) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

// 全局 new 和 delete 操作符实现
void* operator new(std::size_t size) {
    return MemoryManager::instance().allocate(size);
}

void operator delete(void* ptr) noexcept {
    MemoryManager::instance().deallocate(ptr);
}

void* operator new[](std::size_t size) {
    return MemoryManager::instance().allocate(size);
}

void operator delete[](void* ptr) noexcept {
    MemoryManager::instance().deallocate(ptr);
}

// 带调试信息的版本
void* operator new(std::size_t size, const char* file, int line) {
    void* ptr = MemoryManager::instance().allocate(size);
    
    // 如果启用了调试模式，记录文件信息
    if (auto& manager = MemoryManager::instance(); true) {
        std::lock_guard<std::mutex> lock(manager.m_mutex);
        if (manager.m_debugMode) {
            auto it = manager.m_allocations.find(ptr);
            if (it != manager.m_allocations.end()) {
                it->second.file = file;
                it->second.line = line;
            }
        }
    }
    
    return ptr;
}

void operator delete(void* ptr, const char* file, int line) noexcept {
    (void)file; // 避免未使用参数警告
    (void)line;
    MemoryManager::instance().deallocate(ptr);
}

void* operator new[](std::size_t size, const char* file, int line) {
    return operator new(size, file, line);
}

void operator delete[](void* ptr, const char* file, int line) noexcept {
    (void)file; // 避免未使用参数警告
    (void)line;
    operator delete(ptr);
}