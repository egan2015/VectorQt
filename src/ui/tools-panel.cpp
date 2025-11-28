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
#include "tools-panel.h"
#include "drawingscene.h"
#include "../core/toolbase.h"
#include "../tools/drawing-tool-brush.h"
#include "../tools/drawing-tool-pen.h"
#include "../tools/drawing-tool-eraser.h"
#include "../tools/drawing-tool-fill.h"

ToolsPanel::ToolsPanel(QWidget *parent)
    : QWidget(parent)
    , m_scene(nullptr)
    , m_currentTool(nullptr)
    , m_mainLayout(nullptr)
    , m_generalGroup(nullptr)
    , m_antialiasingCheck(nullptr)
    , m_snapToGridCheck(nullptr)
    , m_resetButton(nullptr)
    , m_brushGroup(nullptr)
    , m_brushSizeSlider(nullptr)
    , m_brushSizeLabel(nullptr)
    , m_opacitySlider(nullptr)
    , m_opacityLabel(nullptr)
    , m_smoothingSlider(nullptr)
    , m_smoothingLabel(nullptr)
    , m_penGroup(nullptr)
    , m_strokeWidthSpin(nullptr)
    , m_miterLimitSpin(nullptr)
    , m_joinStyleCombo(nullptr)
    , m_capStyleCombo(nullptr)
    , m_eraserGroup(nullptr)
    , m_eraserSizeSlider(nullptr)
    , m_eraserSizeLabel(nullptr)
    , m_fillGroup(nullptr)
    , m_toleranceSlider(nullptr)
    , m_toleranceLabel(nullptr)
    , m_shapeGroup(nullptr)
    , m_fillShapeCheck(nullptr)
    , m_shapeStrokeWidthSpin(nullptr)
{
    setupUI();
}

void ToolsPanel::setScene(DrawingScene *scene)
{
    m_scene = scene;
    updateToolSettings();
}

void ToolsPanel::setCurrentTool(ToolBase *tool)
{
    if (m_currentTool == tool) {
        return;
    }
    
    m_currentTool = tool;
    updateToolSettings();
}

void ToolsPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);
    
    // 设置通用设置（始终显示）
    setupGeneralSettings();
    
    // 默认显示通用设置
    showGeneralSettings();
}

void ToolsPanel::setupGeneralSettings()
{
    m_generalGroup = new QGroupBox(tr("通用设置"), this);
    QVBoxLayout *generalLayout = new QVBoxLayout(m_generalGroup);
    
    m_antialiasingCheck = new QCheckBox(tr("抗锯齿"), this);
    m_antialiasingCheck->setChecked(true);
    connect(m_antialiasingCheck, &QCheckBox::toggled, this, &ToolsPanel::onAntialiasingToggled);
    generalLayout->addWidget(m_antialiasingCheck);
    
    m_snapToGridCheck = new QCheckBox(tr("网格对齐"), this);
    m_snapToGridCheck->setChecked(false);
    connect(m_snapToGridCheck, &QCheckBox::toggled, this, &ToolsPanel::onSnapToGridToggled);
    generalLayout->addWidget(m_snapToGridCheck);
    
    m_resetButton = new QPushButton(tr("重置设置"), this);
    connect(m_resetButton, &QPushButton::clicked, this, &ToolsPanel::onResetSettings);
    generalLayout->addWidget(m_resetButton);
    
    m_mainLayout->addWidget(m_generalGroup);
}

