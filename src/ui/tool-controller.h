#ifndef TOOL_CONTROLLER_H
#define TOOL_CONTROLLER_H

#include <QObject>

class MainWindow;
class ToolManager;

// 使用现有的ToolType枚举定义
#include "../tools/tool-manager.h"

class ToolController : public QObject
{
    Q_OBJECT

public:
    explicit ToolController(MainWindow *parent = nullptr);
    ~ToolController();

    void setToolManager(ToolManager *toolManager);
    ToolManager *toolManager() const;

    // 获取当前工具
    ToolType currentTool() const;
    bool isToolActive(ToolType tool) const;

signals:
    void toolChanged(ToolType newTool);
    void statusMessageChanged(const QString &message);

public slots:
    // 工具切换
    void switchTool(ToolType tool);
    
    // 具体工具切换方法
    void selectTool();
    void rectangleTool();
    void ellipseTool();
    void bezierTool();
    void nodeEditTool();
    void polylineTool();
    void polygonTool();
    void brushTool();
    void fillTool();
    void gradientFillTool();
    void penTool();
    void eraserTool();
    void lineTool();
    void pathEditTool();
    void textTool();
    
    // 工具参数设置
    void setBrushSize(int size);
    void setBrushOpacity(int opacity);
    void setBrushSmoothing(int smoothing);
    void setEraserSize(int size);
    void setFillTolerance(int tolerance);
    void setStrokeWidth(double width);
    void setAntialiasingEnabled(bool enabled);
    void setSnapToGridEnabled(bool enabled);
    void setPenPressureSupportEnabled(bool enabled);
    void setPenJoinStyle(int style);
    void setPenCapStyle(int style);

private:
    QString getToolName(ToolType tool) const;

private:
    MainWindow *m_mainWindow;
    ToolManager *m_toolManager;
    ToolType m_currentTool;
};

#endif // TOOL_CONTROLLER_H