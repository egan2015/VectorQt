#include "svgcolorparser.h"

#include <QStringList>
#include <QHash>

// 字符串分割辅助函数实现
QStringList SvgColorParser::splitOnWhitespaceOrComma(const QString &str)
{
    QStringList result;
    if (str.isEmpty())
        return result;

    QString current;
    for (int i = 0; i < str.length(); ++i)
    {
        QChar c = str[i];
        if (c.isSpace() || c == ',')
        {
            if (!current.isEmpty())
            {
                result.append(current);
                current.clear();
            }
        }
        else
        {
            current += c;
        }
    }
    
    if (!current.isEmpty())
        result.append(current);
        
    return result;
}

QColor SvgColorParser::parseColor(const QString &colorStr)
{
    if (colorStr.isEmpty())
        return QColor();

    const QString str = colorStr.trimmed().toLower();
    
    // 处理none关键字
    if (str == "none")
        return QColor();
    
    // 处理currentColor关键字
    if (str == "currentcolor")
        return QColor(); // 需要从上下文获取当前颜色
    
    // 处理十六进制颜色
    if (str.startsWith('#'))
    {
        return parseHexColor(str);
    }
    
    // 处理RGB/RGBA颜色
    if (str.startsWith("rgb("))
    {
        QString content = str.mid(4, str.length() - 5).trimmed();
        return parseRgbColor(content, false);
    }
    else if (str.startsWith("rgba("))
    {
        QString content = str.mid(5, str.length() - 6).trimmed();
        return parseRgbColor(content, true);
    }
    
    // 处理HSL/HSLA颜色
    else if (str.startsWith("hsl("))
    {
        QString content = str.mid(4, str.length() - 5).trimmed();
        return parseHslColor(content, false);
    }
    else if (str.startsWith("hsla("))
    {
        QString content = str.mid(5, str.length() - 6).trimmed();
        return parseHslColor(content, true);
    }
    
    // 处理命名颜色
    static const QHash<QString, QColor> namedColors = {
        {"aliceblue", QColor(240, 248, 255)},
        {"antiquewhite", QColor(250, 235, 215)},
        {"aqua", QColor(0, 255, 255)},
        {"aquamarine", QColor(127, 255, 212)},
        {"azure", QColor(240, 255, 255)},
        {"beige", QColor(245, 245, 220)},
        {"bisque", QColor(255, 228, 196)},
        {"black", QColor(0, 0, 0)},
        {"blanchedalmond", QColor(255, 235, 205)},
        {"blue", QColor(0, 0, 255)},
        {"blueviolet", QColor(138, 43, 226)},
        {"brown", QColor(165, 42, 42)},
        {"burlywood", QColor(222, 184, 135)},
        {"cadetblue", QColor(95, 158, 160)},
        {"chartreuse", QColor(127, 255, 0)},
        {"chocolate", QColor(210, 105, 30)},
        {"coral", QColor(255, 127, 80)},
        {"cornflowerblue", QColor(100, 149, 237)},
        {"cornsilk", QColor(255, 248, 220)},
        {"crimson", QColor(220, 20, 60)},
        {"cyan", QColor(0, 255, 255)},
        {"darkblue", QColor(0, 0, 139)},
        {"darkcyan", QColor(0, 139, 139)},
        {"darkgoldenrod", QColor(184, 134, 11)},
        {"darkgray", QColor(169, 169, 169)},
        {"darkgreen", QColor(0, 100, 0)},
        {"darkgrey", QColor(169, 169, 169)},
        {"darkkhaki", QColor(189, 183, 107)},
        {"darkmagenta", QColor(139, 0, 139)},
        {"darkolivegreen", QColor(85, 107, 47)},
        {"darkorange", QColor(255, 140, 0)},
        {"darkorchid", QColor(153, 50, 204)},
        {"darkred", QColor(139, 0, 0)},
        {"darksalmon", QColor(233, 150, 122)},
        {"darkseagreen", QColor(143, 188, 143)},
        {"darkslateblue", QColor(72, 61, 139)},
        {"darkslategray", QColor(47, 79, 79)},
        {"darkslategrey", QColor(47, 79, 79)},
        {"darkturquoise", QColor(0, 206, 209)},
        {"darkviolet", QColor(148, 0, 211)},
        {"deeppink", QColor(255, 20, 147)},
        {"deepskyblue", QColor(0, 191, 255)},
        {"dimgray", QColor(105, 105, 105)},
        {"dimgrey", QColor(105, 105, 105)},
        {"dodgerblue", QColor(30, 144, 255)},
        {"firebrick", QColor(178, 34, 34)},
        {"floralwhite", QColor(255, 250, 240)},
        {"forestgreen", QColor(34, 139, 34)},
        {"fuchsia", QColor(255, 0, 255)},
        {"gainsboro", QColor(220, 220, 220)},
        {"ghostwhite", QColor(248, 248, 255)},
        {"gold", QColor(255, 215, 0)},
        {"goldenrod", QColor(218, 165, 32)},
        {"gray", QColor(128, 128, 128)},
        {"green", QColor(0, 128, 0)},
        {"greenyellow", QColor(173, 255, 47)},
        {"grey", QColor(128, 128, 128)},
        {"honeydew", QColor(240, 255, 240)},
        {"hotpink", QColor(255, 105, 180)},
        {"indianred", QColor(205, 92, 92)},
        {"indigo", QColor(75, 0, 130)},
        {"ivory", QColor(255, 255, 240)},
        {"khaki", QColor(240, 230, 140)},
        {"lavender", QColor(230, 230, 250)},
        {"lavenderblush", QColor(255, 240, 245)},
        {"lawngreen", QColor(124, 252, 0)},
        {"lemonchiffon", QColor(255, 250, 205)},
        {"lightblue", QColor(173, 216, 230)},
        {"lightcoral", QColor(240, 128, 128)},
        {"lightcyan", QColor(224, 255, 255)},
        {"lightgoldenrodyellow", QColor(250, 250, 210)},
        {"lightgray", QColor(211, 211, 211)},
        {"lightgreen", QColor(144, 238, 144)},
        {"lightgrey", QColor(211, 211, 211)},
        {"lightpink", QColor(255, 182, 193)},
        {"lightsalmon", QColor(255, 160, 122)},
        {"lightseagreen", QColor(32, 178, 170)},
        {"lightskyblue", QColor(135, 206, 250)},
        {"lightslategray", QColor(119, 136, 153)},
        {"lightslategrey", QColor(119, 136, 153)},
        {"lightsteelblue", QColor(176, 196, 222)},
        {"lightyellow", QColor(255, 255, 224)},
        {"lime", QColor(0, 255, 0)},
        {"limegreen", QColor(50, 205, 50)},
        {"linen", QColor(250, 240, 230)},
        {"magenta", QColor(255, 0, 255)},
        {"maroon", QColor(128, 0, 0)},
        {"mediumaquamarine", QColor(102, 205, 170)},
        {"mediumblue", QColor(0, 0, 205)},
        {"mediumorchid", QColor(186, 85, 211)},
        {"mediumpurple", QColor(147, 112, 219)},
        {"mediumseagreen", QColor(60, 179, 113)},
        {"mediumslateblue", QColor(123, 104, 238)},
        {"mediumspringgreen", QColor(0, 250, 154)},
        {"mediumturquoise", QColor(72, 209, 204)},
        {"mediumvioletred", QColor(199, 21, 133)},
        {"midnightblue", QColor(25, 25, 112)},
        {"mintcream", QColor(245, 255, 250)},
        {"mistyrose", QColor(255, 228, 225)},
        {"moccasin", QColor(255, 228, 181)},
        {"navajowhite", QColor(255, 222, 173)},
        {"navy", QColor(0, 0, 128)},
        {"oldlace", QColor(253, 245, 230)},
        {"olive", QColor(128, 128, 0)},
        {"olivedrab", QColor(107, 142, 35)},
        {"orange", QColor(255, 165, 0)},
        {"orangered", QColor(255, 69, 0)},
        {"orchid", QColor(218, 112, 214)},
        {"palegoldenrod", QColor(238, 232, 170)},
        {"palegreen", QColor(152, 251, 152)},
        {"paleturquoise", QColor(175, 238, 238)},
        {"palevioletred", QColor(219, 112, 147)},
        {"papayawhip", QColor(255, 239, 213)},
        {"peachpuff", QColor(255, 218, 185)},
        {"peru", QColor(205, 133, 63)},
        {"pink", QColor(255, 192, 203)},
        {"plum", QColor(221, 160, 221)},
        {"powderblue", QColor(176, 224, 230)},
        {"purple", QColor(128, 0, 128)},
        {"red", QColor(255, 0, 0)},
        {"rosybrown", QColor(188, 143, 143)},
        {"royalblue", QColor(65, 105, 225)},
        {"saddlebrown", QColor(139, 69, 19)},
        {"salmon", QColor(250, 128, 114)},
        {"sandybrown", QColor(244, 164, 96)},
        {"seagreen", QColor(46, 139, 87)},
        {"seashell", QColor(255, 245, 238)},
        {"sienna", QColor(160, 82, 45)},
        {"silver", QColor(192, 192, 192)},
        {"skyblue", QColor(135, 206, 235)},
        {"slateblue", QColor(106, 90, 205)},
        {"slategray", QColor(112, 128, 144)},
        {"slategrey", QColor(112, 128, 144)},
        {"snow", QColor(255, 250, 250)},
        {"springgreen", QColor(0, 255, 127)},
        {"steelblue", QColor(70, 130, 180)},
        {"tan", QColor(210, 180, 140)},
        {"teal", QColor(0, 128, 128)},
        {"thistle", QColor(216, 191, 216)},
        {"tomato", QColor(255, 99, 71)},
        {"turquoise", QColor(64, 224, 208)},
        {"violet", QColor(238, 130, 238)},
        {"wheat", QColor(245, 222, 179)},
        {"white", QColor(255, 255, 255)},
        {"whitesmoke", QColor(245, 245, 245)},
        {"yellow", QColor(255, 255, 0)},
        {"yellowgreen", QColor(154, 205, 50)}
    };
    
    auto it = namedColors.find(str);
    if (it != namedColors.end())
    {
        return it.value();
    }
    
    return QColor(); // 无法识别的颜色
}

