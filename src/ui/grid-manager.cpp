#include "grid-manager.h"
#include "drawingscene.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QInputDialog>
#include <QColorDialog>
#include <QApplication>
#include <cmath>

GridManager::GridManager(DrawingScene *scene, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_gridVisible(true)
    , m_gridSize(20.0)
    , m_gridColor(QColor(200, 200, 200, 100))
    , m_gridStyle(Qt::DotLine)
    , m_gridOpacity(0.5)
    , m_gridAlignmentEnabled(true)
    , m_snapEnabled(true)
    , m_objectSnapEnabled(true)
    , m_snapTolerance(3.0)
    , m_objectSnapTolerance(3.0)
    , m_gridCacheValid(false)
{
    if (m_scene) {
        // TODO: 需要DrawingScene添加合适的信号
        // connect(m_scene, &DrawingScene::sceneRectChanged, this, &GridManager::onSceneChanged);
        updateGrid();
    }
}

GridManager::~GridManager()
{
}

void GridManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
    if (m_scene) {
        updateGrid();
    }
}

DrawingScene* GridManager::scene() const
{
    return m_scene;
}

void GridManager::setGridVisible(bool visible)
{
    if (m_gridVisible != visible) {
        m_gridVisible = visible;
        m_gridCacheValid = false;
        updateGrid();
        emit gridVisibilityChanged(visible);
        emit statusMessageChanged(visible ? "网格已显示" : "网格已隐藏");
    }
}

bool GridManager::isGridVisible() const
{
    return m_gridVisible;
}

void GridManager::toggleGrid()
{
    setGridVisible(!m_gridVisible);
}

void GridManager::setGridSize(qreal size)
{
    if (qAbs(m_gridSize - size) > 0.01) {
        m_gridSize = qMax(1.0, size);
        m_gridCacheValid = false;
        updateGrid();
        emit gridSettingsChanged();
        emit statusMessageChanged(QString("网格大小: %1").arg(m_gridSize));
    }
}

qreal GridManager::gridSize() const
{
    return m_gridSize;
}

void GridManager::setGridColor(const QColor &color)
{
    if (m_gridColor != color) {
        m_gridColor = color;
        m_gridCacheValid = false;
        updateGrid();
        emit gridSettingsChanged();
    }
}

QColor GridManager::gridColor() const
{
    return m_gridColor;
}

void GridManager::setGridStyle(Qt::PenStyle style)
{
    if (m_gridStyle != style) {
        m_gridStyle = style;
        m_gridCacheValid = false;
        updateGrid();
        emit gridSettingsChanged();
    }
}

Qt::PenStyle GridManager::gridStyle() const
{
    return m_gridStyle;
}

void GridManager::setGridOpacity(qreal opacity)
{
    opacity = qBound(0.0, opacity, 1.0);
    if (qAbs(m_gridOpacity - opacity) > 0.01) {
        m_gridOpacity = opacity;
        m_gridCacheValid = false;
        updateGrid();
        emit gridSettingsChanged();
    }
}

qreal GridManager::gridOpacity() const
{
    return m_gridOpacity;
}

void GridManager::setGridAlignmentEnabled(bool enabled)
{
    if (m_gridAlignmentEnabled != enabled) {
        m_gridAlignmentEnabled = enabled;
        emit gridAlignmentChanged(enabled);
        emit statusMessageChanged(enabled ? "网格对齐已启用" : "网格对齐已禁用");
    }
}

bool GridManager::isGridAlignmentEnabled() const
{
    return m_gridAlignmentEnabled;
}

void GridManager::toggleGridAlignment()
{
    setGridAlignmentEnabled(!m_gridAlignmentEnabled);
}

void GridManager::setSnapEnabled(bool enabled)
{
    if (m_snapEnabled != enabled) {
        m_snapEnabled = enabled;
        emit snapChanged(enabled);
        emit statusMessageChanged(enabled ? "智能吸附已启用" : "智能吸附已禁用");
    }
}

bool GridManager::isSnapEnabled() const
{
    return m_snapEnabled;
}

void GridManager::toggleSnap()
{
    setSnapEnabled(!m_snapEnabled);
}

void GridManager::setObjectSnapEnabled(bool enabled)
{
    if (m_objectSnapEnabled != enabled) {
        m_objectSnapEnabled = enabled;
        emit snapChanged(enabled);
        emit statusMessageChanged(enabled ? "对象吸附已启用" : "对象吸附已禁用");
    }
}

bool GridManager::isObjectSnapEnabled() const
{
    return m_objectSnapEnabled;
}

void GridManager::toggleObjectSnap()
{
    setObjectSnapEnabled(!m_objectSnapEnabled);
}

void GridManager::setSnapTolerance(qreal tolerance)
{
    m_snapTolerance = qMax(1.0, tolerance);
}

qreal GridManager::snapTolerance() const
{
    return m_snapTolerance;
}

