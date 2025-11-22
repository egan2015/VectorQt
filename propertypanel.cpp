#include "propertypanel.h"
#include "drawingscene.h"
#include "drawing-shape.h"
#include <QGraphicsItem>
#include <QColorDialog>
#include <QFrame>
#include <QScrollArea>
#include <QtMath>

PropertyPanel::PropertyPanel(QWidget *parent)
    : QWidget(parent)
    , m_scene(nullptr)
    , m_updating(false)
{
    setupUI();
    setEnabled(false);
    
    // 设置现代化样式 - 支持暗色主题
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            font-size: 12px;
            border: 1px solid palette(mid);
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 3px 0 3px;
            color: palette(text);
            font-size: 12px;
        }
        QDoubleSpinBox, QSpinBox {
            border: 1px solid palette(mid);
            border-radius: 3px;
            padding: 2px;
            background: palette(base);
            font-size: 11px;
            min-height: 16px;
        }
        QDoubleSpinBox:focus, QSpinBox:focus {
            border: 1px solid palette(highlight);
        }
        QPushButton {
            background: palette(button);
            border: 1px solid palette(mid);
            border-radius: 3px;
            padding: 3px 8px;
            min-height: 18px;
            font-size: 11px;
        }
        QPushButton:hover {
            background: palette(midlight);
        }
        QPushButton:pressed {
            background: palette(mid);
        }
        QLabel {
            color: palette(text);
            font-size: 11px;
        }
    )");
}

void PropertyPanel::setScene(DrawingScene *scene)
{
    if (m_scene) {
        disconnect(m_scene, nullptr, this, nullptr);
    }
    
    m_scene = scene;
    
    if (m_scene) {
        connect(m_scene, &DrawingScene::selectionChanged, 
                this, &PropertyPanel::onSelectionChanged);
        connect(m_scene, &DrawingScene::objectStateChanged,
                this, &PropertyPanel::onObjectStateChanged);
    }
}

