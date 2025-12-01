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
#include <QButtonGroup>
#include <QColorDialog>
#include <QApplication>
#include "page-settings-panel.h"
#include "drawingscene.h"
#include "drawingview.h"

PageSettingsPanel::PageSettingsPanel(QWidget *parent)
    : QWidget(parent)
    , m_scene(nullptr)
    , m_view(nullptr)
    , m_mainLayout(nullptr)
    , m_tabWidget(nullptr)
    , m_pageTab(nullptr)
    , m_pageLayout(nullptr)
    , m_presetSizeCombo(nullptr)
    , m_presetLabel(nullptr)
    , m_customSizeGroup(nullptr)
    , m_customSizeLayout(nullptr)
    , m_widthLabel(nullptr)
    , m_widthSpin(nullptr)
    , m_heightLabel(nullptr)
    , m_heightSpin(nullptr)
    , m_unitLabel(nullptr)
    , m_unitCombo(nullptr)
    , m_orientationGroup(nullptr)
    , m_orientationLayout(nullptr)
    , m_portraitRadio(nullptr)
    , m_landscapeRadio(nullptr)
    , m_marginsGroup(nullptr)
    , m_marginsLayout(nullptr)
    , m_topLabel(nullptr)
    , m_topSpin(nullptr)
    , m_leftLabel(nullptr)
    , m_leftSpin(nullptr)
    , m_rightLabel(nullptr)
    , m_rightSpin(nullptr)
    , m_bottomLabel(nullptr)
    , m_bottomSpin(nullptr)
    , m_backgroundLayout(nullptr)
    , m_backgroundLabel(nullptr)
    , m_backgroundButton(nullptr)
    , m_backgroundColor(QColor(255, 255, 255))
    , m_displayTab(nullptr)
    , m_displayLayout(nullptr)
    , m_showGridCheck(nullptr)
    , m_showRulersCheck(nullptr)
    , m_showGuidesCheck(nullptr)
    , m_gridTab(nullptr)
    , m_gridLayout(nullptr)
    , m_gridEnabledCheck(nullptr)
    , m_gridSizeSpin(nullptr)
    , m_gridSizeLabel(nullptr)
    , m_gridColorButton(nullptr)
    , m_gridColor(QColor(200, 200, 200))
    , m_buttonLayout(nullptr)
    , m_applyButton(nullptr)
    , m_resetButton(nullptr)
    , m_currentPageSize(QSizeF(210, 297)) // A4尺寸，毫米
    , m_currentOrientation(Qt::Vertical)
    , m_currentUnit("mm")
    , m_currentMargins(QMarginsF(10, 10, 10, 10))
{
    setupUI();
}

// 设置场景
void PageSettingsPanel::setScene(DrawingScene *scene)
{
    m_scene = scene;
    
    if (m_scene) {
        // 从场景加载当前设置
        m_currentPageSize = m_scene->sceneRect().size();
        updateOrientationFromSize();
        
        // 更新控件显示
        m_widthSpin->setValue(m_currentPageSize.width());
        m_heightSpin->setValue(m_currentPageSize.height());
        
        // 更新显示设置
        if (m_showGridCheck) {
            m_showGridCheck->setChecked(m_scene->isGridVisible());
        }
        
        // 同步网格大小显示
        m_gridSizeSpin->setValue(m_scene->gridSize());
    }
}

// 从场景更新设置
void PageSettingsPanel::updateFromScene()
{
    if (!m_scene) {
        return;
    }
    
    // 从场景获取当前设置
    m_currentPageSize = m_scene->sceneRect().size();
    updateOrientationFromSize();
    
    // 更新控件显示，但不触发信号
    m_widthSpin->blockSignals(true);
    m_heightSpin->blockSignals(true);
    
    m_widthSpin->setValue(m_currentPageSize.width());
    m_heightSpin->setValue(m_currentPageSize.height());
    
    // 注意：网格大小是编辑器属性，不随SVG导入更新
    // 保持用户当前的网格设置
    
    m_widthSpin->blockSignals(false);
    m_heightSpin->blockSignals(false);
    
    // 更新预设尺寸选择
    updatePresetSelection();
}