void ToolsPanel::setupBrushSettings()
{
    m_brushGroup = new QGroupBox(tr("画笔设置"), this);
    QVBoxLayout *brushLayout = new QVBoxLayout(m_brushGroup);
    
    // 画笔大小
    QHBoxLayout *sizeLayout = new QHBoxLayout();
    QLabel *sizeLabel = new QLabel(tr("大小:"), this);
    m_brushSizeSlider = new QSlider(Qt::Horizontal, this);
    m_brushSizeSlider->setRange(1, 100);
    m_brushSizeSlider->setValue(10);
    m_brushSizeLabel = new QLabel("10", this);
    m_brushSizeLabel->setFixedWidth(30);
    
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(m_brushSizeSlider);
    sizeLayout->addWidget(m_brushSizeLabel);
    brushLayout->addLayout(sizeLayout);
    
    connect(m_brushSizeSlider, &QSlider::valueChanged, this, &ToolsPanel::onBrushSizeChanged);
    
    // 不透明度
    QHBoxLayout *opacityLayout = new QHBoxLayout();
    QLabel *opacityLabel = new QLabel(tr("不透明度:"), this);
    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    m_opacityLabel = new QLabel("100%", this);
    m_opacityLabel->setFixedWidth(40);
    
    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityLabel);
    brushLayout->addLayout(opacityLayout);
    
    connect(m_opacitySlider, &QSlider::valueChanged, this, &ToolsPanel::onOpacityChanged);
    
    // 平滑度
    QHBoxLayout *smoothingLayout = new QHBoxLayout();
    QLabel *smoothingLabel = new QLabel(tr("平滑度:"), this);
    m_smoothingSlider = new QSlider(Qt::Horizontal, this);
    m_smoothingSlider->setRange(0, 100);
    m_smoothingSlider->setValue(50);
    m_smoothingLabel = new QLabel("50", this);
    m_smoothingLabel->setFixedWidth(30);
    
    smoothingLayout->addWidget(smoothingLabel);
    smoothingLayout->addWidget(m_smoothingSlider);
    smoothingLayout->addWidget(m_smoothingLabel);
    brushLayout->addLayout(smoothingLayout);
    
    connect(m_smoothingSlider, &QSlider::valueChanged, this, &ToolsPanel::onSmoothingChanged);
}

void ToolsPanel::setupPenSettings()
{
    m_penGroup = new QGroupBox(tr("钢笔设置"), this);
    QVBoxLayout *penLayout = new QVBoxLayout(m_penGroup);
    
    // 线宽
    QHBoxLayout *strokeLayout = new QHBoxLayout();
    QLabel *strokeLabel = new QLabel(tr("线宽:"), this);
    m_strokeWidthSpin = new QDoubleSpinBox(this);
    m_strokeWidthSpin->setRange(0.1, 100.0);
    m_strokeWidthSpin->setValue(2.0);
    m_strokeWidthSpin->setSuffix(" px");
    
    strokeLayout->addWidget(strokeLabel);
    strokeLayout->addWidget(m_strokeWidthSpin);
    penLayout->addLayout(strokeLayout);
    
    connect(m_strokeWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ToolsPanel::onStrokeWidthChanged);
    
    // 线条连接样式
    QHBoxLayout *joinLayout = new QHBoxLayout();
    QLabel *joinLabel = new QLabel(tr("连接样式:"), this);
    m_joinStyleCombo = new QComboBox(this);
    m_joinStyleCombo->addItem(tr("斜接"), Qt::MiterJoin);
    m_joinStyleCombo->addItem(tr("圆形"), Qt::RoundJoin);
    m_joinStyleCombo->addItem(tr("斜角"), Qt::BevelJoin);
    
    joinLayout->addWidget(joinLabel);
    joinLayout->addWidget(m_joinStyleCombo);
    penLayout->addLayout(joinLayout);
    
    connect(m_joinStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolsPanel::onJoinStyleChanged);
    
    // 线条端点样式
    QHBoxLayout *capLayout = new QHBoxLayout();
    QLabel *capLabel = new QLabel(tr("端点样式:"), this);
    m_capStyleCombo = new QComboBox(this);
    m_capStyleCombo->addItem(tr("平直"), Qt::FlatCap);
    m_capStyleCombo->addItem(tr("方形"), Qt::SquareCap);
    m_capStyleCombo->addItem(tr("圆形"), Qt::RoundCap);
    
    capLayout->addWidget(capLabel);
    capLayout->addWidget(m_capStyleCombo);
    penLayout->addLayout(capLayout);
    
    connect(m_capStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolsPanel::onCapStyleChanged);
    
    // 斜接限制
    QHBoxLayout *miterLayout = new QHBoxLayout();
    QLabel *miterLabel = new QLabel(tr("斜接限制:"), this);
    m_miterLimitSpin = new QDoubleSpinBox(this);
    m_miterLimitSpin->setRange(0.1, 100.0);
    m_miterLimitSpin->setValue(4.0);
    
    miterLayout->addWidget(miterLabel);
    miterLayout->addWidget(m_miterLimitSpin);
    penLayout->addLayout(miterLayout);
    
    connect(m_miterLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ToolsPanel::onMiterLimitChanged);
    
    // 压感支持
    QCheckBox *pressureSupportCheck = new QCheckBox(tr("压感支持"), this);
    pressureSupportCheck->setChecked(false);
    penLayout->addWidget(pressureSupportCheck);
    
    connect(pressureSupportCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        emit penPressureSupportChanged(enabled);
        emit toolSettingsChanged();
    });
}

