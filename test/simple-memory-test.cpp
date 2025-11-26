#include "memory-manager.h"
#include <iostream>

int main() {
    std::cout << "开始简单内存测试...\n";
    
    auto& manager = MemoryManager::instance();
    manager.resetStats();
    
    // 基本分配测试
    int* ptr = new int(42);
    std::cout << "分配整数成功: " << *ptr << "\n";
    
    auto stats = manager.getStats();
    std::cout << "分配次数: " << stats.allocationCount << "\n";
    std::cout << "当前使用: " << stats.currentUsage << " 字节\n";
    
    delete ptr;
    
    stats = manager.getStats();
    std::cout << "释放后使用: " << stats.currentUsage << " 字节\n";
    
    std::cout << "测试完成！\n";
    return 0;
}