void GridManager::setObjectSnapTolerance(qreal tolerance)
{
    m_objectSnapTolerance = qMax(1.0, tolerance);
}

qreal GridManager::objectSnapTolerance() const
{
    return m_objectSnapTolerance;
}

QPointF GridManager::snapToGrid(const QPointF &point) const
{
    if (!m_gridVisible || !m_gridAlignmentEnabled) {
        return point;
    }
    
    return calculateGridPoint(point);
}

QPointF GridManager::snapToNearest(const QPointF &point, const QList<QPointF>& snapPoints) const
{
    if (snapPoints.isEmpty()) {
        return point;
    }
    
    return findNearestSnapPoint(point, snapPoints, m_snapTolerance);
}

QPointF GridManager::snapToGridAlignment(const QPointF &point) const
{
    if (!m_gridVisible || !m_gridAlignmentEnabled) {
        return point;
    }
    
    QPointF gridPoint = calculateGridPoint(point);
    
    // 检查是否足够接近网格线
    if (isNearGridLine(point, m_snapTolerance, gridPoint)) {
        return gridPoint;
    }
    
    return point;
}

void GridManager::showGridSettings()
{
    // 创建设置对话框
    QDialog dialog;
    dialog.setWindowTitle("网格设置");
    dialog.setModal(true);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    // 网格显示
    QCheckBox *visibleCheckBox = new QCheckBox("显示网格");
    visibleCheckBox->setChecked(m_gridVisible);
    layout->addWidget(visibleCheckBox);
    
    // 网格大小
    QHBoxLayout *sizeLayout = new QHBoxLayout();
    QLabel *sizeLabel = new QLabel("网格大小:");
    QDoubleSpinBox *sizeSpinBox = new QDoubleSpinBox();
    sizeSpinBox->setRange(1.0, 100.0);
    sizeSpinBox->setValue(m_gridSize);
    sizeSpinBox->setSingleStep(1.0);
    sizeLayout->addWidget(sizeLabel);
    sizeLayout->addWidget(sizeSpinBox);
    layout->addLayout(sizeLayout);
    
    // 网格颜色
    QHBoxLayout *colorLayout = new QHBoxLayout();
    QLabel *colorLabel = new QLabel("网格颜色:");
    QPushButton *colorButton = new QPushButton();
    colorButton->setStyleSheet(QString("background-color: %1;").arg(m_gridColor.name()));
    colorLayout->addWidget(colorLabel);
    colorLayout->addWidget(colorButton);
    layout->addLayout(colorLayout);
    
    // 网格样式
    QHBoxLayout *styleLayout = new QHBoxLayout();
    QLabel *styleLabel = new QLabel("网格样式:");
    QComboBox *styleComboBox = new QComboBox();
    styleComboBox->addItem("实线", static_cast<int>(Qt::SolidLine));
    styleComboBox->addItem("虚线", static_cast<int>(Qt::DashLine));
    styleComboBox->addItem("点线", static_cast<int>(Qt::DotLine));
    styleComboBox->addItem("点划线", static_cast<int>(Qt::DashDotLine));
    styleComboBox->setCurrentIndex(static_cast<int>(m_gridStyle));
    styleLayout->addWidget(styleLabel);
    styleLayout->addWidget(styleComboBox);
    layout->addLayout(styleLayout);
    
    // 网格透明度
    QHBoxLayout *opacityLayout = new QHBoxLayout();
    QLabel *opacityLabel = new QLabel("网格透明度:");
    QSlider *opacitySlider = new QSlider(Qt::Horizontal);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(static_cast<int>(m_gridOpacity * 100));
    opacityLabel->setText(QString("%1%").arg(static_cast<int>(m_gridOpacity * 100)));
    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(opacitySlider);
    layout->addLayout(opacityLayout);
    
    // 对齐设置
    QGroupBox *alignmentGroup = new QGroupBox("对齐设置");
    QVBoxLayout *alignmentLayout = new QVBoxLayout(alignmentGroup);
    
    QCheckBox *gridAlignmentCheckBox = new QCheckBox("网格对齐");
    gridAlignmentCheckBox->setChecked(m_gridAlignmentEnabled);
    alignmentLayout->addWidget(gridAlignmentCheckBox);
    
    QCheckBox *snapCheckBox = new QCheckBox("智能吸附");
    snapCheckBox->setChecked(m_snapEnabled);
    alignmentLayout->addWidget(snapCheckBox);
    
    QCheckBox *objectSnapCheckBox = new QCheckBox("对象吸附");
    objectSnapCheckBox->setChecked(m_objectSnapEnabled);
    alignmentLayout->addWidget(objectSnapCheckBox);
    
    layout->addWidget(alignmentGroup);
    
    // 吸附容差
    QGroupBox *toleranceGroup = new QGroupBox("吸附容差");
    QVBoxLayout *toleranceLayout = new QVBoxLayout(toleranceGroup);
    
    QHBoxLayout *snapToleranceLayout = new QHBoxLayout();
    QLabel *snapToleranceLabel = new QLabel("网格吸附容差:");
    QSpinBox *snapToleranceSpinBox = new QSpinBox();
    snapToleranceSpinBox->setRange(1, 20);
    snapToleranceSpinBox->setValue(static_cast<int>(m_snapTolerance));
    snapToleranceLayout->addWidget(snapToleranceLabel);
    snapToleranceLayout->addWidget(snapToleranceSpinBox);
    toleranceLayout->addLayout(snapToleranceLayout);
    
    QHBoxLayout *objectToleranceLayout = new QHBoxLayout();
    QLabel *objectToleranceLabel = new QLabel("对象吸附容差:");
    QSpinBox *objectToleranceSpinBox = new QSpinBox();
    objectToleranceSpinBox->setRange(1, 20);
    objectToleranceSpinBox->setValue(static_cast<int>(m_objectSnapTolerance));
    objectToleranceLayout->addWidget(objectToleranceLabel);
    objectToleranceLayout->addWidget(objectToleranceSpinBox);
    toleranceLayout->addLayout(objectToleranceLayout);
    
    layout->addWidget(toleranceGroup);
    
    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("确定");
    QPushButton *cancelButton = new QPushButton("取消");
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(visibleCheckBox, &QCheckBox::toggled, this, &GridManager::setGridVisible);
    connect(sizeSpinBox, &QDoubleSpinBox::valueChanged, 
            this, [this, sizeSpinBox]() { setGridSize(sizeSpinBox->value()); });
    connect(colorButton, &QPushButton::clicked, this, [this, colorButton]() {
        QColor color = QColorDialog::getColor(m_gridColor, nullptr, "选择网格颜色");
        if (color.isValid()) {
            setGridColor(color);
            colorButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));
        }
    });
    connect(styleComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, [this, styleComboBox](int index) { 
                QVariant variant = styleComboBox->itemData(index);
                if (variant.isValid()) {
                    setGridStyle(static_cast<Qt::PenStyle>(variant.toInt()));
                }
            });
    connect(opacitySlider, &QSlider::valueChanged, this, [this, opacityLabel](int value) {
        setGridOpacity(value / 100.0);
        opacityLabel->setText(QString("%1%").arg(value));
    });
    connect(gridAlignmentCheckBox, &QCheckBox::toggled, this, &GridManager::setGridAlignmentEnabled);
    connect(snapCheckBox, &QCheckBox::toggled, this, &GridManager::setSnapEnabled);
    connect(objectSnapCheckBox, &QCheckBox::toggled, this, &GridManager::setObjectSnapEnabled);
    connect(snapToleranceSpinBox, &QSpinBox::valueChanged, 
            this, [this, snapToleranceSpinBox]() { setSnapTolerance(snapToleranceSpinBox->value()); });
    connect(objectToleranceSpinBox, &QSpinBox::valueChanged, 
            this, [this, objectToleranceSpinBox]() { setObjectSnapTolerance(objectToleranceSpinBox->value()); });
    
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        emit statusMessageChanged("网格设置已更新");
    }
}