void PageSettingsPanel::setView(DrawingView *view)
{
    m_view = view;
}

void PageSettingsPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);
    
    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);
    
    // 设置各个标签页
    setupPageSettings();
    setupDisplaySettings();
    setupGridSettings();
    
    m_mainLayout->addWidget(m_tabWidget);
    
    // 创建操作按钮
    m_buttonLayout = new QHBoxLayout();
    m_applyButton = new QPushButton(tr("应用"), this);
    m_resetButton = new QPushButton(tr("重置"), this);
    
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_applyButton);
    m_buttonLayout->addWidget(m_resetButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
    
    // 连接信号
    connect(m_applyButton, &QPushButton::clicked, this, &PageSettingsPanel::onApplyClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &PageSettingsPanel::onResetClicked);
}

void PageSettingsPanel::setupPageSettings()
{
    m_pageTab = new QWidget();
    m_pageLayout = new QVBoxLayout(m_pageTab);
    
    // 预设尺寸
    QHBoxLayout *presetLayout = new QHBoxLayout();
    m_presetLabel = new QLabel(tr("预设尺寸:"), this);
    m_presetSizeCombo = new QComboBox(this);
    updatePresetSizes();
    
    presetLayout->addWidget(m_presetLabel);
    presetLayout->addWidget(m_presetSizeCombo);
    m_pageLayout->addLayout(presetLayout);
    
    connect(m_presetSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageSettingsPanel::onPresetSizeChanged);
    
    // 自定义尺寸
    m_customSizeGroup = new QGroupBox(tr("自定义尺寸"), this);
    m_customSizeLayout = new QGridLayout(m_customSizeGroup);
    
    m_widthLabel = new QLabel(tr("宽度:"), this);
    m_widthSpin = new QDoubleSpinBox(this);
    m_widthSpin->setRange(1.0, 10000.0);
    m_widthSpin->setValue(210.0);
    m_widthSpin->setSuffix(" mm");
    
    m_heightLabel = new QLabel(tr("高度:"), this);
    m_heightSpin = new QDoubleSpinBox(this);
    m_heightSpin->setRange(1.0, 10000.0);
    m_heightSpin->setValue(297.0);
    m_heightSpin->setSuffix(" mm");
    
    m_unitLabel = new QLabel(tr("单位:"), this);
    m_unitCombo = new QComboBox(this);
    m_unitCombo->addItem("mm");
    m_unitCombo->addItem("cm");
    m_unitCombo->addItem("inch");
    m_unitCombo->addItem("pt");
    
    m_customSizeLayout->addWidget(m_widthLabel, 0, 0);
    m_customSizeLayout->addWidget(m_widthSpin, 0, 1);
    m_customSizeLayout->addWidget(m_heightLabel, 1, 0);
    m_customSizeLayout->addWidget(m_heightSpin, 1, 1);
    m_customSizeLayout->addWidget(m_unitLabel, 2, 0);
    m_customSizeLayout->addWidget(m_unitCombo, 2, 1);
    
    m_pageLayout->addWidget(m_customSizeGroup);
    
    connect(m_widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PageSettingsPanel::onWidthChanged);
    connect(m_heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PageSettingsPanel::onHeightChanged);
    connect(m_unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageSettingsPanel::onUnitChanged);
    
    // 方向
    m_orientationGroup = new QGroupBox(tr("方向"), this);
    m_orientationLayout = new QHBoxLayout(m_orientationGroup);
    
    m_portraitRadio = new QRadioButton(tr("纵向"), this);
    m_landscapeRadio = new QRadioButton(tr("横向"), this);
    m_portraitRadio->setChecked(true);
    
    m_orientationLayout->addWidget(m_portraitRadio);
    m_orientationLayout->addWidget(m_landscapeRadio);
    
    m_pageLayout->addWidget(m_orientationGroup);
    
    QButtonGroup *orientationGroup = new QButtonGroup(this);
    orientationGroup->addButton(m_portraitRadio);
    orientationGroup->addButton(m_landscapeRadio);
    connect(orientationGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            this, &PageSettingsPanel::onOrientationChanged);
    
    // 边距
    m_marginsGroup = new QGroupBox(tr("边距"), this);
    m_marginsLayout = new QGridLayout(m_marginsGroup);
    
    m_topLabel = new QLabel(tr("上:"), this);
    m_topSpin = new QDoubleSpinBox(this);
    m_topSpin->setRange(0.0, 100.0);
    m_topSpin->setValue(10.0);
    m_topSpin->setSuffix(" mm");
    
    m_leftLabel = new QLabel(tr("左:"), this);
    m_leftSpin = new QDoubleSpinBox(this);
    m_leftSpin->setRange(0.0, 100.0);
    m_leftSpin->setValue(10.0);
    m_leftSpin->setSuffix(" mm");
    
    m_rightLabel = new QLabel(tr("右:"), this);
    m_rightSpin = new QDoubleSpinBox(this);
    m_rightSpin->setRange(0.0, 100.0);
    m_rightSpin->setValue(10.0);
    m_rightSpin->setSuffix(" mm");
    
    m_bottomLabel = new QLabel(tr("下:"), this);
    m_bottomSpin = new QDoubleSpinBox(this);
    m_bottomSpin->setRange(0.0, 100.0);
    m_bottomSpin->setValue(10.0);
    m_bottomSpin->setSuffix(" mm");
    
    m_marginsLayout->addWidget(m_topLabel, 0, 0);
    m_marginsLayout->addWidget(m_topSpin, 0, 1);
    m_marginsLayout->addWidget(m_leftLabel, 1, 0);
    m_marginsLayout->addWidget(m_leftSpin, 1, 1);
    m_marginsLayout->addWidget(m_rightLabel, 1, 2);
    m_marginsLayout->addWidget(m_rightSpin, 1, 3);
    m_marginsLayout->addWidget(m_bottomLabel, 2, 0);
    m_marginsLayout->addWidget(m_bottomSpin, 2, 1);
    
    m_pageLayout->addWidget(m_marginsGroup);
    
    // 边距变化信号
    connect(m_topSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PageSettingsPanel::onMarginChanged);
    connect(m_leftSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PageSettingsPanel::onMarginChanged);
    connect(m_rightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PageSettingsPanel::onMarginChanged);
    connect(m_bottomSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PageSettingsPanel::onMarginChanged);
    
    // 背景颜色
    m_backgroundLayout = new QHBoxLayout();
    m_backgroundLabel = new QLabel(tr("背景颜色:"), this);
    m_backgroundButton = new QPushButton(this);
    m_backgroundButton->setFixedWidth(50);
    QString style = QString("background-color: %1").arg(m_backgroundColor.name());
    m_backgroundButton->setStyleSheet(style);
    
    m_backgroundLayout->addWidget(m_backgroundLabel);
    m_backgroundLayout->addWidget(m_backgroundButton);
    m_backgroundLayout->addStretch();
    
    m_pageLayout->addLayout(m_backgroundLayout);
    
    connect(m_backgroundButton, &QPushButton::clicked, this, &PageSettingsPanel::onBackgroundColorChanged);
    
    m_pageLayout->addStretch();
    
    m_tabWidget->addTab(m_pageTab, tr("页面"));
}

