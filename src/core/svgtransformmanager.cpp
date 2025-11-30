#include "svgtransformmanager.h"
#include "svgstreamhandler.h"
#include "drawing-shape.h"

#include <QStringList>
#include <QRegularExpression>
#include <QDebug>

// 字符串分割函数实现
QStringList splitOnWhitespaceOrComma(const QString &str)
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

// SVG到Scene变换计算
QTransform SvgTransformManager::calculateSvgToSceneTransform(const SvgMetadata &metadata)
{
    QTransform transform;
    
    if (!metadata.hasViewBox) {
        return transform;
    }
    
    QSizeF svgSize = metadata.size;
    QRectF viewBox = metadata.viewBox;
    
    // 如果SVG有明确的尺寸，使用尺寸计算变换
    if (metadata.hasSize && !svgSize.isEmpty()) {
        // 计算缩放比例
        qreal scaleX = svgSize.width() / viewBox.width();
        qreal scaleY = svgSize.height() / viewBox.height();
        
        // 处理preserveAspectRatio
        QString aspectRatio = metadata.preserveAspectRatio;
        QStringList parts = aspectRatio.split(' ', Qt::SkipEmptyParts);
        
        QString align = "xMidYMid";  // 默认对齐方式
        QString meetOrSlice = "meet"; // 默认meet/slice
        
        if (parts.size() >= 1) {
            align = parts[0];
        }
        if (parts.size() >= 2) {
            meetOrSlice = parts[1];
        }
        
        qreal scale = 1.0;
        if (meetOrSlice == "meet") {
            // 保持宽高比，缩放到完全可见
            scale = qMin(scaleX, scaleY);
        } else if (meetOrSlice == "slice") {
            // 保持宽高比，缩放到填满
            scale = qMax(scaleX, scaleY);
        } else if (meetOrSlice == "none") {
            // 不保持宽高比，拉伸填满
            scaleX = scale;
            scaleY = scale;
            scale = 1.0;
        }
        
        // 计算平移以实现对齐
        qreal scaledViewWidth = viewBox.width() * scale;
        qreal scaledViewHeight = viewBox.height() * scale;
        qreal translateX = 0;
        qreal translateY = 0;
        
        // X对齐
        if (align.startsWith("xMin")) {
            translateX = 0;
        } else if (align.startsWith("xMid")) {
            translateX = (svgSize.width() - scaledViewWidth) / 2.0;
        } else if (align.startsWith("xMax")) {
            translateX = svgSize.width() - scaledViewWidth;
        }
        
        // Y对齐
        if (align.contains("YMin")) {
            translateY = 0;
        } else if (align.contains("YMid")) {
            translateY = (svgSize.height() - scaledViewHeight) / 2.0;
        } else if (align.contains("YMax")) {
            translateY = svgSize.height() - scaledViewHeight;
        }
        
        // 构建变换矩阵：先平移到原点，再缩放，再平移到目标位置
        transform.translate(translateX, translateY);
        if (meetOrSlice == "none") {
            transform.scale(scaleX, scaleY);
        } else {
            transform.scale(scale, scale);
        }
        transform.translate(-viewBox.left(), -viewBox.top());
    } else {
        // 如果没有明确尺寸，直接使用viewBox
        transform.translate(-viewBox.left(), -viewBox.top());
    }
    
    return transform;
}

// 分割变换字符串为函数列表
QList<QPair<QString, QStringList>> SvgTransformManager::splitTransformFunctions(const QString &transformStr)
{
    QList<QPair<QString, QStringList>> functions;
    
    if (transformStr.isEmpty()) {
        return functions;
    }
    
    const QString str = transformStr.trimmed();
    int i = 0;
    const int len = str.length();
    
    while (i < len)
    {
        // 跳过空白字符
        while (i < len && str[i].isSpace())
            i++;
        
        if (i >= len)
            break;
        
        // 解析函数名
        QString func;
        while (i < len && (str[i].isLetterOrNumber() || str[i] == '-'))
        {
            func += str[i];
            i++;
        }
        
        // 跳过空白字符
        while (i < len && str[i].isSpace())
            i++;
        
        // 期望遇到 '('
        if (i < len && str[i] == '(')
            i++;
        
        // 跳过空白字符
        while (i < len && str[i].isSpace())
            i++;
        
        // 解析参数字符串
        QString paramsStr;
        int parenLevel = 0;
        while (i < len)
        {
            if (str[i] == '(')
                parenLevel++;
            else if (str[i] == ')')
            {
                if (parenLevel == 0)
                    break;
                parenLevel--;
            }
            paramsStr += str[i];
            i++;
        }
        
        // 跳过 ')'
        if (i < len && str[i] == ')')
            i++;
        
        // 分割参数
        QStringList params = ::splitOnWhitespaceOrComma(paramsStr);
        functions.append(qMakePair(func, params));
        
        // 跳过空白字符
        while (i < len && str[i].isSpace())
            i++;
    }
    
    return functions;
}