void ToolsPanel::setupEraserSettings()
{
    m_eraserGroup = new QGroupBox(tr("橡皮擦设置"), this);
    QVBoxLayout *eraserLayout = new QVBoxLayout(m_eraserGroup);
    
    // 橡皮擦大小
    QHBoxLayout *sizeLayout = new QHBoxLayout();
    QLabel *sizeLabel = new QLabel(tr("大小:"), this);
    m_eraserSizeSlider = new QSlider(Qt::Horizontal, this);
    m_eraserSizeSlider->setRange(1, 200);
    m_eraserSizeSlider->setValue(20);
    m_eraserSizeLabel = new QLabel("20", this);
    m_eraserSizeLabel->setFixedWidth(30);
    
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(m_eraserSizeSlider);
    sizeLayout->addWidget(m_eraserSizeLabel);
    eraserLayout->addLayout(sizeLayout);
    
    connect(m_eraserSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_eraserSizeLabel->setText(QString::number(value));
        emit eraserSizeChanged(value);
        emit toolSettingsChanged();
    });
}

void ToolsPanel::setupFillSettings()
{
    m_fillGroup = new QGroupBox(tr("填充设置"), this);
    QVBoxLayout *fillLayout = new QVBoxLayout(m_fillGroup);
    
    // 容差
    QHBoxLayout *toleranceLayout = new QHBoxLayout();
    QLabel *toleranceLabel = new QLabel(tr("容差:"), this);
    m_toleranceSlider = new QSlider(Qt::Horizontal, this);
    m_toleranceSlider->setRange(0, 255);
    m_toleranceSlider->setValue(32);
    m_toleranceLabel = new QLabel("32", this);
    m_toleranceLabel->setFixedWidth(30);
    
    toleranceLayout->addWidget(toleranceLabel);
    toleranceLayout->addWidget(m_toleranceSlider);
    toleranceLayout->addWidget(m_toleranceLabel);
    fillLayout->addLayout(toleranceLayout);
    
    connect(m_toleranceSlider, &QSlider::valueChanged, this, &ToolsPanel::onFillToleranceChanged);
}

void ToolsPanel::setupShapeSettings()
{
    m_shapeGroup = new QGroupBox(tr("形状设置"), this);
    QVBoxLayout *shapeLayout = new QVBoxLayout(m_shapeGroup);
    
    // 填充形状
    m_fillShapeCheck = new QCheckBox(tr("填充形状"), this);
    m_fillShapeCheck->setChecked(false);
    shapeLayout->addWidget(m_fillShapeCheck);
    
    // 形状线宽
    QHBoxLayout *strokeLayout = new QHBoxLayout();
    QLabel *strokeLabel = new QLabel(tr("线宽:"), this);
    m_shapeStrokeWidthSpin = new QDoubleSpinBox(this);
    m_shapeStrokeWidthSpin->setRange(0.1, 50.0);
    m_shapeStrokeWidthSpin->setValue(1.0);
    m_shapeStrokeWidthSpin->setSuffix(" px");
    
    strokeLayout->addWidget(strokeLabel);
    strokeLayout->addWidget(m_shapeStrokeWidthSpin);
    shapeLayout->addLayout(strokeLayout);
}

