#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAction>
#include <QColorDialog>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QFrame>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include "../ui/colorpalette.h"
#include "../ui/drawingscene.h"

ColorPalette::ColorPalette(QWidget *parent)
    : QWidget(parent)
    , m_scene(nullptr)
    , m_fillColorButton(nullptr)
    , m_strokeColorButton(nullptr)
    , m_fillColorLabel(nullptr)
    , m_strokeColorLabel(nullptr)
    , m_currentFillColor(Qt::blue)
    , m_currentStrokeColor(Qt::black)
{
    setupUI();
    createColorActions();
}

void ColorPalette::setScene(DrawingScene *scene)
{
    m_scene = scene;
}

void ColorPalette::setupUI()
{
    // 主布局 - 垂直布局，两行分别用于填充色和边框色
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(2);
    
    // 第一行：填充色区域
    QHBoxLayout *fillLayout = new QHBoxLayout();
    fillLayout->setContentsMargins(0, 0, 0, 0);
    fillLayout->setSpacing(4);
    
    // 填充色标签和当前颜色按钮
    m_fillColorLabel = new QLabel("填充");
    m_fillColorLabel->setStyleSheet("font-weight: bold; font-size: 10px; padding: 0 8px;");
    fillLayout->addWidget(m_fillColorLabel);
    
    m_fillColorButton = new QPushButton();
    m_fillColorButton->setFixedSize(28, 20);
    m_fillColorButton->setToolTip("点击选择填充颜色");
    updateColorButton(m_fillColorButton, m_currentFillColor);
    connect(m_fillColorButton, &QPushButton::clicked, this, &ColorPalette::onFillColorButtonClicked);
    fillLayout->addWidget(m_fillColorButton);
    
    // 分隔符
    QFrame *separator1 = new QFrame();
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    separator1->setFixedWidth(1);
    fillLayout->addWidget(separator1);
    
    // 填充色颜色选择区域 - 使用可拖动的滚动区域
    DraggableScrollArea *fillScrollArea = new DraggableScrollArea();
    fillScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fillScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fillScrollArea->setFixedHeight(24);
    fillScrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget *fillColorWidget = new QWidget();
    QHBoxLayout *fillColorLayout = new QHBoxLayout(fillColorWidget);
    fillColorLayout->setContentsMargins(0, 0, 0, 0);
    fillColorLayout->setSpacing(0);
    
    // 填充色颜色按钮将在这里动态添加
    m_fillColorArea = fillColorWidget;
    fillScrollArea->setWidget(fillColorWidget);
    fillScrollArea->setWidgetResizable(true);
    
    fillLayout->addWidget(fillScrollArea);
    
    mainLayout->addLayout(fillLayout);
    
    // 第二行：边框色区域
    QHBoxLayout *strokeLayout = new QHBoxLayout();
    strokeLayout->setContentsMargins(0, 0, 0, 0);
    strokeLayout->setSpacing(4);
    
    // 边框色标签和当前颜色按钮
    m_strokeColorLabel = new QLabel("边框");
    m_strokeColorLabel->setStyleSheet("font-weight: bold; font-size: 10px; padding: 0 8px;");
    strokeLayout->addWidget(m_strokeColorLabel);
    
    m_strokeColorButton = new QPushButton();
    m_strokeColorButton->setFixedSize(28, 20);
    m_strokeColorButton->setToolTip("点击选择边框颜色");
    updateColorButton(m_strokeColorButton, m_currentStrokeColor);
    connect(m_strokeColorButton, &QPushButton::clicked, this, &ColorPalette::onStrokeColorButtonClicked);
    strokeLayout->addWidget(m_strokeColorButton);
    
    // 分隔符
    QFrame *separator2 = new QFrame();
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    separator2->setFixedWidth(1);
    strokeLayout->addWidget(separator2);
    
    // 边框色颜色选择区域 - 使用可拖动的滚动区域
    DraggableScrollArea *strokeScrollArea = new DraggableScrollArea();
    strokeScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    strokeScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    strokeScrollArea->setFixedHeight(24);
    strokeScrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget *strokeColorWidget = new QWidget();
    QHBoxLayout *strokeColorLayout = new QHBoxLayout(strokeColorWidget);
    strokeColorLayout->setContentsMargins(0, 0, 0, 0);
    strokeColorLayout->setSpacing(0);
    
    // 边框色颜色按钮将在这里动态添加
    m_strokeColorArea = strokeColorWidget;
    strokeScrollArea->setWidget(strokeColorWidget);
    strokeScrollArea->setWidgetResizable(true);
    
    strokeLayout->addWidget(strokeScrollArea);
    
    
    
    mainLayout->addLayout(strokeLayout);
    
    // 设置整体样式
    setStyleSheet(R"(
        ColorPalette {
            background: palette(window);
            border: 1px solid palette(mid);
            border-radius: 3px;
        }
        QPushButton {
            border: 1px solid palette(text);
            border-radius: 2px;
        }
        QPushButton:hover {
            border: 2px solid palette(highlight);
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QToolButton {
            background: palette(button);
            border: 1px solid palette(mid);
            border-radius: 2px;
        }
        QToolButton:hover {
            background: palette(light);
            border: 1px solid palette(highlight);
        }
    )");
}

void ColorPalette::createColorActions()
{
    // 获取W3C标准颜色
    m_w3cColors = getW3CColors();
    
    // 为填充色创建颜色按钮
    QHBoxLayout *fillColorLayout = qobject_cast<QHBoxLayout*>(m_fillColorArea->layout());
    if (fillColorLayout) {
        // 添加无填充选项（第一个位置）
        QPushButton *noFillButton = new QPushButton();
        noFillButton->setFixedSize(18, 18);
        noFillButton->setToolTip("无填充");
        
        connect(noFillButton, &QPushButton::clicked, [this]() {
            selectFillColor(QColor(Qt::transparent));
        });
        
        fillColorLayout->addWidget(noFillButton);
        
        // 设置无填充按钮的初始样式
        updateColorButton(noFillButton, QColor(Qt::transparent));
        
        for (const NamedColor &namedColor : m_w3cColors) {
            QPushButton *colorButton = new QPushButton();
            colorButton->setFixedSize(18, 18);
            colorButton->setStyleSheet(QString("background-color: %1; border: none;").arg(namedColor.color.name()));
            colorButton->setToolTip(namedColor.name);
            
            connect(colorButton, &QPushButton::clicked, [this, namedColor]() {
                selectFillColor(namedColor.color);
            });
            
            fillColorLayout->addWidget(colorButton);
        }
        
        // 设置填充色区域的最小宽度，确保颜色按钮可见（包括无填充按钮和分隔线）
        m_fillColorArea->setMinimumWidth((m_w3cColors.size() + 1) * 20); // 额外2个位置：无填充按钮和分隔线，按钮大小18px
    }
    
    // 为边框色创建颜色按钮
    QHBoxLayout *strokeColorLayout = qobject_cast<QHBoxLayout*>(m_strokeColorArea->layout());
    if (strokeColorLayout) {
        // 添加无边框选项（第一个位置）
        QPushButton *noStrokeButton = new QPushButton();
        noStrokeButton->setFixedSize(18, 18);
        noStrokeButton->setToolTip("无边框");
        
        connect(noStrokeButton, &QPushButton::clicked, [this]() {
            selectStrokeColor(QColor(Qt::transparent));
        });
        
        strokeColorLayout->addWidget(noStrokeButton);
        
        // 设置无边框按钮的初始样式
        updateColorButton(noStrokeButton, QColor(Qt::transparent));
        
        for (const NamedColor &namedColor : m_w3cColors) {
            QPushButton *colorButton = new QPushButton();
            colorButton->setFixedSize(18, 18);
            colorButton->setStyleSheet(QString("background-color: %1; border: none;").arg(namedColor.color.name()));
            colorButton->setToolTip(namedColor.name);
            
            connect(colorButton, &QPushButton::clicked, [this, namedColor]() {
                selectStrokeColor(namedColor.color);
            });
            
            strokeColorLayout->addWidget(colorButton);
        }
        
        // 设置边框色区域的最小宽度，确保颜色按钮可见（包括无边框按钮和分隔线）
        m_strokeColorArea->setMinimumWidth((m_w3cColors.size() + 1) * 20); // 额外2个位置：无边框按钮和分隔线，按钮大小18px
    }
}

QList<ColorPalette::NamedColor> ColorPalette::getW3CColors() const
{
    // W3C标准颜色名称和颜色值
    QList<NamedColor> colors = {
        {"AliceBlue", QColor(240, 248, 255)},
        {"AntiqueWhite", QColor(250, 235, 215)},
        {"Aqua", QColor(0, 255, 255)},
        {"Aquamarine", QColor(127, 255, 212)},
        {"Azure", QColor(240, 255, 255)},
        {"Beige", QColor(245, 245, 220)},
        {"Bisque", QColor(255, 228, 196)},
        {"Black", QColor(0, 0, 0)},
        {"BlanchedAlmond", QColor(255, 235, 205)},
        {"Blue", QColor(0, 0, 255)},
        {"BlueViolet", QColor(138, 43, 226)},
        {"Brown", QColor(165, 42, 42)},
        {"BurlyWood", QColor(222, 184, 135)},
        {"CadetBlue", QColor(95, 158, 160)},
        {"Chartreuse", QColor(127, 255, 0)},
        {"Chocolate", QColor(210, 105, 30)},
        {"Coral", QColor(255, 127, 80)},
        {"CornflowerBlue", QColor(100, 149, 237)},
        {"Cornsilk", QColor(255, 248, 220)},
        {"Crimson", QColor(220, 20, 60)},
        {"Cyan", QColor(0, 255, 255)},
        {"DarkBlue", QColor(0, 0, 139)},
        {"DarkCyan", QColor(0, 139, 139)},
        {"DarkGoldenRod", QColor(184, 134, 11)},
        {"DarkGray", QColor(169, 169, 169)},
        {"DarkGreen", QColor(0, 100, 0)},
        {"DarkKhaki", QColor(189, 183, 107)},
        {"DarkMagenta", QColor(139, 0, 139)},
        {"DarkOliveGreen", QColor(85, 107, 47)},
        {"DarkOrange", QColor(255, 140, 0)},
        {"DarkOrchid", QColor(153, 50, 204)},
        {"DarkRed", QColor(139, 0, 0)},
        {"DarkSalmon", QColor(233, 150, 122)},
        {"DarkSeaGreen", QColor(143, 188, 143)},
        {"DarkSlateBlue", QColor(72, 61, 139)},
        {"DarkSlateGray", QColor(47, 79, 79)},
        {"DarkTurquoise", QColor(0, 206, 209)},
        {"DarkViolet", QColor(148, 0, 211)},
        {"DeepPink", QColor(255, 20, 147)},
        {"DeepSkyBlue", QColor(0, 191, 255)},
        {"DimGray", QColor(105, 105, 105)},
        {"DodgerBlue", QColor(30, 144, 255)},
        {"FireBrick", QColor(178, 34, 34)},
        {"FloralWhite", QColor(255, 250, 240)},
        {"ForestGreen", QColor(34, 139, 34)},
        {"Fuchsia", QColor(255, 0, 255)},
        {"Gainsboro", QColor(220, 220, 220)},
        {"GhostWhite", QColor(248, 248, 255)},
        {"Gold", QColor(255, 215, 0)},
        {"GoldenRod", QColor(218, 165, 32)},
        {"Gray", QColor(128, 128, 128)},
        {"Green", QColor(0, 128, 0)},
        {"GreenYellow", QColor(173, 255, 47)},
        {"HoneyDew", QColor(240, 255, 240)},
        {"HotPink", QColor(255, 105, 180)},
        {"IndianRed", QColor(205, 92, 92)},
        {"Indigo", QColor(75, 0, 130)},
        {"Ivory", QColor(255, 255, 240)},
        {"Khaki", QColor(240, 230, 140)},
        {"Lavender", QColor(230, 230, 250)},
        {"LavenderBlush", QColor(255, 240, 245)},
        {"LawnGreen", QColor(124, 252, 0)},
        {"LemonChiffon", QColor(255, 250, 205)},
        {"LightBlue", QColor(173, 216, 230)},
        {"LightCoral", QColor(240, 128, 128)},
        {"LightCyan", QColor(224, 255, 255)},
        {"LightGoldenRodYellow", QColor(250, 250, 210)},
        {"LightGray", QColor(211, 211, 211)},
        {"LightGreen", QColor(144, 238, 144)},
        {"LightPink", QColor(255, 182, 193)},
        {"LightSalmon", QColor(255, 160, 122)},
        {"LightSeaGreen", QColor(32, 178, 170)},
        {"LightSkyBlue", QColor(135, 206, 250)},
        {"LightSlateGray", QColor(119, 136, 153)},
        {"LightSteelBlue", QColor(176, 196, 222)},
        {"LightYellow", QColor(255, 255, 224)},
        {"Lime", QColor(0, 255, 0)},
        {"LimeGreen", QColor(50, 205, 50)},
        {"Linen", QColor(250, 240, 230)},
        {"Magenta", QColor(255, 0, 255)},
        {"Maroon", QColor(128, 0, 0)},
        {"MediumAquaMarine", QColor(102, 205, 170)},
        {"MediumBlue", QColor(0, 0, 205)},
        {"MediumOrchid", QColor(186, 85, 211)},
        {"MediumPurple", QColor(147, 112, 219)},
        {"MediumSeaGreen", QColor(60, 179, 113)},
        {"MediumSlateBlue", QColor(123, 104, 238)},
        {"MediumSpringGreen", QColor(0, 250, 154)},
        {"MediumTurquoise", QColor(72, 209, 204)},
        {"MediumVioletRed", QColor(199, 21, 133)},
        {"MidnightBlue", QColor(25, 25, 112)},
        {"MintCream", QColor(245, 255, 250)},
        {"MistyRose", QColor(255, 228, 225)},
        {"Moccasin", QColor(255, 228, 181)},
        {"NavajoWhite", QColor(255, 222, 173)},
        {"Navy", QColor(0, 0, 128)},
        {"OldLace", QColor(253, 245, 230)},
        {"Olive", QColor(128, 128, 0)},
        {"OliveDrab", QColor(107, 142, 35)},
        {"Orange", QColor(255, 165, 0)},
        {"OrangeRed", QColor(255, 69, 0)},
        {"Orchid", QColor(218, 112, 214)},
        {"PaleGoldenRod", QColor(238, 232, 170)},
        {"PaleGreen", QColor(152, 251, 152)},
        {"PaleTurquoise", QColor(175, 238, 238)},
        {"PaleVioletRed", QColor(219, 112, 147)},
        {"PapayaWhip", QColor(255, 239, 213)},
        {"PeachPuff", QColor(255, 218, 185)},
        {"Peru", QColor(205, 133, 63)},
        {"Pink", QColor(255, 192, 203)},
        {"Plum", QColor(221, 160, 221)},
        {"PowderBlue", QColor(176, 224, 230)},
        {"Purple", QColor(128, 0, 128)},
        {"Red", QColor(255, 0, 0)},
        {"RosyBrown", QColor(188, 143, 143)},
        {"RoyalBlue", QColor(65, 105, 225)},
        {"SaddleBrown", QColor(139, 69, 19)},
        {"Salmon", QColor(250, 128, 114)},
        {"SandyBrown", QColor(244, 164, 96)},
        {"SeaGreen", QColor(46, 139, 87)},
        {"SeaShell", QColor(255, 245, 238)},
        {"Sienna", QColor(160, 82, 45)},
        {"Silver", QColor(192, 192, 192)},
        {"SkyBlue", QColor(135, 206, 235)},
        {"SlateBlue", QColor(106, 90, 205)},
        {"SlateGray", QColor(112, 128, 144)},
        {"Snow", QColor(255, 250, 250)},
        {"SpringGreen", QColor(0, 255, 127)},
        {"SteelBlue", QColor(70, 130, 180)},
        {"Tan", QColor(210, 180, 140)},
        {"Teal", QColor(0, 128, 128)},
        {"Thistle", QColor(216, 191, 216)},
        {"Tomato", QColor(255, 99, 71)},
        {"Turquoise", QColor(64, 224, 208)},
        {"Violet", QColor(238, 130, 238)},
        {"Wheat", QColor(245, 222, 179)},
        {"White", QColor(255, 255, 255)},
        {"WhiteSmoke", QColor(245, 245, 245)},
        {"Yellow", QColor(255, 255, 0)},
        {"YellowGreen", QColor(154, 205, 50)}
    };
    
    // 按HSL色彩空间排序，创建类似Inkscape的渐变效果
    std::sort(colors.begin(), colors.end(), [](const NamedColor &a, const NamedColor &b) {
        // 将颜色转换为HSL
        QColor colorA = a.color.toHsl();
        QColor colorB = b.color.toHsl();
        
        // 获取HSL值
        int hA = colorA.hslHue();
        int sA = colorA.hslSaturation();
        int lA = colorA.lightness();
        int hB = colorB.hslHue();
        int sB = colorB.hslSaturation();
        int lB = colorB.lightness();
        
        // 处理色相值为-1的情况（黑白灰的色相值为-1）
        // 将黑白灰放在最前面
        if (hA < 0 && hB < 0) {
            // 对于黑白灰，按亮度从深到浅排序
            return lA < lB;
        }
        if (hA < 0) return true;  // 黑白灰排在前面
        if (hB < 0) return false; // 彩色排在后面
        
        // 优先按色相排序，创建光谱渐变效果
        if (hA != hB) {
            return hA < hB;
        }
        
        // 在相同色相内，按亮度从深到浅排序，创建渐变效果
        // 这是关键：相同色相的颜色按亮度排序，形成渐变
        if (lA != lB) {
            return lA < lB; // 深色在前，浅色在后
        }
        
        // 在相同亮度内，按饱和度排序
        return sA < sB;
    });
    
    return colors;
}

void ColorPalette::updateColorButton(QPushButton *button, const QColor &color)
{
    if (color == Qt::transparent) {
        // 对于透明色，使用CSS背景图案实现平铺
        button->setStyleSheet(
            "QPushButton {"
            "  background-color: white;"
            "  background-image: url(:/icons/icons/no-fill-pattern.svg);"
            "  background-repeat: repeat-xy;"
            "  border: 1px solid #888;"
            "}"
        );
        button->setIcon(QIcon()); // 清除图标，使用CSS背景
    } else {
        button->setStyleSheet(QString("background-color: %1; border: 1px solid palette(text);").arg(color.name()));
        button->setIcon(QIcon()); // 清除图标
    }
}

void ColorPalette::selectFillColor(const QColor &color)
{
    m_currentFillColor = color;
    updateColorButton(m_fillColorButton, color);
    emit fillColorChanged(color);
    emit applyColorToSelection(color, true); // 应用填充色到选中对象
}

void ColorPalette::selectStrokeColor(const QColor &color)
{
    m_currentStrokeColor = color;
    updateColorButton(m_strokeColorButton, color);
    emit strokeColorChanged(color);
    emit applyColorToSelection(color, false); // 应用边框色到选中对象
}

void ColorPalette::onFillColorButtonClicked()
{
    QColor color = QColorDialog::getColor(m_currentFillColor, this, "选择填充颜色");
    if (color.isValid()) {
        selectFillColor(color);
    }
}

void ColorPalette::onStrokeColorButtonClicked()
{
    QColor color = QColorDialog::getColor(m_currentStrokeColor, this, "选择边框颜色");
    if (color.isValid()) {
        selectStrokeColor(color);
    }
}