// 解析单个变换函数
QTransform SvgTransformManager::parseTransformFunction(const QString &funcName, const QStringList &params)
{
    QTransform transform;
    
    if (funcName == "translate" && params.size() >= 1)
    {
        qreal tx = params[0].toDouble();
        qreal ty = params.size() > 1 ? params[1].toDouble() : 0.0;
        transform.translate(tx, ty);
    }
    else if (funcName == "rotate" && params.size() >= 1)
    {
        qreal angle = params[0].toDouble();
        if (params.size() >= 3)
        {
            qreal cx = params[1].toDouble();
            qreal cy = params[2].toDouble();
            transform.translate(cx, cy);
            transform.rotate(angle);
            transform.translate(-cx, -cy);
        }
        else
        {
            transform.rotate(angle);
        }
    }
    else if (funcName == "scale" && params.size() >= 1)
    {
        qreal sx = params[0].toDouble();
        qreal sy = params.size() > 1 ? params[1].toDouble() : sx;
        transform.scale(sx, sy);
    }
    else if (funcName == "skewX" && params.size() >= 1)
    {
        qreal angle = params[0].toDouble();
        transform.shear(qTan(angle * M_PI / 180.0), 0.0);
    }
    else if (funcName == "skewY" && params.size() >= 1)
    {
        qreal angle = params[0].toDouble();
        transform.shear(0.0, qTan(angle * M_PI / 180.0));
    }
    else if (funcName == "matrix" && params.size() >= 6)
    {
        qreal a = params[0].toDouble();
        qreal b = params[1].toDouble();
        qreal c = params[2].toDouble();
        qreal d = params[3].toDouble();
        qreal e = params[4].toDouble();
        qreal f = params[5].toDouble();
        QTransform matrix(a, b, c, d, e, f);
        transform *= matrix;
    }
    
    return transform;
}

// 解析变换字符串
QTransform SvgTransformManager::parseTransform(const QString &transformStr)
{
    if (transformStr.isEmpty()) {
        return QTransform();
    }
    
    // 分割变换函数
    QList<QPair<QString, QStringList>> functions = splitTransformFunctions(transformStr);
    
    // 按SVG顺序正向应用变换
    QTransform transform;
    for (const auto &functionPair : functions)
    {
        QString funcName = functionPair.first;
        QStringList params = functionPair.second;
        
        QTransform funcTransform = parseTransformFunction(funcName, params);
        transform *= funcTransform;
    }
    
    return transform;
}

// 检查变换字符串是否有效
bool SvgTransformManager::isValidTransform(const QString &transformStr)
{
    if (transformStr.isEmpty()) {
        return false;
    }
    
    QTransform transform = parseTransform(transformStr);
    return !transform.isIdentity();
}

// 应用变换到图形对象
void SvgTransformManager::applyTransformToShape(DrawingShape *shape, const QString &transformStr)
{
    if (!shape || transformStr.isEmpty()) {
        return;
    }
    
    QTransform transform = parseTransform(transformStr);
    
    if (!transform.isIdentity())
    {
        shape->applyTransform(transform);
    }
}

// 获取变换的调试信息
QString SvgTransformManager::getTransformDebugInfo(const QTransform &transform)
{
    if (transform.isIdentity()) {
        return "Identity Transform";
    }
    
    QString info;
    info += QString("Matrix: [%1, %2, %3, %4, %5, %6]")
             .arg(transform.m11())
             .arg(transform.m12())
             .arg(transform.m13())
             .arg(transform.m21())
             .arg(transform.m22())
             .arg(transform.m23());
    
    // 提取变换参数
    qreal det = transform.determinant();
    info += QString("\nDeterminant: %1").arg(det);
    
    // 检查是否包含旋转
    if (transform.m12() != 0 || transform.m21() != 0) {
        info += "\nContains rotation or skew";
    }
    
    // 检查缩放
    if (transform.m11() != 1 || transform.m22() != 1) {
        info += QString("\nScale: X=%1, Y=%2").arg(transform.m11()).arg(transform.m22());
    }
    
    // 检查平移
    if (transform.m31() != 0 || transform.m32() != 0) {
        info += QString("\nTranslation: X=%1, Y=%2").arg(transform.m31()).arg(transform.m32());
    }
    
    return info;
}