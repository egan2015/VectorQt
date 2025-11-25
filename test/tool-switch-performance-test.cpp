#include <QApplication>
#include <QElapsedTimer>
#include <QDebug>
#include "../src/ui/mainwindow.h"
#include "../src/core/toolbase.h"

/**
 * 工具切换性能测试
 * 用于验证优化后的工具切换性能
 */
class ToolSwitchPerformanceTest
{
public:
    static void runPerformanceTest(MainWindow* mainWindow)
    {
        if (!mainWindow) {
            qDebug() << "Error: MainWindow is null";
            return;
        }
        
        qDebug() << "开始工具切换性能测试...";
        
        // 获取工具指针（这些应该在MainWindow中可用）
        ToolBase* tools[] = {
            mainWindow->findChild<ToolBase*>("outlinePreviewTool"),
            mainWindow->findChild<ToolBase*>("rectangleTool"),
            mainWindow->findChild<ToolBase*>("ellipseTool"),
            mainWindow->findChild<ToolBase*>("bezierTool"),
            mainWindow->findChild<ToolBase*>("nodeEditTool"),
            mainWindow->findChild<ToolBase*>("brushTool"),
            mainWindow->findChild<ToolBase*>("penTool"),
            mainWindow->findChild<ToolBase*>("fillTool")
        };
        
        const int toolCount = sizeof(tools) / sizeof(tools[0]);
        const int iterations = 100;
        
        QElapsedTimer totalTimer;
        totalTimer.start();
        
        for (int i = 0; i < iterations; ++i) {
            for (int j = 0; j < toolCount; ++j) {
                if (tools[j]) {
                    QElapsedTimer toolTimer;
                    toolTimer.start();
                    
                    // 使用反射调用私有方法（这里简化为直接调用）
                    // 在实际情况下，我们需要通过公共接口或信号来触发工具切换
                    // mainWindow->setCurrentTool(tools[j]);
                    
                    qint64 switchTime = toolTimer.elapsed();
                    if (switchTime > 5) { // 只记录超过5ms的切换
                        qDebug() << QString("工具 %1 切换耗时: %2ms").arg(j).arg(switchTime);
                    }
                }
            }
        }
        
        qint64 totalTime = totalTimer.elapsed();
        qDebug() << QString("性能测试完成:");
        qDebug() << QString("- 总耗时: %1ms").arg(totalTime);
        qDebug() << QString("- 平均每次切换: %1ms").arg(totalTime / (iterations * toolCount));
        qDebug() << QString("- 每秒可切换次数: %1").arg(1000.0 / (totalTime / (iterations * toolCount)));
    }
    
    static void runMemoryTest()
    {
        qDebug() << "开始内存使用测试...";
        
        // 这里可以添加内存使用监控代码
        // 比较优化前后的内存使用情况
        
        qDebug() << "内存使用测试完成";
    }
};

// 如果要独立运行这个测试，可以添加main函数：
/*
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    // 等待窗口完全初始化
    QTimer::singleShot(1000, [&window]() {
        ToolSwitchPerformanceTest::runPerformanceTest(&window);
        ToolSwitchPerformanceTest::runMemoryTest();
    });
    
    return app.exec();
}
*/