void PropertyPanel::setupUI()
{
    // 创建滚动区域以支持小屏幕
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget *contentWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Transform group - 更紧凑
    m_transformGroup = new QGroupBox("变换", this);
    m_transformGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    QGridLayout *transformLayout = new QGridLayout(m_transformGroup);
    transformLayout->setSpacing(4);
    transformLayout->setContentsMargins(8, 15, 8, 8);
    
    // Position section - 简化
    transformLayout->addWidget(new QLabel("X:"), 0, 0);
    m_xSpinBox = new QDoubleSpinBox();
    m_xSpinBox->setRange(-10000, 10000);
    m_xSpinBox->setDecimals(1);
    m_xSpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_xSpinBox->setMinimumWidth(60);
    transformLayout->addWidget(m_xSpinBox, 0, 1);
    
    transformLayout->addWidget(new QLabel("Y:"), 0, 2);
    m_ySpinBox = new QDoubleSpinBox();
    m_ySpinBox->setRange(-10000, 10000);
    m_ySpinBox->setDecimals(1);
    m_ySpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_ySpinBox->setMinimumWidth(60);
    transformLayout->addWidget(m_ySpinBox, 0, 3);
    
    // Size section
    transformLayout->addWidget(new QLabel("宽度:"), 1, 0);
    m_widthSpinBox = new QDoubleSpinBox();
    m_widthSpinBox->setRange(0, 10000);
    m_widthSpinBox->setDecimals(1);
    m_widthSpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_widthSpinBox->setMinimumWidth(60);
    transformLayout->addWidget(m_widthSpinBox, 1, 1);
    
    transformLayout->addWidget(new QLabel("高度:"), 1, 2);
    m_heightSpinBox = new QDoubleSpinBox();
    m_heightSpinBox->setRange(0, 10000);
    m_heightSpinBox->setDecimals(1);
    m_heightSpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_heightSpinBox->setMinimumWidth(60);
    transformLayout->addWidget(m_heightSpinBox, 1, 3);
    
    // Rotation section
    transformLayout->addWidget(new QLabel("旋转:"), 2, 0);
    m_rotationSpinBox = new QDoubleSpinBox();
    m_rotationSpinBox->setRange(-360, 360);
    m_rotationSpinBox->setDecimals(1);
    m_rotationSpinBox->setSuffix("°");
    m_rotationSpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_rotationSpinBox->setMinimumWidth(60);
    transformLayout->addWidget(m_rotationSpinBox, 2, 1, 1, 2);
    
    mainLayout->addWidget(m_transformGroup);
    
    // Appearance group - 更紧凑
    m_appearanceGroup = new QGroupBox("外观", this);
    m_appearanceGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    QGridLayout *appearanceLayout = new QGridLayout(m_appearanceGroup);
    appearanceLayout->setSpacing(4);
    appearanceLayout->setContentsMargins(8, 15, 8, 8);
    
    appearanceLayout->addWidget(new QLabel("填充:"), 0, 0);
    m_fillColorButton = new QPushButton("选择颜色");
    m_fillColorButton->setMinimumHeight(24);
    m_fillColorButton->setMinimumWidth(70);
    appearanceLayout->addWidget(m_fillColorButton, 0, 1);
    
    appearanceLayout->addWidget(new QLabel("描边:"), 1, 0);
    m_strokeColorButton = new QPushButton("选择颜色");
    m_strokeColorButton->setMinimumHeight(24);
    m_strokeColorButton->setMinimumWidth(70);
    appearanceLayout->addWidget(m_strokeColorButton, 1, 1);
    
    // Stroke properties
    appearanceLayout->addWidget(new QLabel("描边宽度:"), 2, 0);
    m_strokeWidthSpinBox = new QSpinBox();
    m_strokeWidthSpinBox->setRange(0, 100);
    m_strokeWidthSpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_strokeWidthSpinBox->setMinimumWidth(70);
    appearanceLayout->addWidget(m_strokeWidthSpinBox, 2, 1);
    
    appearanceLayout->addWidget(new QLabel("线型:"), 3, 0);
    m_strokeStyleComboBox = new QComboBox();
    m_strokeStyleComboBox->addItem("实线", static_cast<int>(Qt::SolidLine));
    m_strokeStyleComboBox->addItem("虚线", static_cast<int>(Qt::DashLine));
    m_strokeStyleComboBox->addItem("点线", static_cast<int>(Qt::DotLine));
    m_strokeStyleComboBox->addItem("点划线", static_cast<int>(Qt::DashDotLine));
    m_strokeStyleComboBox->addItem("双点划线", static_cast<int>(Qt::DashDotDotLine));
    m_strokeStyleComboBox->setMinimumWidth(70);
    appearanceLayout->addWidget(m_strokeStyleComboBox, 3, 1);
    
    appearanceLayout->addWidget(new QLabel("不透明度:"), 4, 0);
    m_opacitySpinBox = new QDoubleSpinBox();
    m_opacitySpinBox->setRange(0, 1);
    m_opacitySpinBox->setDecimals(2);
    m_opacitySpinBox->setSingleStep(0.1);
    m_opacitySpinBox->setValue(1.0);
    m_opacitySpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    m_opacitySpinBox->setMinimumWidth(70);
    appearanceLayout->addWidget(m_opacitySpinBox, 4, 1);
    
    mainLayout->addWidget(m_appearanceGroup);
    
    // Apply button with modern styling - 更紧凑
    m_applyButton = new QPushButton("应用更改", this);
    m_applyButton->setMinimumHeight(28);
    m_applyButton->setStyleSheet(R"(
        QPushButton {
            background: palette(highlight);
            color: palette(highlighted-text);
            border: none;
            border-radius: 4px;
            font-weight: bold;
            font-size: 11px;
        }
        QPushButton:hover {
            background: palette(highlight).lighter(120);
        }
        QPushButton:pressed {
            background: palette(highlight).darker(120);
        }
    )");
    mainLayout->addWidget(m_applyButton);
    
    mainLayout->addStretch(0); // 减少底部间距
    
    scrollArea->setWidget(contentWidget);
    
    // 主布局
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);
    
    // Connect signals
    connect(m_xSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertyPanel::onPositionChanged);
    connect(m_ySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertyPanel::onPositionChanged);
    connect(m_widthSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertyPanel::onSizeChanged);
    connect(m_heightSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertyPanel::onSizeChanged);
    connect(m_rotationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertyPanel::onRotationChanged);
    connect(m_fillColorButton, &QPushButton::clicked,
            this, &PropertyPanel::onFillColorChanged);
    connect(m_strokeColorButton, &QPushButton::clicked,
            this, &PropertyPanel::onStrokeColorChanged);
    connect(m_strokeWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PropertyPanel::onStrokeWidthChanged);
    connect(m_strokeStyleComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PropertyPanel::onStrokeStyleChanged);
    connect(m_opacitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertyPanel::onOpacityChanged);
    connect(m_applyButton, &QPushButton::clicked,
            this, &PropertyPanel::onApplyClicked);
}

void PropertyPanel::onSelectionChanged()
{
    if (!m_scene) {
        setEnabled(false);
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    
    if (selected.isEmpty()) {
        setEnabled(false);
        return;
    }
    
    setEnabled(true);
    updateValues();
}

void PropertyPanel::onObjectStateChanged(DrawingShape* shape)
{
    // 检查这个形状是否是当前选中的形状
    if (!m_scene || m_updating) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        if (item == shape) {
            // 这是当前选中的形状，更新属性面板显示
            updateValues();
        }
    }
}

