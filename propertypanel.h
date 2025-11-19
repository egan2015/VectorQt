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
};

#endif // PROPERTYPANEL_H