void PageSettingsPanel::setupDisplaySettings()
{
    m_displayTab = new QWidget();
    m_displayLayout = new QVBoxLayout(m_displayTab);
    
    m_showGridCheck = new QCheckBox(tr("显示网格"), this);
    m_showRulersCheck = new QCheckBox(tr("显示标尺"), this);
    m_showGuidesCheck = new QCheckBox(tr("显示参考线"), this);
    
    // 设置默认值
    m_showGridCheck->setChecked(true);
    m_showRulersCheck->setChecked(true);
    m_showGuidesCheck->setChecked(true);
    
    m_displayLayout->addWidget(m_showGridCheck);
    m_displayLayout->addWidget(m_showRulersCheck);
    m_displayLayout->addWidget(m_showGuidesCheck);
    
    connect(m_showGridCheck, &QCheckBox::toggled, this, &PageSettingsPanel::onGridVisibleToggled);
    connect(m_showRulersCheck, &QCheckBox::toggled, this, &PageSettingsPanel::onRulerVisibleToggled);
    
    m_displayLayout->addStretch();
    
    m_tabWidget->addTab(m_displayTab, tr("显示"));
}

void PageSettingsPanel::setupGridSettings()
{
    m_gridTab = new QWidget();
    m_gridLayout = new QVBoxLayout(m_gridTab);
    
    m_gridEnabledCheck = new QCheckBox(tr("启用网格对齐"), this);
    m_gridEnabledCheck->setChecked(true);
    
    QHBoxLayout *gridSizeLayout = new QHBoxLayout();
    m_gridSizeLabel = new QLabel(tr("网格大小:"), this);
    m_gridSizeSpin = new QDoubleSpinBox(this);
    m_gridSizeSpin->setRange(1.0, 100.0);
    m_gridSizeSpin->setValue(20.0);
    m_gridSizeSpin->setSuffix(" px");
    
    gridSizeLayout->addWidget(m_gridSizeLabel);
    gridSizeLayout->addWidget(m_gridSizeSpin);
    
    QHBoxLayout *gridColorLayout = new QHBoxLayout();
    QLabel *gridColorLabel = new QLabel(tr("网格颜色:"), this);
    m_gridColorButton = new QPushButton(this);
    m_gridColorButton->setFixedWidth(50);
    QString gridStyle = QString("background-color: %1").arg(m_gridColor.name());
    m_gridColorButton->setStyleSheet(gridStyle);
    
    gridColorLayout->addWidget(gridColorLabel);
    gridColorLayout->addWidget(m_gridColorButton);
    gridColorLayout->addStretch();
    
    connect(m_gridColorButton, &QPushButton::clicked, [this]() {
        QColor color = QColorDialog::getColor(m_gridColor, this, tr("选择网格颜色"));
        if (color.isValid()) {
            m_gridColor = color;
            QString style = QString("background-color: %1").arg(m_gridColor.name());
            m_gridColorButton->setStyleSheet(style);
            emit settingsChanged();
        }
    });
    
    m_gridLayout->addWidget(m_gridEnabledCheck);
    m_gridLayout->addLayout(gridSizeLayout);
    m_gridLayout->addLayout(gridColorLayout);
    m_gridLayout->addStretch();
    
    m_tabWidget->addTab(m_gridTab, tr("网格"));
}