void GridManager::onSceneChanged()
{
    // 场景变化时更新网格
    updateGrid();
}

void GridManager::updateGrid()
{
    if (!m_scene) {
        return;
    }
    
    m_gridCacheValid = false;
    // 触发场景更新以重绘网格
    m_scene->update();
}

QPointF GridManager::calculateGridPoint(const QPointF &point) const
{
    if (m_gridSize <= 0) {
        return point;
    }
    
    qreal x = qRound(point.x() / m_gridSize) * m_gridSize;
    qreal y = qRound(point.y() / m_gridSize) * m_gridSize;
    
    return QPointF(x, y);
}

bool GridManager::isNearGridLine(const QPointF &point, qreal tolerance, QPointF &gridPoint) const
{
    gridPoint = calculateGridPoint(point);
    
    // 检查垂直网格线
    if (qAbs(point.x() - gridPoint.x()) <= tolerance) {
        gridPoint.setX(gridPoint.x());
        return true;
    }
    
    // 检查水平网格线
    if (qAbs(point.y() - gridPoint.y()) <= tolerance) {
        gridPoint.setY(gridPoint.y());
        return true;
    }
    
    return false;
}

QPointF GridManager::findNearestSnapPoint(const QPointF &point, const QList<QPointF>& snapPoints, qreal tolerance) const
{
    QPointF nearestPoint = point;
    qreal minDistance = tolerance;
    
    for (const QPointF &snapPoint : snapPoints) {
        qreal distance = QLineF(point, snapPoint).length();
        if (distance < minDistance) {
            minDistance = distance;
            nearestPoint = snapPoint;
        }
    }
    
    return nearestPoint;
}