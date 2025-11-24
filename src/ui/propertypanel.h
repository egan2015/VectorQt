#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include <QWidget>
#include <QGraphicsItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QTimer>

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
    
    // Transform components
    QGroupBox *m_transformGroup;
    QGridLayout *m_transformLayout;
    
    // Transform controls
    QDoubleSpinBox *m_xSpinBox;
    QDoubleSpinBox *m_ySpinBox;
    QDoubleSpinBox *m_widthSpinBox;
    QDoubleSpinBox *m_heightSpinBox;
    QDoubleSpinBox *m_rotationSpinBox;
    
    // Appearance controls
    QGroupBox *m_appearanceGroup;
    QGridLayout *m_appearanceLayout;
    QPushButton *m_fillColorButton;
    QPushButton *m_strokeColorButton;
    QSpinBox *m_strokeWidthSpinBox;
    QComboBox *m_strokeStyleComboBox;
    QDoubleSpinBox *m_opacitySpinBox;
    
    // Performance monitoring
    QGroupBox *m_performanceGroup;
    QLabel *m_fpsLabel;
    QLabel *m_memoryLabel;
    QLabel *m_drawCallsLabel;
    QTimer *m_performanceTimer;
    
    // Apply button
    QPushButton *m_applyButton;
    
    // State tracking
    bool m_updating;
    double m_lastKnownRotation;
    double m_lastKnownWidth;
    double m_lastKnownHeight;
};

#endif // PROPERTYPANEL_H