void PageSettingsPanel::updatePresetSizes()
{
    m_presetSizeCombo->clear();
    m_presetSizeCombo->addItem(tr("自定义"), -1);
    m_presetSizeCombo->addItem("A4 (210×297 mm)", 0);
    m_presetSizeCombo->addItem("A3 (297×420 mm)", 1);
    m_presetSizeCombo->addItem("A5 (148×210 mm)", 2);
    m_presetSizeCombo->addItem("Letter (8.5×11 inch)", 3);
    m_presetSizeCombo->addItem("Legal (8.5×14 inch)", 4);
    m_presetSizeCombo->addItem("Tabloid (11×17 inch)", 5);
}

void PageSettingsPanel::updateOrientationFromSize()
{
    if (m_currentPageSize.width() > m_currentPageSize.height()) {
        m_currentOrientation = Qt::Horizontal;
        if (m_landscapeRadio) m_landscapeRadio->setChecked(true);
    } else {
        m_currentOrientation = Qt::Vertical;
        if (m_portraitRadio) m_portraitRadio->setChecked(true);
    }
}

void PageSettingsPanel::updateSizeFromOrientation()
{
    if (m_currentOrientation == Qt::Horizontal) {
        // 横向：交换宽高
        double temp = m_widthSpin->value();
        m_widthSpin->setValue(m_heightSpin->value());
        m_heightSpin->setValue(temp);
    }
}

