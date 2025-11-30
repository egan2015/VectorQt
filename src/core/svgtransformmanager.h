#ifndef SVGTRANSFORMMANAGER_H
#define SVGTRANSFORMMANAGER_H

#include <QTransform>
#include <QString>
#include <QPointF>

// 前向声明
struct SvgMetadata;
class DrawingShape;

// 字符串分割辅助函数
QStringList splitOnWhitespaceOrComma(const QString &str);

/**
 * SVG变换管理器 - 统一处理所有SVG变换逻辑
 * 包括SVG到Scene的全局变换、元素变换等
 */
class SvgTransformManager
{
public:
    /**
     * 计算SVG到Scene的变换矩阵
     * 处理viewBox、preserveAspectRatio等
     * @param metadata SVG元数据
     * @return 变换矩阵
     */
    static QTransform calculateSvgToSceneTransform(const SvgMetadata &metadata);
    
    /**
     * 解析SVG变换字符串
     * 支持translate、rotate、scale、skewX、skewY、matrix等
     * @param transformStr SVG变换字符串
     * @return 变换矩阵
     */
    static QTransform parseTransform(const QString &transformStr);
    
    /**
     * 应用变换到图形对象
     * 统一的变换应用入口
     * @param shape 图形对象
     * @param transformStr 变换字符串
     */
    static void applyTransformToShape(DrawingShape *shape, const QString &transformStr);
    
    /**
     * 检查变换字符串是否有效
     * @param transformStr 变换字符串
     * @return 是否有效
     */
    static bool isValidTransform(const QString &transformStr);
    
    /**
     * 获取变换的调试信息
     * @param transform 变换矩阵
     * @return 调试字符串
     */
    static QString getTransformDebugInfo(const QTransform &transform);

private:
    /**
     * 解析单个变换函数
     * @param funcName 函数名
     * @param params 参数列表
     * @return 变换矩阵
     */
    static QTransform parseTransformFunction(const QString &funcName, const QStringList &params);
    
    /**
     * 分割变换字符串为函数列表
     * @param transformStr 变换字符串
     * @return 函数列表
     */
    static QList<QPair<QString, QStringList>> splitTransformFunctions(const QString &transformStr);
    
    
};

#endif // SVGTRANSFORMMANAGER_H