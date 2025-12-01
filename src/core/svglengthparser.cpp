#include "svglengthparser.h"

#include <QString>

qreal SvgLengthParser::parseLength(const QString &lengthStr)
{
    return parseLength(lengthStr, 0.0); // 默认参考值为0
}

qreal SvgLengthParser::parseLength(const QString &lengthStr, qreal referenceValue)
{
    if (lengthStr.isEmpty())
        return 0.0;

    const QString str = lengthStr.trimmed();
    
    // 查找单位部分（从后往前查找第一个非数字字符）
    int unitStart = str.length() - 1;
    while (unitStart >= 0 && (str[unitStart].isLetter() || str[unitStart] == '%'))
    {
        unitStart--;
    }
    unitStart++; // 回到第一个单位字符的位置
    
    QString numberStr = str.left(unitStart);
    QString unitStr = str.mid(unitStart).toLower();
    
    bool ok;
    qreal value = numberStr.toDouble(&ok);
    if (!ok)
        return 0.0;
    
    // 解析单位
    LengthUnit unit = parseUnit(unitStr);
    
    // 单位转换（假设96 DPI）
    qreal factor = getUnitToPixelFactor(unit);
    
    if (unit == Percent && referenceValue > 0)
    {
        return value * referenceValue / 100.0;
    }
    
    return value * factor;
}

bool SvgLengthParser::isValidLength(const QString &lengthStr)
{
    if (lengthStr.isEmpty())
        return false;
        
    bool ok;
    QString str = lengthStr.trimmed();
    
    // 查找单位部分
    int unitStart = str.length() - 1;
    while (unitStart >= 0 && (str[unitStart].isLetter() || str[unitStart] == '%'))
    {
        unitStart--;
    }
    unitStart++;
    
    QString numberStr = str.left(unitStart);
    numberStr.toDouble(&ok);
    
    return ok;
}

SvgLengthParser::LengthUnit SvgLengthParser::parseUnit(const QString &str)
{
    if (str.isEmpty() || str == "px")
        return Px;
    else if (str == "pt")
        return Pt;
    else if (str == "pc")
        return Pc;
    else if (str == "in")
        return In;
    else if (str == "cm")
        return Cm;
    else if (str == "mm")
        return Mm;
    else if (str == "%")
        return Percent;
    else
        return None;
}

qreal SvgLengthParser::getUnitToPixelFactor(LengthUnit unit)
{
    // 单位转换（假设96 DPI）
    switch (unit)
    {
        case Px:
            return 1.0;
        case Pt:
            return 1.25;      // 1 pt = 1.25 px (96/72)
        case Pc:
            return 15.0;      // 1 pc = 12 pt = 15 px
        case In:
            return 96.0;      // 1 in = 96 px
        case Cm:
            return 37.7953;   // 1 in = 2.54 cm, so 1 cm = 96/2.54 px
        case Mm:
            return 3.77953;   // 1 mm = 0.1 cm, so 1 mm = 96/25.4 px
        case Percent:
            return 1.0;       // 百分比需要上下文，这里直接返回值
        case None:
        default:
            return 1.0;
    }
}