// 槽函数实现
void PageSettingsPanel::onPresetSizeChanged()
{
    int index = m_presetSizeCombo->currentData().toInt();
    if (index < 0) return; // 自定义
    
    QSizeF size;
    switch (index) {
        case 0: size = QSizeF(210, 297); break;    // A4
        case 1: size = QSizeF(297, 420); break;    // A3
        case 2: size = QSizeF(148, 210); break;    // A5
        case 3: size = QSizeF(215.9, 279.4); break; // Letter
        case 4: size = QSizeF(215.9, 355.6); break; // Legal
        case 5: size = QSizeF(279.4, 431.8); break; // Tabloid
        default: return;
    }
    
    m_widthSpin->setValue(size.width());
    m_heightSpin->setValue(size.height());
    updateOrientationFromSize();
}

void PageSettingsPanel::onWidthChanged(double width)
{
    m_currentPageSize.setWidth(width);
    updateOrientationFromSize();
    emit settingsChanged();
}

void PageSettingsPanel::onHeightChanged(double height)
{
    m_currentPageSize.setHeight(height);
    updateOrientationFromSize();
    emit settingsChanged();
}

void PageSettingsPanel::onOrientationChanged()
{
    Qt::Orientation newOrientation = m_landscapeRadio->isChecked() ? Qt::Horizontal : Qt::Vertical;
    if (newOrientation != m_currentOrientation) {
        m_currentOrientation = newOrientation;
        updateSizeFromOrientation();
        emit pageOrientationChanged(m_currentOrientation);
        emit settingsChanged();
    }
}

void PageSettingsPanel::onUnitChanged()
{
    QString newUnit = m_unitCombo->currentText();
    if (newUnit != m_currentUnit) {
        // 这里应该进行单位转换，简化处理
        m_currentUnit = newUnit;
        emit unitChanged(m_currentUnit);
        emit settingsChanged();
    }
}

void PageSettingsPanel::onMarginChanged()
{
    m_currentMargins = QMarginsF(
        m_leftSpin->value(),
        m_topSpin->value(),
        m_rightSpin->value(),
        m_bottomSpin->value()
    );
    emit settingsChanged();
}

void PageSettingsPanel::onBackgroundColorChanged()
{
    QColor color = QColorDialog::getColor(m_backgroundColor, this, tr("选择背景颜色"));
    if (color.isValid()) {
        m_backgroundColor = color;
        QString style = QString("background-color: %1").arg(m_backgroundColor.name());
        m_backgroundButton->setStyleSheet(style);
        emit settingsChanged();
    }
}

void PageSettingsPanel::onGridVisibleToggled(bool visible)
{
    if (m_scene) {
        m_scene->setGridVisible(visible);
    }
    emit settingsChanged();
}

void PageSettingsPanel::onRulerVisibleToggled(bool visible)
{
    // TODO: 实现标尺显示/隐藏
    emit settingsChanged();
}

void PageSettingsPanel::onApplyClicked()
{
    applySettings();
}

void PageSettingsPanel::onResetClicked()
{
    resetToDefaults();
}

// 公共方法实现
QSizeF PageSettingsPanel::getPageSize() const
{
    return m_currentPageSize;
}

Qt::Orientation PageSettingsPanel::getPageOrientation() const
{
    return m_currentOrientation;
}

QString PageSettingsPanel::getUnit() const
{
    return m_currentUnit;
}

