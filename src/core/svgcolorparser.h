#ifndef SVGCOLORPARSER_H
#define SVGCOLORPARSER_H

#include <QColor>
#include <QString>

/**
 * SVG颜色解析器 - 手写解析器，避免正则表达式性能问题
 * 支持十六进制、RGB/RGBA、HSL/HSLA、命名颜色等格式
 */
class SvgColorParser
{
public:
    /**
     * 解析SVG颜色字符串
     * @param colorStr 颜色字符串
     * @return 解析后的颜色，无效时返回无效颜色
     */
    static QColor parseColor(const QString &colorStr);
    
    /**
     * 检查颜色字符串是否有效
     * @param colorStr 颜色字符串
     * @return 是否有效
     */
    static bool isValidColor(const QString &colorStr);

private:
    /**
     * 解析十六进制颜色
     * @param hexStr 十六进制字符串
     * @return 解析后的颜色
     */
    static QColor parseHexColor(const QString &hexStr);
    
    /**
     * 解析RGB/RGBA颜色
     * @param content 内容字符串
     * @param hasAlpha 是否包含透明度
     * @return 解析后的颜色
     */
    static QColor parseRgbColor(const QString &content, bool hasAlpha);
    
    /**
     * 解析HSL/HSLA颜色
     * @param content 内容字符串
     * @param hasAlpha 是否包含透明度
     * @return 解析后的颜色
     */
    static QColor parseHslColor(const QString &content, bool hasAlpha);
    
    /**
     * 字符串分割辅助函数
     * @param str 输入字符串
     * @return 分割后的字符串列表
     */
    static QStringList splitOnWhitespaceOrComma(const QString &str);
};

#endif // SVGCOLORPARSER_H