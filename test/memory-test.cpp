#include "memory-manager.h"
#include <iostream>
#include <vector>
#include <string>

void testBasicAllocation() {
    std::cout << "=== 测试基本内存分配 ===\n";
    
    // 测试基本分配和释放
    int* intPtr = new int(42);
    std::cout << "分配整数: " << *intPtr << "\n";
    delete intPtr;
    
    // 测试数组分配
    int* arrayPtr = new int[10];
    for (int i = 0; i < 10; ++i) {
        arrayPtr[i] = i;
    }
    std::cout << "分配数组: " << arrayPtr[5] << "\n";
    delete[] arrayPtr;
    
    // 测试对象分配
    std::string* strPtr = new std::string("Hello, Memory Manager!");
    std::cout << "分配字符串: " << *strPtr << "\n";
    delete strPtr;
    
    std::cout << "\n";
}

void testMemoryStats() {
    std::cout << "=== 测试内存统计 ===\n";
    
    auto& manager = MemoryManager::instance();
    manager.resetStats();
    manager.enableDebugMode(true);
    
    // 分配一些内存
    std::vector<int*> pointers;
    for (int i = 0; i < 3; ++i) {
        pointers.push_back(new int[10]); // 减少分配大小
    }
    
    // 显示简化的统计信息
    auto stats = manager.getStats();
    std::cout << "分配次数: " << stats.allocationCount << "\n";
    std::cout << "当前使用: " << stats.currentUsage << " 字节\n";
    
    // 释放部分内存
    for (int i = 0; i < 2; ++i) {
        delete[] pointers[i];
    }
    
    stats = manager.getStats();
    std::cout << "释放部分后使用: " << stats.currentUsage << " 字节\n";
    
    // 清理剩余内存
    delete[] pointers[2];
    
    stats = manager.getStats();
    std::cout << "全部释放后使用: " << stats.currentUsage << " 字节\n";
    
    manager.enableDebugMode(false);
    std::cout << "\n";
}

void testMemoryLeaks() {
    std::cout << "=== 测试内存泄漏检测 ===\n";
    
    auto& manager = MemoryManager::instance();
    manager.resetStats();
    manager.enableDebugMode(true);
    
    // 故意制造少量内存泄漏
    new int(123);
    new double(456.789);
    
    // 显示简化的泄漏报告
    auto stats = manager.getStats();
    std::cout << "分配次数: " << stats.allocationCount << "\n";
    std::cout << "当前使用: " << stats.currentUsage << " 字节\n";
    
    // 注意：这里故意不释放内存来演示泄漏检测
    std::cout << "注意：上述内存泄漏是故意制造的用于测试\n\n";
}

void testLargeAllocations() {
    std::cout << "=== 测试大内存分配 ===\n";
    
    auto& manager = MemoryManager::instance();
    manager.resetStats();
    
    // 分配较小的大块内存用于测试
    const size_t largeSize = 1024; // 1KB
    char* largeBlock = new char[largeSize];
    
    std::cout << "分配了 " << largeSize << " 字节的内存块\n";
    auto stats = manager.getStats();
    std::cout << "当前使用: " << stats.currentUsage << " 字节\n";
    
    delete[] largeBlock;
    
    stats = manager.getStats();
    std::cout << "释放后使用: " << stats.currentUsage << " 字节\n";
    std::cout << "\n";
}

int main() {
    std::cout << "开始内存管理器测试...\n\n";
    
    try {
        testBasicAllocation();
        testMemoryStats();
        testMemoryLeaks();
        testLargeAllocations();
        
        std::cout << "所有测试完成！\n";
        
        // 最终内存泄漏报告
        auto& manager = MemoryManager::instance();
        std::cout << "\n=== 最终内存状态 ===\n";
        auto finalStats = manager.getStats();
        std::cout << "总分配次数: " << finalStats.allocationCount << "\n";
        std::cout << "总释放次数: " << finalStats.deallocationCount << "\n";
        std::cout << "当前使用: " << finalStats.currentUsage << " 字节\n";
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}