bool SvgColorParser::isValidColor(const QString &colorStr)
{
    QColor color = parseColor(colorStr);
    return color.isValid();
}

QColor SvgColorParser::parseHexColor(const QString &hexStr)
{
    QString hexColor = hexStr.mid(1); // 去掉#号
    
    if (hexColor.length() == 3)
    {
        // 短格式 #RGB 转换为 #RRGGBB
        QString expanded;
        for (int i = 0; i < 3; ++i)
        {
            expanded += QString(hexColor[i]) + QString(hexColor[i]);
        }
        hexColor = expanded;
    }
    else if (hexColor.length() == 4)
    {
        // 短格式 #RGBA 转换为 #RRGGBBAA
        QString expanded;
        for (int i = 0; i < 4; ++i)
        {
            expanded += QString(hexColor[i]) + QString(hexColor[i]);
        }
        hexColor = expanded;
    }
    
    bool ok;
    uint color = hexColor.toUInt(&ok, 16);
    if (ok)
    {
        if (hexColor.length() == 6)
        {
            return QColor::fromRgb(color);
        }
        else if (hexColor.length() == 8)
        {
            return QColor::fromRgba(color);
        }
    }
    
    return QColor();
}

QColor SvgColorParser::parseRgbColor(const QString &content, bool hasAlpha)
{
    QStringList parts = splitOnWhitespaceOrComma(content);
    if (parts.size() >= 3)
    {
        int r = qBound(0, parts[0].toInt(), 255);
        int g = qBound(0, parts[1].toInt(), 255);
        int b = qBound(0, parts[2].toInt(), 255);
        int a = 255;
        if (hasAlpha && parts.size() >= 4)
        {
            a = qBound(0, qRound(parts[3].toDouble() * 255), 255);
        }
        return QColor(r, g, b, a);
    }
    
    return QColor();
}

QColor SvgColorParser::parseHslColor(const QString &content, bool hasAlpha)
{
    QStringList parts = splitOnWhitespaceOrComma(content);
    if (parts.size() >= 3)
    {
        qreal h = qBound(0.0, parts[0].toDouble(), 360.0);
        qreal s = qBound(0.0, parts[1].toDouble(), 100.0) / 100.0;
        qreal l = qBound(0.0, parts[2].toDouble(), 100.0) / 100.0;
        int a = 255;
        if (hasAlpha && parts.size() >= 4)
        {
            a = qBound(0, qRound(parts[3].toDouble() * 255), 255);
        }
        return QColor::fromHslF(h / 360.0, s, l, a / 255.0);
    }
    
    return QColor();
}