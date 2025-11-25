# Qt信号槽与Event-Bus混合通信策略

## 核心原则
**Qt信号槽为主，Event-Bus为辅，明确分工，避免冲突**

## 分工策略

### 🎯 **Qt信号槽负责的场景**
1. **UI组件间直接通信**
   - 界面控件交互
   - 用户操作响应
   - 视图更新通知

2. **对象生命周期管理**
   - 父子对象通信
   - 对象创建/销毁通知
   - 状态同步

3. **需要Qt元对象系统的场景**
   - QML集成
   - 跨线程通信
   - 信号/槽的自动连接

4. **紧耦合组件通信**
   - 工具与场景的交互
   - 图形与图层的通信
   - 视图与模型的绑定

### 📡 **Event-Bus负责的场景**
1. **松耦合组件通知**
   - 性能警告广播
   - 全局状态变化通知
   - 插件间通信

2. **一对多广播**
   - 工具切换通知（多个UI组件需要知道）
   - 设置变更通知
   - 主题切换通知

3. **调试和监控**
   - 性能数据收集
   - 操作日志记录
   - 错误报告

4. **扩展性需求**
   - 未来插件系统
   - 脚本引擎集成
   - 远程控制接口

## 具体实施

### 1. 保留Qt信号槽的例子
```cpp
// 工具切换 - 保持信号槽（紧耦合）
class ToolManager : public QObject {
    Q_SIGNAL void toolChanged(ToolBase* oldTool, ToolBase* newTool);
    Q_SIGNAL void toolTypeChanged(ToolType oldType, ToolType newType);
};

// 选择变化 - 保持信号槽（UI直接相关）
class DrawingScene : public QGraphicsScene {
    Q_SIGNAL void selectionChanged();
    Q_SIGNAL void objectStateChanged(DrawingShape* shape);
};
```

### 2. 使用Event-Bus的例子
```cpp
// 性能警告 - 使用Event-Bus（松耦合）
EventBus::instance().publish(PerformanceWarningEvent{
    "ToolSwitch", switchTime, "100ms"
});

// 全局设置变更 - 使用Event-Bus（一对多）
EventBus::instance().publish(SettingsChangedEvent{
    "Theme", "Dark", {"ui", "colors", "icons"}
});
```

### 3. 混合使用策略
```cpp
class ToolManager : public QObject {
    void switchTool(ToolType type) {
        // 1. 紧耦合通信用信号槽
        emit toolChanged(oldTool, newTool);
        
        // 2. 松耦合通知用Event-Bus
        EventBus::instance().publish(ToolSwitchEvent{
            oldType, newType, toolName, switchTime
        });
        
        // 3. 性能监控用Event-Bus
        if (switchTime > 100) {
            EventBus::instance().publish(PerformanceWarningEvent{
                "ToolSwitch", switchTime, "100ms"
            });
        }
    }
};
```

## 迁移计划

### 阶段1：清理现有Event-Bus
1. 移除MainWindow中未使用的Event-Bus订阅
2. 保留Event-Bus但明确其使用场景
3. 添加使用注释和文档

### 阶段2：优化信号槽使用
1. 审查现有信号槽，去除冗余
2. 统一信号命名规范
3. 添加信号使用文档

### 阶段3：逐步引入Event-Bus
1. 在性能监控中使用Event-Bus
2. 在全局设置中使用Event-Bus
3. 为未来扩展预留Event-Bus接口

## 最佳实践

### ✅ **推荐做法**
1. **优先使用Qt信号槽**，除非有明确理由
2. **明确通信模式**：紧耦合用信号槽，松耦合用Event-Bus
3. **文档化决策**：在代码中注释为什么选择某种机制
4. **保持一致性**：同类功能使用相同通信机制

### ❌ **避免做法**
1. **双重实现**：同一个功能同时用两套机制
2. **过度设计**：简单场景强行使用Event-Bus
3. **混用混乱**：没有明确规则随意选择机制
4. **忽略性能**：不考虑性能影响盲目使用Event-Bus

## 结论

Event-Bus不是必需品，但可以作为Qt信号槽的补充。关键是：
1. **明确分工**，避免冲突
2. **保持简单**，不要过度设计
3. **文档化决策**，便于维护

这样可以既享受Qt信号槽的便利，又为未来扩展预留空间。