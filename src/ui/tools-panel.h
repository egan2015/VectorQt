#ifndef TOOLS_PANEL_H
#define TOOLS_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>

class ToolBase;
class DrawingScene;

/**
 * 工具面板 - 提供当前工具的设置和选项
 */
class ToolsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ToolsPanel(QWidget *parent = nullptr);
    
    // 设置场景
    void setScene(DrawingScene *scene);
    void setCurrentTool(ToolBase *tool);
    void updateToolSettings();
    
    // 更新控件值
    void updateBrushSettings(ToolBase *tool);
    void updatePenSettings(ToolBase *tool);
    void updateEraserSettings(ToolBase *tool);
    void updateFillSettings(ToolBase *tool);

signals:
    void toolSettingsChanged();
    void brushSizeChanged(int size);
    void brushOpacityChanged(int opacity);
    void brushSmoothingChanged(int smoothing);
    void eraserSizeChanged(int size);
    void fillToleranceChanged(int tolerance);
    void strokeWidthChanged(double width);
    void antialiasingChanged(bool enabled);
    void snapToGridChanged(bool enabled);
    void penPressureSupportChanged(bool enabled);
    void penJoinStyleChanged(int style);
    void penCapStyleChanged(int style);

private slots:
    void onBrushSizeChanged(int size);
    void onOpacityChanged(int value);
    void onSmoothingChanged(int value);
    void onFillToleranceChanged(int value);
    void onStrokeWidthChanged(double width);
    void onMiterLimitChanged(double limit);
    void onJoinStyleChanged(int style);
    void onCapStyleChanged(int style);
    void onAntialiasingToggled(bool enabled);
    void onSnapToGridToggled(bool enabled);
    void onResetSettings();

private:
    void setupUI();
    void setupBrushSettings();
    void setupPenSettings();
    void setupEraserSettings();
    void setupFillSettings();
    void setupShapeSettings();
    void setupGeneralSettings();
    void clearAllGroups();
    void showBrushSettings();
    void showPenSettings();
    void showEraserSettings();
    void showFillSettings();
    void showShapeSettings();
    void showGeneralSettings();
    
    DrawingScene *m_scene;
    ToolBase *m_currentTool;
    
    // 布局
    QVBoxLayout *m_mainLayout;
    
    // 通用设置组
    QGroupBox *m_generalGroup;
    QCheckBox *m_antialiasingCheck;
    QCheckBox *m_snapToGridCheck;
    QPushButton *m_resetButton;
    
    // 画笔设置组
    QGroupBox *m_brushGroup;
    QSlider *m_brushSizeSlider;
    QLabel *m_brushSizeLabel;
    QSlider *m_opacitySlider;
    QLabel *m_opacityLabel;
    QSlider *m_smoothingSlider;
    QLabel *m_smoothingLabel;
    
    // 钢笔设置组
    QGroupBox *m_penGroup;
    QDoubleSpinBox *m_strokeWidthSpin;
    QDoubleSpinBox *m_miterLimitSpin;
    QComboBox *m_joinStyleCombo;
    QComboBox *m_capStyleCombo;
    
    // 橡皮擦设置组
    QGroupBox *m_eraserGroup;
    QSlider *m_eraserSizeSlider;
    QLabel *m_eraserSizeLabel;
    
    // 填充设置组
    QGroupBox *m_fillGroup;
    QSlider *m_toleranceSlider;
    QLabel *m_toleranceLabel;
    
    // 形状设置组
    QGroupBox *m_shapeGroup;
    QCheckBox *m_fillShapeCheck;
    QDoubleSpinBox *m_shapeStrokeWidthSpin;
};

#endif // TOOLS_PANEL_H