void ToolsPanel::updateToolSettings()
{
    clearAllGroups();
    
    // 通用设置始终显示
    if (!m_generalGroup) {
        setupGeneralSettings();
    }
    m_mainLayout->addWidget(m_generalGroup);
    
    if (!m_currentTool) {
        showGeneralSettings();
        return;
    }
    
    // 根据工具类型显示相应的设置
    QString toolType = m_currentTool->metaObject()->className();
    
    if (toolType.contains("Brush", Qt::CaseInsensitive)) {
        showBrushSettings();
        updateBrushSettings(m_currentTool);
    } else if (toolType.contains("Pen", Qt::CaseInsensitive)) {
        showPenSettings();
        updatePenSettings(m_currentTool);
    } else if (toolType.contains("Eraser", Qt::CaseInsensitive)) {
        showEraserSettings();
        updateEraserSettings(m_currentTool);
    } else if (toolType.contains("Fill", Qt::CaseInsensitive)) {
        showFillSettings();
        updateFillSettings(m_currentTool);
    } else {
        // 形状工具
        showShapeSettings();
    }
}

void ToolsPanel::clearAllGroups()
{
    // 移除所有设置组（除了通用设置）
    if (m_brushGroup) {
        m_mainLayout->removeWidget(m_brushGroup);
        m_brushGroup->deleteLater();
        m_brushGroup = nullptr;
    }
    
    if (m_penGroup) {
        m_mainLayout->removeWidget(m_penGroup);
        m_penGroup->deleteLater();
        m_penGroup = nullptr;
    }
    
    if (m_eraserGroup) {
        m_mainLayout->removeWidget(m_eraserGroup);
        m_eraserGroup->deleteLater();
        m_eraserGroup = nullptr;
    }
    
    if (m_fillGroup) {
        m_mainLayout->removeWidget(m_fillGroup);
        m_fillGroup->deleteLater();
        m_fillGroup = nullptr;
    }
    
    if (m_shapeGroup) {
        m_mainLayout->removeWidget(m_shapeGroup);
        m_shapeGroup->deleteLater();
        m_shapeGroup = nullptr;
    }
}

void ToolsPanel::showBrushSettings()
{
    if (!m_brushGroup) {
        setupBrushSettings();
    }
    m_mainLayout->addWidget(m_brushGroup);
}

void ToolsPanel::showPenSettings()
{
    if (!m_penGroup) {
        setupPenSettings();
    }
    m_mainLayout->addWidget(m_penGroup);
}

void ToolsPanel::showEraserSettings()
{
    if (!m_eraserGroup) {
        setupEraserSettings();
    }
    m_mainLayout->addWidget(m_eraserGroup);
}

void ToolsPanel::showFillSettings()
{
    if (!m_fillGroup) {
        setupFillSettings();
    }
    m_mainLayout->addWidget(m_fillGroup);
}

void ToolsPanel::showShapeSettings()
{
    if (!m_shapeGroup) {
        setupShapeSettings();
    }
    m_mainLayout->addWidget(m_shapeGroup);
}

void ToolsPanel::showGeneralSettings()
{
    // 通用设置始终显示，无需特殊处理
}

// 槽函数实现
void ToolsPanel::onBrushSizeChanged(int size)
{
    m_brushSizeLabel->setText(QString::number(size));
    emit brushSizeChanged(size);
    emit toolSettingsChanged();
}

void ToolsPanel::onOpacityChanged(int value)
{
    m_opacityLabel->setText(QString::number(value) + "%");
    emit brushOpacityChanged(value);
    emit toolSettingsChanged();
}

