#include "tool-controller.h"
#include "mainwindow.h"
#include "../tools/tool-manager.h"

// 确保使用现有的ToolType定义

ToolController::ToolController(MainWindow *parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_toolManager(nullptr)
    , m_currentTool(ToolType::Select)
{
}

ToolController::~ToolController()
{
}

void ToolController::setToolManager(ToolManager *toolManager)
{
    m_toolManager = toolManager;
}

ToolManager *ToolController::toolManager() const
{
    return m_toolManager;
}

ToolType ToolController::currentTool() const
{
    return m_currentTool;
}

bool ToolController::isToolActive(ToolType tool) const
{
    return m_currentTool == tool;
}

void ToolController::switchTool(ToolType tool)
{
    if (!m_toolManager) {
        emit statusMessageChanged("工具管理器未初始化");
        return;
    }
    
    // 调用实际的工具管理器切换工具
    m_toolManager->switchTool(tool);
    
    ToolType oldTool = m_currentTool;
    m_currentTool = tool;
    
    if (oldTool != tool) {
        emit toolChanged(tool);
        emit statusMessageChanged(QString("已切换到 %1 工具").arg(getToolName(tool)));
    }
}

void ToolController::selectTool()
{
    switchTool(ToolType::Select);
}

void ToolController::rectangleTool()
{
    switchTool(ToolType::Rectangle);
}

void ToolController::ellipseTool()
{
    switchTool(ToolType::Ellipse);
}

void ToolController::bezierTool()
{
    switchTool(ToolType::Bezier);
}

void ToolController::nodeEditTool()
{
    switchTool(ToolType::NodeEdit);
}

void ToolController::polylineTool()
{
    switchTool(ToolType::Polyline);
}

void ToolController::polygonTool()
{
    switchTool(ToolType::Polygon);
}

void ToolController::brushTool()
{
    switchTool(ToolType::Brush);
}

void ToolController::fillTool()
{
    switchTool(ToolType::Fill);
}

void ToolController::gradientFillTool()
{
    switchTool(ToolType::GradientFill);
}

void ToolController::penTool()
{
    switchTool(ToolType::Pen);
}

void ToolController::eraserTool()
{
    switchTool(ToolType::Eraser);
}

void ToolController::lineTool()
{
    switchTool(ToolType::Line);
}

void ToolController::pathEditTool()
{
    switchTool(ToolType::PathEdit);
}

void ToolController::textTool()
{
    switchTool(ToolType::Text);
}

void ToolController::setBrushSize(int size)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置画笔大小
    emit statusMessageChanged(QString("画笔大小: %1").arg(size));
}

void ToolController::setBrushOpacity(int opacity)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置画笔透明度
    emit statusMessageChanged(QString("画笔透明度: %1%").arg(opacity));
}

void ToolController::setBrushSmoothing(int smoothing)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置画笔平滑度
    emit statusMessageChanged(QString("画笔平滑度: %1").arg(smoothing));
}

void ToolController::setEraserSize(int size)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置橡皮擦大小
    emit statusMessageChanged(QString("橡皮擦大小: %1").arg(size));
}

void ToolController::setFillTolerance(int tolerance)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置填充容差
    emit statusMessageChanged(QString("填充容差: %1").arg(tolerance));
}

void ToolController::setStrokeWidth(double width)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置描边宽度
    emit statusMessageChanged(QString("描边宽度: %1").arg(width));
}

void ToolController::setAntialiasingEnabled(bool enabled)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置抗锯齿
    emit statusMessageChanged(QString("抗锯齿: %1").arg(enabled ? "开启" : "关闭"));
}

void ToolController::setSnapToGridEnabled(bool enabled)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置网格对齐
    emit statusMessageChanged(QString("网格对齐: %1").arg(enabled ? "开启" : "关闭"));
}

void ToolController::setPenPressureSupportEnabled(bool enabled)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置压感支持
    emit statusMessageChanged(QString("压感支持: %1").arg(enabled ? "开启" : "关闭"));
}

void ToolController::setPenJoinStyle(int style)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置连接样式
    QString styleName;
    switch (style) {
        case 0: styleName = "尖角"; break;
        case 1: styleName = "圆角"; break;
        case 2: styleName = "斜角"; break;
        default: styleName = "未知"; break;
    }
    emit statusMessageChanged(QString("连接样式: %1").arg(styleName));
}

void ToolController::setPenCapStyle(int style)
{
    if (!m_toolManager) return;
    
    // TODO: 调用工具管理器设置端点样式
    QString styleName;
    switch (style) {
        case 0: styleName = "平头"; break;
        case 1: styleName = "圆头"; break;
        case 2: styleName = "方头"; break;
        default: styleName = "未知"; break;
    }
    emit statusMessageChanged(QString("端点样式: %1").arg(styleName));
}

QString ToolController::getToolName(ToolType tool) const
{
    switch (tool) {
        case ToolType::Select: return "选择";
        case ToolType::Rectangle: return "矩形";
        case ToolType::Ellipse: return "椭圆";
        case ToolType::Bezier: return "贝塞尔";
        case ToolType::NodeEdit: return "节点编辑";
        case ToolType::Polyline: return "折线";
        case ToolType::Polygon: return "多边形";
        case ToolType::Brush: return "画笔";
        case ToolType::Fill: return "填充";
        case ToolType::GradientFill: return "渐变填充";
        case ToolType::Pen: return "钢笔";
        case ToolType::Eraser: return "橡皮擦";
        case ToolType::Line: return "直线";
        case ToolType::PathEdit: return "路径编辑";
        case ToolType::Text: return "文本";
        default: return "未知";
    }
}