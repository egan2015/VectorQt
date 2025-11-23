#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QColorDialog>
#include <QGroupBox>

class DrawingScene;
class DrawingShape;

class PropertyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget *parent = nullptr);
    
    void setScene(DrawingScene *scene);

public slots:
    void onSelectionChanged();
    void onObjectStateChanged(DrawingShape* shape);

private slots:
    void onPositionChanged();
    void onSizeChanged();
    void onRotationChanged();
    void onFillColorChanged();
    void onStrokeColorChanged();
    void onStrokeWidthChanged();
    void onStrokeStyleChanged();
    void onOpacityChanged();
    void onApplyClicked();

private:
    void setupUI();
    void updateValues();
    void setEnabled(bool enabled);
    
    DrawingScene *m_scene;
    
    // Transform group
    QGroupBox *m_transformGroup;
    QDoubleSpinBox *m_xSpinBox;
    QDoubleSpinBox *m_ySpinBox;
    QDoubleSpinBox *m_widthSpinBox;
    QDoubleSpinBox *m_heightSpinBox;
    QDoubleSpinBox *m_rotationSpinBox;
    
    // Appearance group
    QGroupBox *m_appearanceGroup;
    QPushButton *m_fillColorButton;
    QPushButton *m_strokeColorButton;
    QSpinBox *m_strokeWidthSpinBox;
    QComboBox *m_strokeStyleComboBox;
    QDoubleSpinBox *m_opacitySpinBox;
    
    QPushButton *m_applyButton;
    
    bool m_updating;
    double m_lastKnownRotation;  // 用于递归检测和微小变化过滤
    double m_lastKnownWidth;     // 用于宽度变化检测
    double m_lastKnownHeight;    // 用于高度变化检测
};

#endif // PROPERTYPANEL_H