QMarginsF PageSettingsPanel::getPageMargins() const
{
    return m_currentMargins;
}

QColor PageSettingsPanel::getBackgroundColor() const
{
    return m_backgroundColor;
}

bool PageSettingsPanel::isGridVisible() const
{
    return m_showGridCheck ? m_showGridCheck->isChecked() : false;
}

bool PageSettingsPanel::isRulerVisible() const
{
    return m_showRulersCheck ? m_showRulersCheck->isChecked() : false;
}

void PageSettingsPanel::applySettings()
{
    if (m_scene) {
        // 应用页面大小
        m_scene->setSceneRect(0, 0, m_currentPageSize.width(), m_currentPageSize.height());
        
        // 应用网格设置
        m_scene->setGridVisible(m_showGridCheck->isChecked());
        m_scene->setGridSize(m_gridSizeSpin->value());
        m_scene->setGridColor(m_gridColor);
        m_scene->setGridAlignmentEnabled(m_gridEnabledCheck->isChecked());
        
        // 应用背景颜色
        m_scene->setBackgroundBrush(QBrush(m_backgroundColor));
    }
    
    emit settingsChanged();
    emit pageSizeChanged(m_currentPageSize);
}

void PageSettingsPanel::updatePresetSelection()
{
    // 检查当前尺寸是否匹配预设尺寸
    bool found = false;
    for (int i = 0; i < m_presetSizeCombo->count(); ++i) {
        int presetIndex = m_presetSizeCombo->itemData(i).toInt();
        if (presetIndex < 0) continue; // 跳过"自定义"
        
        QSizeF presetSize;
        switch (presetIndex) {
            case 0: presetSize = QSizeF(210, 297); break;    // A4
            case 1: presetSize = QSizeF(297, 420); break;    // A3
            case 2: presetSize = QSizeF(148, 210); break;    // A5
            case 3: presetSize = QSizeF(215.9, 279.4); break; // Letter
            case 4: presetSize = QSizeF(215.9, 355.6); break; // Legal
            case 5: presetSize = QSizeF(279.4, 431.8); break; // Tabloid
            default: continue;
        }
        
        // 考虑横向/纵向
        if ((m_portraitRadio->isChecked() && m_currentPageSize == presetSize) ||
            (!m_portraitRadio->isChecked() && m_currentPageSize == QSizeF(presetSize.height(), presetSize.width()))) {
            m_presetSizeCombo->setCurrentIndex(i);
            found = true;
            break;
        }
    }
    
    if (!found) {
        // 设置为"自定义"
        for (int i = 0; i < m_presetSizeCombo->count(); ++i) {
            if (m_presetSizeCombo->itemData(i).toInt() == -1) {
                m_presetSizeCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void PageSettingsPanel::resetToDefaults()
{
    // 重置到默认A4纵向设置
    m_presetSizeCombo->setCurrentIndex(1); // A4
    m_widthSpin->setValue(210.0);
    m_heightSpin->setValue(297.0);
    m_unitCombo->setCurrentText("mm");
    m_portraitRadio->setChecked(true);
    
    m_topSpin->setValue(10.0);
    m_leftSpin->setValue(10.0);
    m_rightSpin->setValue(10.0);
    m_bottomSpin->setValue(10.0);
    
    m_backgroundColor = QColor(255, 255, 255);
    QString style = QString("background-color: %1").arg(m_backgroundColor.name());
    m_backgroundButton->setStyleSheet(style);
    
    m_showGridCheck->setChecked(true);
    m_showRulersCheck->setChecked(true);
    m_showGuidesCheck->setChecked(true);
    
    m_gridEnabledCheck->setChecked(true);
    m_gridSizeSpin->setValue(20.0);
    m_gridColor = QColor(200, 200, 200);
    QString gridStyle = QString("background-color: %1").arg(m_gridColor.name());
    m_gridColorButton->setStyleSheet(gridStyle);
    
    applySettings();
}