#ifndef SVGLENGTHPARSER_H
#define SVGLENGTHPARSER_H

#include <QString>

/**
 * SVG长度解析器 - 解析SVG长度值（px, pt, cm, mm, in, %等）
 * 支持绝对单位和相对单位
 */
class SvgLengthParser
{
public:
    /**
     * 解析SVG长度字符串
     * @param lengthStr 长度字符串
     * @return 解析后的长度值（以像素为单位）
     */
    static qreal parseLength(const QString &lengthStr);
    
    /**
     * 解析SVG长度字符串，提供上下文用于百分比计算
     * @param lengthStr 长度字符串
     * @param referenceValue 参考值（用于百分比计算）
     * @return 解析后的长度值
     */
    static qreal parseLength(const QString &lengthStr, qreal referenceValue);
    
    /**
     * 检查长度字符串是否有效
     * @param lengthStr 长度字符串
     * @return 是否有效
     */
    static bool isValidLength(const QString &lengthStr);

private:
    /**
     * 单位类型枚举
     */
    enum LengthUnit {
        Px,     // 像素
        Pt,     // 点
        Pc,     // 派卡
        In,     // 英寸
        Cm,     // 厘米
        Mm,     // 毫米
        Percent, // 百分比
        None    // 无单位
    };
    
    /**
     * 解析单位类型
     * @param str 单位字符串
     * @return 单位类型
     */
    static LengthUnit parseUnit(const QString &str);
    
    /**
     * 获取单位到像素的转换系数
     * @param unit 单位类型
     * @return 转换系数
     */
    static qreal getUnitToPixelFactor(LengthUnit unit);
};

#endif // SVGLENGTHPARSER_H