void PropertyPanel::updateValues()
{
    if (!m_scene || m_updating) {
        return;
    }
    
    m_updating = true;
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        
        QRectF bounds = item->boundingRect();
        QPointF pos = item->pos();
        
        m_xSpinBox->setValue(pos.x());
        m_ySpinBox->setValue(pos.y());
        m_widthSpinBox->setValue(bounds.width());
        m_heightSpinBox->setValue(bounds.height());
        
        // 获取旋转角度
        if (shape) {
            qreal rotationRad = shape->transform().rotation();
            qreal rotationDeg = rotationRad * 180.0 / M_PI;
            m_rotationSpinBox->setValue(rotationDeg);
        } else {
            m_rotationSpinBox->setValue(item->rotation());
        }
        
        if (shape) {
            // 更新DrawingShape特有的属性
            QBrush fillBrush = shape->fillBrush();
            QPen strokePen = shape->strokePen();
            
            // 设置填充颜色按钮
            QString fillColorStyle = QString("background-color: %1").arg(fillBrush.color().name());
            m_fillColorButton->setStyleSheet(fillColorStyle);
            
            // 设置描边颜色按钮
            QString strokeColorStyle = QString("background-color: %1").arg(strokePen.color().name());
            m_strokeColorButton->setStyleSheet(strokeColorStyle);
            
            // 设置描边宽度
            m_strokeWidthSpinBox->setValue(qRound(strokePen.widthF()));
            
            // 设置线型
            int styleValue = static_cast<int>(strokePen.style());
            for (int i = 0; i < m_strokeStyleComboBox->count(); ++i) {
                if (m_strokeStyleComboBox->itemData(i).toInt() == styleValue) {
                    m_strokeStyleComboBox->setCurrentIndex(i);
                    break;
                }
            }
            
            // 设置不透明度
            m_opacitySpinBox->setValue(item->opacity());
        }
    }
    
    m_updating = false;
}

void PropertyPanel::setEnabled(bool enabled)
{
    m_transformGroup->setEnabled(enabled);
    m_appearanceGroup->setEnabled(enabled);
    m_applyButton->setEnabled(enabled);
    QWidget::setEnabled(enabled);
}

void PropertyPanel::onPositionChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        item->setPos(m_xSpinBox->value(), m_ySpinBox->value());
        m_scene->setModified(true);
    }
}

void PropertyPanel::onSizeChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        // This is a simplified implementation
        // In a real application, you would need to handle different item types
        m_scene->setModified(true);
    }
}

void PropertyPanel::onRotationChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        
        if (shape) {
            // 对于DrawingShape，使用DrawingTransform设置旋转
            DrawingTransform transform = shape->transform();
            QPointF center = shape->boundingRect().center();
            qreal angleDeg = m_rotationSpinBox->value();
            qreal angleRad = angleDeg * M_PI / 180.0;
            transform.rotate(angleRad, center);
            shape->setTransform(transform);
        } else {
            // 对于其他图形项，使用标准旋转
            item->setRotation(m_rotationSpinBox->value());
        }
        
        m_scene->setModified(true);
    }
}

void PropertyPanel::onFillColorChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            QColor color = QColorDialog::getColor(shape->fillBrush().color(), this, "选择填充颜色");
            if (color.isValid()) {
                shape->setFillBrush(QBrush(color));
                QString colorStyle = QString("background-color: %1").arg(color.name());
                m_fillColorButton->setStyleSheet(colorStyle);
                m_scene->setModified(true);
            }
        }
    }
}

void PropertyPanel::onStrokeColorChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            QColor color = QColorDialog::getColor(shape->strokePen().color(), this, "选择描边颜色");
            if (color.isValid()) {
                QPen pen = shape->strokePen();
                pen.setColor(color);
                shape->setStrokePen(pen);
                QString colorStyle = QString("background-color: %1").arg(color.name());
                m_strokeColorButton->setStyleSheet(colorStyle);
                m_scene->setModified(true);
            }
        }
    }
}

void PropertyPanel::onStrokeWidthChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            QPen pen = shape->strokePen();
            pen.setWidth(m_strokeWidthSpinBox->value());
            shape->setStrokePen(pen);
            m_scene->setModified(true);
        }
    }
}

void PropertyPanel::onStrokeStyleChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item);
        if (shape) {
            Qt::PenStyle style = static_cast<Qt::PenStyle>(m_strokeStyleComboBox->currentData().toInt());
            QPen pen = shape->strokePen();
            pen.setStyle(style);
            shape->setStrokePen(pen);
            m_scene->setModified(true);
        }
    }
}

void PropertyPanel::onOpacityChanged()
{
    if (m_updating || !m_scene) {
        return;
    }
    
    QList<QGraphicsItem*> selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        QGraphicsItem *item = selected.first();
        item->setOpacity(m_opacitySpinBox->value());
        m_scene->setModified(true);
    }
}

void PropertyPanel::onApplyClicked()
{
    if (!m_scene) {
        return;
    }
    
    m_scene->setModified(true);
}
