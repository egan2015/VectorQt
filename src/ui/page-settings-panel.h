#ifndef PAGE_SETTINGS_PANEL_H
#define PAGE_SETTINGS_PANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QTabWidget>
#include <QRadioButton>

class DrawingScene;
class DrawingView;

/**
 * 页面设置面板 - 管理画布和文档设置
 */
class PageSettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PageSettingsPanel(QWidget *parent = nullptr);
    
    // 设置场景
    void setScene(DrawingScene *scene);
    
    // 从场景更新设置
    void updateFromScene();
    
    // 更新预设尺寸选择
    void updatePresetSelection();
    
    // 设置视图
    void setView(DrawingView *view);
    
    // 获取页面设置
    QSizeF getPageSize() const;
    Qt::Orientation getPageOrientation() const;
    QString getUnit() const;
    QMarginsF getPageMargins() const;
    QColor getBackgroundColor() const;
    bool isGridVisible() const;
    bool isRulerVisible() const;
    
    // 应用设置
    void applySettings();
    void resetToDefaults();

signals:
    void settingsChanged();
    void pageSizeChanged(const QSizeF &size);
    void pageOrientationChanged(Qt::Orientation orientation);
    void unitChanged(const QString &unit);

private slots:
    void onPresetSizeChanged();
    void onWidthChanged(double width);
    void onHeightChanged(double height);
    void onOrientationChanged();
    void onUnitChanged();
    void onMarginChanged();
    void onBackgroundColorChanged();
    void onGridVisibleToggled(bool visible);
    void onRulerVisibleToggled(bool visible);
    void onApplyClicked();
    void onResetClicked();

private:
    void setupUI();
    void setupPageSettings();
    void setupDisplaySettings();
    void setupGridSettings();
    void updatePresetSizes();
    void updateOrientationFromSize();
    void updateSizeFromOrientation();
    void convertUnits(const QString &fromUnit, const QString &toUnit);
    
    // 单位转换常量
    static constexpr double MM_TO_INCH = 0.0393701;
    static constexpr double INCH_TO_MM = 25.4;
    static constexpr double POINT_TO_MM = 0.352778;
    static constexpr double MM_TO_POINT = 2.83465;
    
    DrawingScene *m_scene;
    DrawingView *m_view;
    
    // 主布局
    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;
    
    // 页面设置标签页
    QWidget *m_pageTab;
    QVBoxLayout *m_pageLayout;
    
    // 预设尺寸
    QComboBox *m_presetSizeCombo;
    QLabel *m_presetLabel;
    
    // 自定义尺寸
    QGroupBox *m_customSizeGroup;
    QGridLayout *m_customSizeLayout;
    QLabel *m_widthLabel;
    QDoubleSpinBox *m_widthSpin;
    QLabel *m_heightLabel;
    QDoubleSpinBox *m_heightSpin;
    QLabel *m_unitLabel;
    QComboBox *m_unitCombo;
    
    // 方向
    QGroupBox *m_orientationGroup;
    QHBoxLayout *m_orientationLayout;
    QRadioButton *m_portraitRadio;
    QRadioButton *m_landscapeRadio;
    
    // 边距
    QGroupBox *m_marginsGroup;
    QGridLayout *m_marginsLayout;
    QLabel *m_topLabel;
    QDoubleSpinBox *m_topSpin;
    QLabel *m_leftLabel;
    QDoubleSpinBox *m_leftSpin;
    QLabel *m_rightLabel;
    QDoubleSpinBox *m_rightSpin;
    QLabel *m_bottomLabel;
    QDoubleSpinBox *m_bottomSpin;
    
    // 背景颜色
    QHBoxLayout *m_backgroundLayout;
    QLabel *m_backgroundLabel;
    QPushButton *m_backgroundButton;
    QColor m_backgroundColor;
    
    // 显示设置标签页
    QWidget *m_displayTab;
    QVBoxLayout *m_displayLayout;
    
    QCheckBox *m_showGridCheck;
    QCheckBox *m_showRulersCheck;
    QCheckBox *m_showGuidesCheck;
    
    // 网格设置标签页
    QWidget *m_gridTab;
    QVBoxLayout *m_gridLayout;
    
    QCheckBox *m_gridEnabledCheck;
    QDoubleSpinBox *m_gridSizeSpin;
    QLabel *m_gridSizeLabel;
    QPushButton *m_gridColorButton;
    QColor m_gridColor;
    
    // 操作按钮
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_applyButton;
    QPushButton *m_resetButton;
    
    // 当前设置
    QSizeF m_currentPageSize;
    Qt::Orientation m_currentOrientation;
    QString m_currentUnit;
    QMarginsF m_currentMargins;
};

#endif // PAGE_SETTINGS_PANEL_H