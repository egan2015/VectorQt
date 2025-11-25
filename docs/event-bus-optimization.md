# Event-Bus优化方案

## 当前问题
1. Event-Bus实现完成但几乎未使用
2. 与Qt信号槽形成双重通信机制
3. 缺乏统一的事件发布策略

## 优化方案

### 1. 统一事件发布策略
- 在ToolManager中发布工具切换事件
- 在Scene中发布选择变化事件
- 在性能监控中发布警告事件

### 2. 替换部分信号槽
将以下信号槽替换为Event-Bus：
- 工具切换通知
- 选择变化通知
- 性能警告通知

### 3. 保留Qt信号槽的场景
- UI控件的直接交互
- 需要Qt元对象系统的场景
- 跨线程通信

### 4. 实现建议
```cpp
// 在ToolManager::switchTool中添加
EventBus::instance().publish(ToolSwitchEvent{
    oldType, newType, toolName, switchTime
});

// 在Scene中发布选择事件
EventBus::instance().publish(SelectionChangedEvent{
    selectedCount, selectedTypes, boundingRect
});
```