void ToolsPanel::onSmoothingChanged(int value)
{
    m_smoothingLabel->setText(QString::number(value));
    emit brushSmoothingChanged(value);
    emit toolSettingsChanged();
}

void ToolsPanel::onFillToleranceChanged(int value)
{
    m_toleranceLabel->setText(QString::number(value));
    emit fillToleranceChanged(value);
    emit toolSettingsChanged();
}

void ToolsPanel::onStrokeWidthChanged(double width)
{
    emit strokeWidthChanged(width);
    emit toolSettingsChanged();
}

void ToolsPanel::onMiterLimitChanged(double limit)
{
    emit toolSettingsChanged();
}

void ToolsPanel::onJoinStyleChanged(int style)
{
    emit toolSettingsChanged();
}

void ToolsPanel::onCapStyleChanged(int style)
{
    emit toolSettingsChanged();
}

void ToolsPanel::onAntialiasingToggled(bool enabled)
{
    emit antialiasingChanged(enabled);
    emit toolSettingsChanged();
}

void ToolsPanel::onSnapToGridToggled(bool enabled)
{
    emit snapToGridChanged(enabled);
    emit toolSettingsChanged();
}

void ToolsPanel::onResetSettings()
{
    // 重置所有设置到默认值
    if (m_brushSizeSlider) m_brushSizeSlider->setValue(10);
    if (m_opacitySlider) m_opacitySlider->setValue(100);
    if (m_smoothingSlider) m_smoothingSlider->setValue(50);
    if (m_strokeWidthSpin) m_strokeWidthSpin->setValue(2.0);
    if (m_miterLimitSpin) m_miterLimitSpin->setValue(4.0);
    if (m_joinStyleCombo) m_joinStyleCombo->setCurrentIndex(0);
    if (m_capStyleCombo) m_capStyleCombo->setCurrentIndex(0);
    if (m_eraserSizeSlider) m_eraserSizeSlider->setValue(20);
    if (m_toleranceSlider) m_toleranceSlider->setValue(32);
    if (m_fillShapeCheck) m_fillShapeCheck->setChecked(false);
    if (m_shapeStrokeWidthSpin) m_shapeStrokeWidthSpin->setValue(1.0);
    
    emit toolSettingsChanged();
}

void ToolsPanel::updateBrushSettings(ToolBase *tool)
{
    DrawingToolBrush *brushTool = qobject_cast<DrawingToolBrush*>(tool);
    if (!brushTool) return;
    
    if (m_brushSizeSlider) {
        m_brushSizeSlider->setValue(brushTool->brushSizeForPanel());
    }
    if (m_opacitySlider) {
        m_opacitySlider->setValue(brushTool->brushOpacity());
    }
    if (m_smoothingSlider) {
        m_smoothingSlider->setValue(brushTool->brushSmoothingForPanel());
    }
}

void ToolsPanel::updatePenSettings(ToolBase *tool)
{
    DrawingToolPen *penTool = qobject_cast<DrawingToolPen*>(tool);
    if (!penTool) return;
    
    if (m_strokeWidthSpin) {
        m_strokeWidthSpin->setValue(penTool->strokeWidthForPanel());
    }
    // TODO: 更新压感支持状态和其他设置
}

void ToolsPanel::updateEraserSettings(ToolBase *tool)
{
    DrawingToolEraser *eraserTool = qobject_cast<DrawingToolEraser*>(tool);
    if (!eraserTool) return;
    
    if (m_eraserSizeSlider) {
        m_eraserSizeSlider->setValue(eraserTool->eraserSizeForPanel());
    }
}

void ToolsPanel::updateFillSettings(ToolBase *tool)
{
    DrawingToolFill *fillTool = qobject_cast<DrawingToolFill*>(tool);
    if (!fillTool) return;
    
    if (m_toleranceSlider) {
        m_toleranceSlider->setValue(fillTool->toleranceForPanel());
    }
}