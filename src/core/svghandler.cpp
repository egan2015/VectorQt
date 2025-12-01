#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

#include <QPointF>
#include <QTransform>
#include <//QDebug>
#include "svghandler.h"
#include "svgstreamhandler.h"
#include "fastpathparser.h"
#include "svgelementcollector.h"
#include "drawing-shape.h"
#include "drawing-layer.h"
#include "drawing-group.h"
#include "layer-manager.h"
#include "../ui/drawingscene.h"

// 全局存储定义
QHash<QString, QGradient> s_gradients;
QHash<QString, QGraphicsEffect *> s_filters;
QHash<QString, QBrush> s_patterns;
QHash<QString, QDomElement> s_markers;
QHash<QString, MarkerData> s_markerDataCache;
QHash<QString, QDomElement> s_definedElements;

// 手写解析器类定义 - 避免Qt正则表达式的性能问题

// SvgStringUtils类定义 - 提供简单的字符串分割功能，避免正则表达式
class SvgStringUtils
{
public:
    static QStringList splitOnWhitespaceOrComma(const QString &str)
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
    
    static QStringList splitOnWhitespace(const QString &str)
    {
        QStringList result;
        if (str.isEmpty())
            return result;

        QString current;
        for (int i = 0; i < str.length(); ++i)
        {
            QChar c = str[i];
            if (c.isSpace())
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
};

// SvgTransformParser类定义 - 用于解析SVG变换字符串
class SvgTransformParser
{
public:
    QTransform parse(const QString &transformStr)
    {
        if (transformStr.isEmpty())
            return QTransform();

        const QString str = transformStr.trimmed();
        QString result;
        int i = 0;
        const int len = str.length();
        
        // 先收集所有变换
        QList<QPair<QString, QStringList>> transforms;
        
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
            QStringList params = SvgStringUtils::splitOnWhitespaceOrComma(paramsStr);
            transforms.append(qMakePair(func, params));
            
            // 跳过空白字符
            while (i < len && str[i].isSpace())
                i++;
        }
        
        // 按SVG顺序正向应用变换
        QTransform transform;
        for (const auto &transformPair : transforms)
        {
            QString func = transformPair.first;
            QStringList params = transformPair.second;

            if (func == "translate" && params.size() >= 1)
            {
                qreal tx = params[0].toDouble();
                qreal ty = params.size() > 1 ? params[1].toDouble() : 0.0;
                transform.translate(tx, ty);
            }
            else if (func == "rotate" && params.size() >= 1)
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
            else if (func == "scale" && params.size() >= 1)
            {
                qreal sx = params[0].toDouble();
                qreal sy = params.size() > 1 ? params[1].toDouble() : sx;
                transform.scale(sx, sy);
            }
            else if (func == "skewX" && params.size() >= 1)
            {
                qreal angle = params[0].toDouble();
                transform.shear(qTan(angle * M_PI / 180.0), 0.0);
            }
            else if (func == "skewY" && params.size() >= 1)
            {
                qreal angle = params[0].toDouble();
                transform.shear(0.0, qTan(angle * M_PI / 180.0));
            }
            else if (func == "matrix" && params.size() >= 6)
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
        }
        
        return transform;
    }
};





// SvgPointParser类定义 - 用于解析SVG多边形和折线的点坐标
class SvgPointParser
{
public:
    static void parsePoints(const QString &pointsStr, QPainterPath &path)
    {
        if (pointsStr.isEmpty())
            return;

        const QString str = pointsStr.trimmed();
        if (str.isEmpty())
            return;

        int i = 0;
        const int len = str.length();
        
        while (i < len)
        {
            // 跳过空白字符
            while (i < len && str[i].isSpace())
                i++;
            
            if (i >= len)
                break;
            
            // 解析X坐标
            QString xStr;
            while (i < len && (str[i].isDigit() || str[i] == '.' || str[i] == '-' || str[i] == '+'))
            {
                xStr += str[i];
                i++;
            }
            
            // 跳过空白字符
            while (i < len && str[i].isSpace())
                i++;
            
            // 期望遇到逗号
            if (i < len && str[i] == ',')
                i++;
            
            // 跳过空白字符
            while (i < len && str[i].isSpace())
                i++;
            
            // 解析Y坐标
            QString yStr;
            while (i < len && (str[i].isDigit() || str[i] == '.' || str[i] == '-' || str[i] == '+'))
            {
                yStr += str[i];
                i++;
            }
            
            // 转换为数值并添加到路径
            bool okX, okY;
            qreal x = xStr.toDouble(&okX);
            qreal y = yStr.toDouble(&okY);
            
            if (okX && okY)
            {
                if (path.elementCount() == 0)
                {
                    path.moveTo(x, y);
                }
                else
                {
                    path.lineTo(x, y);
                }
            }
            
            // 跳过到下一个点的空白字符
            while (i < len && str[i].isSpace())
                i++;
        }
    }
};

// SvgUrlParser类定义 - 用于解析SVG URL引用，如url(#markerId)
class SvgUrlParser
{
public:
    static QString parseUrlReference(const QString &urlStr)
    {
        if (urlStr.isEmpty())
            return QString();

        const QString str = urlStr.trimmed();
        
        // 检查是否以 "url(#" 开头
        if (!str.startsWith("url(#"))
            return QString();
        
        // 查找结尾的 ")"
        int endPos = str.indexOf(')');
        if (endPos == -1)
            return QString();
        
        // 提取ID部分（去掉 "url(#" 和 ")"）
        QString id = str.mid(5, endPos - 5); // 5 = len("url(#")
        return id.trimmed();
    }
};

// SvgUseTransformParser类定义 - 专门用于解析和调整use元素的变换
class SvgUseTransformParser
{
public:
    static QString adjustTransformForUseElement(const QString &transformStr, qreal x, qreal y)
    {
        if (transformStr.isEmpty())
            return QString();

        const QString str = transformStr.trimmed();
        QString result;
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
            while (i < len && str[i].isLetterOrNumber())
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
            QStringList params = SvgStringUtils::splitOnWhitespaceOrComma(paramsStr);
            
            if (!result.isEmpty())
                result += " ";
            
            // 处理不同的变换类型
            if (func == "rotate" && params.size() >= 1)
            {
                qreal angle = params[0].toDouble();
                qreal cx = 0, cy = 0;
                if (params.size() >= 3)
                {
                    cx = params[1].toDouble();
                    cy = params[2].toDouble();
                    // 调整旋转中心：SVG中的旋转中心是绝对坐标，需要转换为相对坐标
                    cx += x;
                    cy += y;
                }
                result += QString("rotate(%1 %2 %3)").arg(angle).arg(cx).arg(cy);
            }
            else if (func == "scale" && params.size() >= 1)
            {
                qreal sx = params[0].toDouble();
                qreal sy = params.size() > 1 ? params[1].toDouble() : sx;

                // 对于use元素，缩放应该围绕use元素的位置进行
                // 需要调整为：translate(x,y) scale(sx,sy) translate(-x,-y)
                result += QString("translate(%1 %2) scale(%3 %4) translate(%5 %6)")
                        .arg(x).arg(y).arg(sx).arg(sy).arg(-x).arg(-y);
            }
            else
            {
                // 其他变换保持不变
                result += func + "(" + paramsStr + ")";
            }
            
            // 跳过空白字符
            while (i < len && str[i].isSpace())
                i++;
        }
        
        return result;
    }
};



// 辅助函数：计算marker边界框
static QRectF calculateMarkerBounds(const MarkerData &markerData)
{
    if (!markerData.isValid)
    {
        return QRectF(0, 0, 10, 10); // 默认大小
    }

    switch (markerData.type)
    {
    case MarkerData::Circle:
        if (markerData.params.size() >= 3)
        {
            qreal cx = markerData.params[0].toReal();
            qreal cy = markerData.params[1].toReal();
            qreal r = markerData.params[2].toReal();
            return QRectF(cx - r, cy - r, 2 * r, 2 * r);
        }
        break;

    case MarkerData::Rect:
        if (markerData.params.size() >= 4)
        {
            qreal x = markerData.params[0].toReal();
            qreal y = markerData.params[1].toReal();
            qreal width = markerData.params[2].toReal();
            qreal height = markerData.params[3].toReal();
            return QRectF(x, y, width, height);
        }
        break;

    case MarkerData::Path:
        if (markerData.params.size() >= 1)
        {
            QPainterPath path = markerData.params[0].value<QPainterPath>();
            return path.boundingRect();
        }
        break;

    case MarkerData::Polygon:
        if (markerData.params.size() >= 1)
        {
            QPainterPath path = markerData.params[0].value<QPainterPath>();
            return path.boundingRect();
        }
        break;

    default:
        break;
    }

    return QRectF(0, 0, 10, 10); // 默认大小
}

bool SvgHandler::importFromSvg(DrawingScene *scene, const QString &fileName)
{
    // 设置SVG导入标志，防止创建默认图层
    LayerManager::instance()->setSvgImporting(true);
    
    // 优先使用流式解析器处理大文件
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        //qDebug() << "无法打开SVG文件:" << fileName;
        LayerManager::instance()->setSvgImporting(false);
        return false;
    }

    // 暂时禁用流式解析，使用DOM方式
    qint64 fileSize = file.size();
    bool useStreamParser = false; // fileSize > 100 * 1024; // 100KB threshold
    
    if (useStreamParser)
    {
        file.close();
        // 使用SvgStreamHandler进行流式解析
        bool result = SvgStreamHandler::importFromSvg(scene, fileName);
        return result;
    }
    else
    {
        // 小文件继续使用DOM解析
        QDomDocument doc;
        QString errorMsg;
        int errorLine, errorCol;

        if (!doc.setContent(&file, &errorMsg, &errorLine, &errorCol))
        {
            //qDebug() << "解析SVG文件失败:" << errorMsg << "行:" << errorLine << "列:" << errorCol;
            LayerManager::instance()->setSvgImporting(false);
            return false;
        }

        file.close();

        bool result = parseSvgDocument(scene, doc);
        LayerManager::instance()->setSvgImporting(false);
        return result;
    }
}

bool SvgHandler::parseSvgDocument(DrawingScene *scene, const QDomDocument &doc)
{
    QDomElement root = doc.documentElement();
    
    if (root.tagName() != "svg")
    {
        //qDebug() << "不是有效的SVG文档，标签名:" << root.tagName();
        return false;
    }

    // 解析SVG元数据（viewBox、size等）
    SvgMetadata metadata = parseSvgMetadata(root);
    
    // 应用SVG设置到Scene（设置sceneRect等）
    applySvgSettingsToScene(scene, metadata);
    
    // 计算SVG到Scene的变换矩阵
    QTransform svgToSceneTransform = calculateSvgToSceneTransform(metadata);

    // 清空之前存储的定义元素
    s_definedElements.clear();

    // 使用优化的元素收集器，单次遍历收集所有元素
    SvgElementCollector::CollectedElements collected = SvgElementCollector::collect(root);
    
    
    
    // 更新全局定义元素缓存
    s_definedElements = collected.definedElements;

    // 清理之前的渐变定义
    s_gradients.clear();

    // 批量处理渐变定义
    for (const QDomElement &gradient : collected.linearGradients) {
        QString id = gradient.attribute("id");
        if (!id.isEmpty()) {
            QLinearGradient linearGradient = parseLinearGradient(gradient);
            s_gradients[id] = linearGradient;
        }
    }
    for (const QDomElement &gradient : collected.radialGradients) {
        QString id = gradient.attribute("id");
        if (!id.isEmpty()) {
            QRadialGradient radialGradient = parseRadialGradient(gradient);
            s_gradients[id] = radialGradient;
        }
    }

    // 清理之前的滤镜定义
    s_filters.clear();

    // 批量处理滤镜定义
    for (const QDomElement &filter : collected.gaussianBlurFilters) {
        QString id = filter.attribute("filter-id"); // 使用我们设置的filter-id属性
        if (!id.isEmpty()) {
            QGraphicsBlurEffect *blurEffect = parseGaussianBlurFilter(filter);
            if (blurEffect) {
                s_filters[id] = blurEffect;
            }
        }
    }
    for (const QDomElement &filter : collected.dropShadowFilters) {
        QString id = filter.attribute("filter-id"); // 使用我们设置的filter-id属性
        if (!id.isEmpty()) {
            QGraphicsDropShadowEffect *shadowEffect = parseDropShadowFilter(filter);
            if (shadowEffect) {
                s_filters[id] = shadowEffect;
            }
        }
    }

    // 清理之前的图案定义
    s_patterns.clear();

    // 批量处理图案定义
    for (const QDomElement &pattern : collected.patterns) {
        QString id = pattern.attribute("id");
        if (!id.isEmpty()) {
            QBrush patternBrush = parsePatternBrush(pattern);
            s_patterns[id] = patternBrush;
        }
    }

    // 清理之前的标记定义
    s_markers.clear();
    s_markerDataCache.clear();

    // 批量处理标记定义
    for (const QDomElement &marker : collected.markers) {
        QString id = marker.attribute("id");
        if (!id.isEmpty()) {
            s_markers[id] = marker.cloneNode().toElement();
            // 预解析标记数据
            MarkerData markerData = parseMarkerData(marker);
            s_markerDataCache[id] = markerData;
        }
    }

    // 使用收集到的元素创建图形对象
    int elementCount = 0;
    
    // 处理图层元素（包括嵌套的处理）
    for (const QDomElement &element : collected.layers) {
        DrawingGroup *group = parseGroupElement(scene, element);
        if (group) {
            elementCount++;
        }
    }
    
    // 处理普通组元素（非图层），只处理顶层组
    for (const QDomElement &element : collected.groups) {
        // 检查是否是顶层组（没有父组或父组不是g元素）
        QDomNode parent = element.parentNode();
        bool isTopLevel = true;
        while (!parent.isNull() && parent.isElement()) {
            if (parent.toElement().tagName() == "g") {
                isTopLevel = false;
                break;
            }
            parent = parent.parentNode();
        }
        
        if (isTopLevel) {
            DrawingGroup *group = parseGroupElement(scene, element);
            if (group) {
                elementCount++;
            }
        }
    }
    
    // 处理基本图形元素（只处理不在组中的元素）
    auto processShapeElements = [&](const QList<QDomElement> &elements) {
        for (const QDomElement &element : elements) {
            // 检查元素是否在组中
            QDomNode parent = element.parentNode();
            bool isInGroup = false;
            while (!parent.isNull()) {
                if (parent.isElement() && parent.toElement().tagName() == "g") {
                    isInGroup = true;
                    break;
                }
                parent = parent.parentNode();
            }
            
            // 只有不在组中的元素才直接添加到场景
            if (!isInGroup) {
                DrawingShape *shape = parseSvgElement(element);
                if (shape) {
                    scene->addItem(shape);
                    // 设置Z值，确保按照SVG文档顺序显示
                    shape->setZValue(elementCount);
                    elementCount++;
                }
            }
        }
    };
    
    // 处理各种图形元素
    processShapeElements(collected.paths);
    processShapeElements(collected.rectangles);
    processShapeElements(collected.ellipses);
    processShapeElements(collected.circles);
    processShapeElements(collected.lines);
    processShapeElements(collected.polylines);
    processShapeElements(collected.polygons);
    processShapeElements(collected.texts);
    processShapeElements(collected.useElements);

    // SVG导入完成后，只删除现有的背景图层，保持导入图层的原始名称
    LayerManager *layerManager = LayerManager::instance();
    if (layerManager->layerCount() > 0)
    {
        // 删除现有的背景图层
        QList<DrawingLayer *> layersToDelete;
        for (DrawingLayer *layer : layerManager->layers())
        {
            if (layer->name() == "背景图层")
            {
                layersToDelete.append(layer);
            }
        }

        for (DrawingLayer *layer : layersToDelete)
        {
            layerManager->deleteLayer(layer);
        }

        // 设置第一个导入图层为活动图层
        if (layerManager->layerCount() > 0)
        {
            DrawingLayer *firstLayer = layerManager->layer(0);
            layerManager->setActiveLayer(firstLayer);
        }
    }

    // 应用SVG到Scene的变换到所有元素
    if (!svgToSceneTransform.isIdentity()) {
        // 对场景中的所有顶级元素应用变换
        QList<QGraphicsItem*> items = scene->items();
        for (QGraphicsItem *item : items) {
            if (DrawingShape *shape = dynamic_cast<DrawingShape*>(item)) {
                // 只对不在图层中的元素应用变换
                if (!shape->parentItem()) {
                    shape->setTransform(svgToSceneTransform * shape->transform());
                }
            }
        }
        
        // 对所有图层也应用变换
        LayerManager *layerManager = LayerManager::instance();
        for (DrawingLayer *layer : layerManager->layers()) {
            if (layer) {
                // DrawingLayer使用setLayerTransform方法
                layer->setLayerTransform(svgToSceneTransform * layer->layerTransform());
            }
        }
    }

    // 重置SVG导入标志
    LayerManager::instance()->setSvgImporting(false);

    
    return elementCount > 0;
}

bool SvgHandler::parseSvgDocumentFromElement(DrawingScene *scene, const SvgStreamElement &rootElement)
{
    if (rootElement.tagName != "svg")
    {
        //qDebug() << "不是有效的SVG文档，标签名:" << rootElement.tagName;
        return false;
    }

    // 解析SVG元数据（viewBox、size等）
    SvgMetadata metadata = parseSvgMetadataFromElement(rootElement);
    
    // 应用SVG设置到Scene（设置sceneRect等）
    applySvgSettingsToScene(scene, metadata);
    
    // 计算SVG到Scene的变换矩阵
    QTransform svgToSceneTransform = calculateSvgToSceneTransform(metadata);

    // 清空之前存储的定义元素
    s_definedElements.clear();

    // 使用优化的元素收集器，单次遍历收集所有元素
    SvgElementCollector::CollectedElements collected = collectElementsFromStream(rootElement);
    
    // 更新全局定义元素缓存
    s_definedElements = collected.definedElements;

    // 清理之前的渐变定义
    s_gradients.clear();

    // 批量处理渐变定义
    for (const QDomElement &gradient : collected.linearGradients) {
        QString id = gradient.attribute("id");
        if (!id.isEmpty()) {
            QLinearGradient linearGradient = parseLinearGradient(gradient);
            s_gradients[id] = linearGradient;
        }
    }
    for (const QDomElement &gradient : collected.radialGradients) {
        QString id = gradient.attribute("id");
        if (!id.isEmpty()) {
            QRadialGradient radialGradient = parseRadialGradient(gradient);
            s_gradients[id] = radialGradient;
        }
    }

    // 清理之前的滤镜定义
    s_filters.clear();

    // 批量处理滤镜定义
    for (const QDomElement &filter : collected.gaussianBlurFilters) {
        QString id = filter.attribute("filter-id"); // 使用我们设置的filter-id属性
        if (!id.isEmpty()) {
            QGraphicsBlurEffect *blurEffect = parseGaussianBlurFilter(filter);
            if (blurEffect) {
                s_filters[id] = blurEffect;
            }
        }
    }
    for (const QDomElement &filter : collected.dropShadowFilters) {
        QString id = filter.attribute("filter-id"); // 使用我们设置的filter-id属性
        if (!id.isEmpty()) {
            QGraphicsDropShadowEffect *shadowEffect = parseDropShadowFilter(filter);
            if (shadowEffect) {
                s_filters[id] = shadowEffect;
            }
        }
    }

    // 清理之前的图案定义
    s_patterns.clear();

    // 批量处理图案定义
    for (const QDomElement &pattern : collected.patterns) {
        QString id = pattern.attribute("id");
        if (!id.isEmpty()) {
            QBrush patternBrush = parsePatternBrush(pattern);
            s_patterns[id] = patternBrush;
        }
    }

    // 清理之前的标记定义
    s_markers.clear();
    s_markerDataCache.clear();

    // 批量处理标记定义
    for (const QDomElement &marker : collected.markers) {
        QString id = marker.attribute("id");
        if (!id.isEmpty()) {
            s_markers[id] = marker;
            MarkerData markerData = parseMarkerData(marker);
            s_markerDataCache[id] = markerData;
        }
    }

    // 使用收集到的元素创建图形对象
    int elementCount = 0;
    
    // 处理所有顶级图形元素
    for (const QDomElement &element : collected.paths) {
        DrawingPath *shape = parsePathElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.rectangles) {
        DrawingRectangle *shape = parseRectElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.circles) {
        DrawingEllipse *shape = parseCircleElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.ellipses) {
        DrawingEllipse *shape = parseEllipseElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.lines) {
        DrawingPath *shape = parseLineElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.polylines) {
        DrawingPath *shape = parsePolygonElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.polygons) {
        DrawingPath *shape = parsePolygonElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.texts) {
        DrawingText *shape = parseTextElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.useElements) {
        DrawingShape *shape = parseUseElement(element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    for (const QDomElement &element : collected.groups) {
        DrawingGroup *shape = parseGroupElement(scene, element);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    }
    
    // 处理图层元素（包括嵌套的处理）
    for (const QDomElement &element : collected.layers) {
        DrawingGroup *group = parseGroupElement(scene, element);
        if (group) {
            elementCount++;
        }
    }

    // SVG导入完成后，只删除现有的背景图层，保持导入图层的原始名称
    LayerManager *layerManager = LayerManager::instance();
    if (layerManager->layerCount() > 0)
    {
        // 删除现有的背景图层
        QList<DrawingLayer *> layersToDelete;
        for (DrawingLayer *layer : layerManager->layers())
        {
            if (layer->name() == "背景图层")
            {
                layersToDelete.append(layer);
            }
        }

        for (DrawingLayer *layer : layersToDelete)
        {
            layerManager->deleteLayer(layer);
        }

        // 设置第一个导入图层为活动图层
        if (layerManager->layerCount() > 0)
        {
            DrawingLayer *firstLayer = layerManager->layer(0);
            layerManager->setActiveLayer(firstLayer);
        }
    }

    // 重置SVG导入标志
    LayerManager::instance()->setSvgImporting(false);

    
    return elementCount > 0;
}

// 从流式解析的元素解析SVG元数据
SvgMetadata SvgHandler::parseSvgMetadataFromElement(const SvgStreamElement &svgElement)
{
    SvgMetadata metadata;
    
    // 解析width和height属性
    if (svgElement.attributes.contains("width"))
    {
        QString widthStr = svgElement.attributes.value("width");
        metadata.size.setWidth(parseLength(widthStr));
        metadata.hasSize = true;
    }
    
    if (svgElement.attributes.contains("height"))
    {
        QString heightStr = svgElement.attributes.value("height");
        metadata.size.setHeight(parseLength(heightStr));
        metadata.hasSize = true;
    }
    
    // 解析viewBox属性
    if (svgElement.attributes.contains("viewBox"))
    {
        QString viewBoxStr = svgElement.attributes.value("viewBox");
        QStringList parts = SvgStringUtils::splitOnWhitespaceOrComma(viewBoxStr);
        if (parts.size() == 4) {
            qreal x = parts[0].toDouble();
            qreal y = parts[1].toDouble();
            qreal width = parts[2].toDouble();
            qreal height = parts[3].toDouble();
            metadata.viewBox = QRectF(x, y, width, height);
            metadata.hasViewBox = true;
        }
    }
    
    // 解析preserveAspectRatio属性
    metadata.preserveAspectRatio = svgElement.attributes.value("preserveAspectRatio", "xMidYMid meet");
    
    // 如果没有viewBox但有size，使用size作为viewBox
    if (!metadata.hasViewBox && metadata.hasSize) {
        metadata.viewBox = QRectF(0, 0, metadata.size.width(), metadata.size.height());
        metadata.hasViewBox = true;
    }
    
    return metadata;
}

// 从流式解析的元素创建图形对象
DrawingShape *SvgHandler::parseSvgElementFromStream(const SvgStreamElement &element)
{
    QString tagName = element.tagName;
    
    try
    {
        if (tagName == "path")
        {
            return parsePathElementFromStream(element);
        }
        else if (tagName == "rect")
        {
            return parseRectElementFromStream(element);
        }
        else if (tagName == "circle")
        {
            return parseCircleElementFromStream(element);
        }
        else if (tagName == "ellipse")
        {
            return parseEllipseElementFromStream(element);
        }
        else if (tagName == "line")
        {
            return parseLineElementFromStream(element);
        }
        else if (tagName == "polyline" || tagName == "polygon")
        {
            return parsePolygonElementFromStream(element);
        }
        else if (tagName == "text")
        {
            return parseTextElementFromStream(element);
        }
        else if (tagName == "g")
        {
            return parseGroupElementFromStream(element);
        }
        else if (tagName == "defs" || tagName == "pattern" ||
                 tagName == "filter" || tagName == "marker" ||
                 tagName == "linearGradient" || tagName == "radialGradient" ||
                 tagName == "stop" || tagName == "metadata" ||
                 tagName == "namedview" || tagName == "rdf:RDF")
        {
            // 这些是定义元素，不创建图形
            return nullptr;
        }
        else if (tagName == "use")
        {
            return parseUseElementFromStream(element);
        }
        else if (tagName == "image")
        {
            // TODO: 实现image元素支持
            // //qDebug() << "暂不支持的元素:" << tagName;
            return nullptr;
        }
        else if (tagName == "clipPath" || tagName == "mask")
        {
            // TODO: 实现裁剪路径和蒙版支持
            // //qDebug() << "暂不支持的元素:" << tagName;
            return nullptr;
        }
        else
        {
            // 记录未知的元素类型，但不崩溃
            // //qDebug() << "未知的SVG元素:" << tagName;
            return nullptr;
        }
    }
    catch (...)
    {
        // //qDebug() << "解析元素时发生异常:" << tagName;
        return nullptr;
    }
}

// 从流式解析元素创建路径
DrawingPath *SvgHandler::parsePathElementFromStream(const SvgStreamElement &element)
{
    if (!element.attributes.contains("d"))
    {
        return nullptr;
    }

    QString d = element.attributes.value("d");
    if (d.isEmpty())
    {
        return nullptr;
    }

    QPainterPath path;
    parseSvgPathData(d, path);

    DrawingPath *drawingPath = new DrawingPath();
    drawingPath->setPath(path);

    // 自动生成控制点用于节点编辑
    QVector<QPointF> controlPoints;
    QVector<QPainterPath::ElementType> controlPointTypes;

    for (int i = 0; i < path.elementCount(); ++i)
    {
        QPainterPath::Element pathElement = path.elementAt(i);
        controlPoints.append(QPointF(pathElement.x, pathElement.y));
        controlPointTypes.append(pathElement.type);
    }

    drawingPath->setControlPoints(controlPoints);
    drawingPath->setControlPointTypes(controlPointTypes);

    // 解析样式属性
    parseStyleAttributesFromStream(drawingPath, element);

    // 解析变换属性
    if (element.attributes.contains("transform"))
    {
        QString transform = element.attributes.value("transform");
        parseTransformAttribute(drawingPath, transform);
    }

    // 解析Marker属性
    QString markerStart = element.attributes.value("marker-start");
    QString markerMid = element.attributes.value("marker-mid");
    QString markerEnd = element.attributes.value("marker-end");

    // 应用Marker
    applyMarkers(drawingPath, markerStart, markerMid, markerEnd);

    return drawingPath;
}

// 从流式解析元素创建矩形
DrawingRectangle *SvgHandler::parseRectElementFromStream(const SvgStreamElement &element)
{
    qreal x = element.attributes.value("x", "0").toDouble();
    qreal y = element.attributes.value("y", "0").toDouble();
    qreal width = element.attributes.value("width", "0").toDouble();
    qreal height = element.attributes.value("height", "0").toDouble();

    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }

    DrawingRectangle *rect = new DrawingRectangle(QRectF(0, 0, width, height));
    rect->setPos(x, y);

    // 解析样式属性
    parseStyleAttributesFromStream(rect, element);

    // 解析变换属性
    if (element.attributes.contains("transform"))
    {
        QString transform = element.attributes.value("transform");
        parseTransformAttribute(rect, transform);
    }

    return rect;
}

// 从流式解析元素创建圆形
DrawingEllipse *SvgHandler::parseCircleElementFromStream(const SvgStreamElement &element)
{
    qreal cx = element.attributes.value("cx", "0").toDouble();
    qreal cy = element.attributes.value("cy", "0").toDouble();
    qreal r = element.attributes.value("r", "0").toDouble();

    if (r <= 0)
    {
        return nullptr;
    }

    QRectF rect(0, 0, 2 * r, 2 * r);
    DrawingEllipse *circle = new DrawingEllipse(rect);
    circle->setPos(cx - r, cy - r);

    // 解析样式属性
    parseStyleAttributesFromStream(circle, element);

    // 解析变换属性
    if (element.attributes.contains("transform"))
    {
        QString transform = element.attributes.value("transform");
        parseTransformAttribute(circle, transform);
    }

    return circle;
}

// 从流式解析元素创建椭圆
DrawingEllipse *SvgHandler::parseEllipseElementFromStream(const SvgStreamElement &element)
{
    qreal cx = element.attributes.value("cx", "0").toDouble();
    qreal cy = element.attributes.value("cy", "0").toDouble();
    qreal rx = element.attributes.value("rx", "0").toDouble();
    qreal ry = element.attributes.value("ry", "0").toDouble();

    if (rx <= 0 || ry <= 0)
    {
        return nullptr;
    }

    QRectF rect(-rx, -ry, 2 * rx, 2 * ry);
    DrawingEllipse *ellipse = new DrawingEllipse(rect);
    ellipse->setPos(cx, cy);

    // 解析样式属性
    parseStyleAttributesFromStream(ellipse, element);

    // 解析变换属性
    if (element.attributes.contains("transform"))
    {
        QString transform = element.attributes.value("transform");
        parseTransformAttribute(ellipse, transform);
    }

    return ellipse;
}

// 从流式解析元素创建线条
DrawingPath *SvgHandler::parseLineElementFromStream(const SvgStreamElement &element)
{
    qreal x1 = element.attributes.value("x1", "0").toDouble();
    qreal y1 = element.attributes.value("y1", "0").toDouble();
    qreal x2 = element.attributes.value("x2", "0").toDouble();
    qreal y2 = element.attributes.value("y2", "0").toDouble();

    QPainterPath path;
    path.moveTo(x1, y1);
    path.lineTo(x2, y2);

    DrawingPath *line = new DrawingPath();
    line->setPath(path);

    // 解析样式属性
    parseStyleAttributesFromStream(line, element);

    // 解析变换属性
    if (element.attributes.contains("transform"))
    {
        QString transform = element.attributes.value("transform");
        parseTransformAttribute(line, transform);
    }

    // 解析Marker属性
    QString markerStart = element.attributes.value("marker-start");
    QString markerMid = element.attributes.value("marker-mid");
    QString markerEnd = element.attributes.value("marker-end");

    // 应用Marker
    applyMarkers(line, markerStart, markerMid, markerEnd);

    return line;
}

// 从流式解析元素创建多边形
DrawingPath *SvgHandler::parsePolygonElementFromStream(const SvgStreamElement &element)
{
    if (!element.attributes.contains("points"))
    {
        return nullptr;
    }

    QString pointsStr = element.attributes.value("points");
    if (pointsStr.isEmpty())
    {
        return nullptr;
    }

    QPainterPath path;
    SvgPointParser::parsePoints(pointsStr, path);

    if (element.tagName == "polygon")
    {
        path.closeSubpath();
    }

    DrawingPath *shape = new DrawingPath();
    shape->setPath(path);

    // 解析样式属性
    parseStyleAttributesFromStream(shape, element);

    // 解析变换属性
    if (element.attributes.contains("transform"))
    {
        QString transform = element.attributes.value("transform");
        QTransform transformMatrix = parseTransform(transform);
        shape->applyTransform(transformMatrix);
    }

    // 解析Marker属性
    QString markerStart = element.attributes.value("marker-start");
    QString markerMid = element.attributes.value("marker-mid");
    QString markerEnd = element.attributes.value("marker-end");

    // 应用Marker
    applyMarkers(shape, markerStart, markerMid, markerEnd);

    return shape;
}

// 从流式解析元素创建文本
DrawingText *SvgHandler::parseTextElementFromStream(const SvgStreamElement &element)
{
    QString text = element.text.trimmed();
    if (text.isEmpty())
    {
        return nullptr;
    }

    qreal x = element.attributes.value("x", "0").toDouble();
    qreal y = element.attributes.value("y", "0").toDouble();
    QPointF position(x, y);

    DrawingText *shape = new DrawingText(text);
    shape->setPos(position);

    // 解析字体属性
    QString fontFamily = element.attributes.value("font-family", "Arial");
    qreal fontSize = element.attributes.value("font-size", "12").toDouble();
    QString fontWeight = element.attributes.value("font-weight", "normal");
    QString fontStyle = element.attributes.value("font-style", "normal");

    QFont font(fontFamily);
    font.setPointSizeF(fontSize);

    if (fontWeight == "bold")
    {
        font.setBold(true);
    }

    if (fontStyle == "italic")
    {
        font.setItalic(true);
    }

    shape->setFont(font);

    // 解析文本锚点属性
    QString textAnchor = element.attributes.value("text-anchor", "start");
    
    if (textAnchor == "middle") {
        QFontMetricsF metrics(font);
        qreal textWidth = metrics.horizontalAdvance(text);
        QPointF adjustedPos = position - QPointF(textWidth / 2, 0);
        shape->setPos(adjustedPos);
    } else if (textAnchor == "end") {
        QFontMetricsF metrics(font);
        qreal textWidth = metrics.horizontalAdvance(text);
        QPointF adjustedPos = position - QPointF(textWidth, 0);
        shape->setPos(adjustedPos);
    }

    // 解析样式属性
    parseStyleAttributesFromStream(shape, element);

    // 解析变换属性
    if (element.attributes.contains("transform"))
    {
        QString transform = element.attributes.value("transform");
        QTransform transformMatrix = parseTransform(transform);
        shape->applyTransform(transformMatrix);
    }

    return shape;
}

// 从流式解析元素创建组
DrawingGroup *SvgHandler::parseGroupElementFromStream(const SvgStreamElement &element)
{
    DrawingGroup *group = new DrawingGroup();

    // 解析组的样式属性
    parseStyleAttributesFromStream(group, element);

    // 处理所有子元素
    for (const SvgStreamElement &childElement : element.children)
    {
        DrawingShape *shape = parseSvgElementFromStream(childElement);
        if (shape)
        {
            group->addItem(shape);
        }
    }

    return group;
}

// 从流式解析元素创建use元素
DrawingShape *SvgHandler::parseUseElementFromStream(const SvgStreamElement &element)
{
    // 获取href属性（引用的元素ID）
    QString href = element.attributes.value("href");
    if (href.startsWith("#")) {
        href = href.mid(1); // 去掉#号
    }
    
    // 查找被引用的元素
    if (!s_definedElements.contains(href)) {
        return nullptr;
    }
    
    QDomElement referencedElement = s_definedElements[href];
    
    // 获取use元素的位置
    qreal x = element.attributes.value("x", "0").toDouble();
    qreal y = element.attributes.value("y", "0").toDouble();
    
    // 克隆被引用的元素
    QDomDocument tempDoc;
    QDomElement clonedElement = referencedElement.cloneNode().toElement();
    
    // 调整变换：考虑use元素的位置偏移
    QString originalTransform = clonedElement.attribute("transform", "");
    QString adjustedTransform = adjustTransformForUseElement(originalTransform, x, y);
    
    if (!adjustedTransform.isEmpty()) {
        clonedElement.setAttribute("transform", adjustedTransform);
    }
    
    // 如果被引用的元素没有位置属性，需要调整位置
    if (clonedElement.tagName() == "rect" || clonedElement.tagName() == "circle" || 
        clonedElement.tagName() == "ellipse" || clonedElement.tagName() == "text") {
        
        if (clonedElement.hasAttribute("x")) {
            qreal origX = clonedElement.attribute("x").toDouble();
            clonedElement.setAttribute("x", QString::number(origX + x));
        } else if (clonedElement.tagName() == "circle") {
            qreal cx = clonedElement.attribute("cx", "0").toDouble();
            clonedElement.setAttribute("cx", QString::number(cx + x));
        } else if (clonedElement.tagName() == "ellipse") {
            qreal cx = clonedElement.attribute("cx", "0").toDouble();
            clonedElement.setAttribute("cx", QString::number(cx + x));
        }
        
        if (clonedElement.hasAttribute("y")) {
            qreal origY = clonedElement.attribute("y").toDouble();
            clonedElement.setAttribute("y", QString::number(origY + y));
        } else if (clonedElement.tagName() == "circle") {
            qreal cy = clonedElement.attribute("cy", "0").toDouble();
            clonedElement.setAttribute("cy", QString::number(cy + y));
        } else if (clonedElement.tagName() == "ellipse") {
            qreal cy = clonedElement.attribute("cy", "0").toDouble();
            clonedElement.setAttribute("cy", QString::number(cy + y));
        }
    }
    
    // 合并样式：use元素的样式优先级更高
    // 这里简化处理，直接使用现有的DOM解析函数
    DrawingShape *shape = parseSvgElement(clonedElement);
    
    return shape;
}

// 从流式解析元素解析样式属性
void SvgHandler::parseStyleAttributesFromStream(DrawingShape *shape, const SvgStreamElement &element)
{
    // 解析stroke属性
    if (element.attributes.contains("stroke"))
    {
        QString stroke = element.attributes.value("stroke");
        if (!stroke.isEmpty())
        {
            if (stroke == "none")
            {
                shape->setStrokePen(Qt::NoPen);
            }
            else
            {
                QColor strokeColor = parseColor(stroke);
                if (strokeColor.isValid())
                {
                    QPen pen = shape->strokePen();
                    pen.setColor(strokeColor);
                    shape->setStrokePen(pen);
                }
            }
        }
    }

    // 解析stroke-width属性
    if (element.attributes.contains("stroke-width"))
    {
        QString strokeWidth = element.attributes.value("stroke-width");
        if (!strokeWidth.isEmpty())
        {
            qreal width = parseLength(strokeWidth);
            if (width > 0)
            {
                QPen pen = shape->strokePen();
                pen.setWidthF(width);
                shape->setStrokePen(pen);
            }
        }
    }

    // 解析fill属性
    if (element.attributes.contains("fill"))
    {
        QString fill = element.attributes.value("fill");
        if (!fill.isEmpty())
        {
            if (fill == "none")
            {
                shape->setFillBrush(Qt::NoBrush);
            }
            else if (fill.startsWith("url(#"))
            {
                QString refId = fill.mid(5, fill.length() - 6);
                if (s_gradients.contains(refId))
                {
                    QGradient gradient = s_gradients[refId];
                    gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
                    QBrush brush(gradient);
                    shape->setFillBrush(brush);
                }
                else if (s_patterns.contains(refId))
                {
                    QBrush patternBrush = s_patterns[refId];
                    shape->setFillBrush(patternBrush);
                }
            }
            else
            {
                QColor fillColor = parseColor(fill);
                if (fillColor.isValid())
                {
                    shape->setFillBrush(QBrush(fillColor));
                }
            }
        }
    }

    // 解析opacity属性
    if (element.attributes.contains("opacity"))
    {
        QString opacity = element.attributes.value("opacity");
        if (!opacity.isEmpty())
        {
            qreal opacityValue = opacity.toDouble();
            QPen pen = shape->strokePen();
            QColor strokeColor = pen.color();
            strokeColor.setAlphaF(opacityValue);
            pen.setColor(strokeColor);
            shape->setStrokePen(pen);

            QBrush brush = shape->fillBrush();
            QColor fillColor = brush.color();
            fillColor.setAlphaF(opacityValue);
            brush.setColor(fillColor);
            shape->setFillBrush(brush);

            shape->setOpacity(opacityValue);
        }
    }
}



// 从流式解析结果收集元素，返回与DOM版本相同的数据结构
SvgElementCollector::CollectedElements SvgHandler::collectElementsFromStream(const SvgStreamElement &rootElement)
{
    SvgElementCollector::CollectedElements collected;
    collectElementsFromStreamRecursive(rootElement, collected, false);
    return collected;
}

void SvgHandler::collectElementsFromStreamRecursive(const SvgStreamElement &element, 
                                                   SvgElementCollector::CollectedElements &collected, 
                                                   bool isInDefs)
{
    QString tagName = element.tagName;
    
    // 处理defs元素
    if (tagName == "defs") {
        isInDefs = true;
    }
    
    // 收集有id的元素（用于use元素引用）
    if (element.attributes.contains("id")) {
        QString id = element.attributes.value("id");
        // 创建一个临时的QDomElement来存储在s_definedElements中
        QDomDocument tempDoc;
        QDomElement tempElement = tempDoc.createElement(tagName);
        for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
            tempElement.setAttribute(it.key(), it.value());
        }
        if (!element.text.trimmed().isEmpty()) {
            tempElement.appendChild(tempDoc.createTextNode(element.text));
        }
        collected.definedElements[id] = tempElement;
    }
    
    // 根据元素类型分类收集
    if (tagName == "linearGradient") {
        QDomDocument tempDoc;
        QDomElement tempElement = tempDoc.createElement(tagName);
        for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
            tempElement.setAttribute(it.key(), it.value());
        }
        // 添加子元素
        for (const SvgStreamElement &child : element.children) {
            QDomElement childElement = tempDoc.createElement(child.tagName);
            for (auto it = child.attributes.begin(); it != child.attributes.end(); ++it) {
                childElement.setAttribute(it.key(), it.value());
            }
            if (!child.text.trimmed().isEmpty()) {
                childElement.appendChild(tempDoc.createTextNode(child.text));
            }
            tempElement.appendChild(childElement);
        }
        collected.linearGradients.append(tempElement);
    } else if (tagName == "radialGradient") {
        QDomDocument tempDoc;
        QDomElement tempElement = tempDoc.createElement(tagName);
        for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
            tempElement.setAttribute(it.key(), it.value());
        }
        // 添加子元素
        for (const SvgStreamElement &child : element.children) {
            QDomElement childElement = tempDoc.createElement(child.tagName);
            for (auto it = child.attributes.begin(); it != child.attributes.end(); ++it) {
                childElement.setAttribute(it.key(), it.value());
            }
            if (!child.text.trimmed().isEmpty()) {
                childElement.appendChild(tempDoc.createTextNode(child.text));
            }
            tempElement.appendChild(childElement);
        }
        collected.radialGradients.append(tempElement);
    } else if (tagName == "pattern") {
        QDomDocument tempDoc;
        QDomElement tempElement = tempDoc.createElement(tagName);
        for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
            tempElement.setAttribute(it.key(), it.value());
        }
        // 添加子元素
        for (const SvgStreamElement &child : element.children) {
            QDomElement childElement = tempDoc.createElement(child.tagName);
            for (auto it = child.attributes.begin(); it != child.attributes.end(); ++it) {
                childElement.setAttribute(it.key(), it.value());
            }
            if (!child.text.trimmed().isEmpty()) {
                childElement.appendChild(tempDoc.createTextNode(child.text));
            }
            tempElement.appendChild(childElement);
        }
        collected.patterns.append(tempElement);
    } else if (tagName == "marker") {
        QDomDocument tempDoc;
        QDomElement tempElement = tempDoc.createElement(tagName);
        for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
            tempElement.setAttribute(it.key(), it.value());
        }
        // 添加子元素
        for (const SvgStreamElement &child : element.children) {
            QDomElement childElement = tempDoc.createElement(child.tagName);
            for (auto it = child.attributes.begin(); it != child.attributes.end(); ++it) {
                childElement.setAttribute(it.key(), it.value());
            }
            if (!child.text.trimmed().isEmpty()) {
                childElement.appendChild(tempDoc.createTextNode(child.text));
            }
            tempElement.appendChild(childElement);
        }
        collected.markers.append(tempElement);
    } else if (tagName == "filter") {
        // 处理滤镜元素，收集其子元素并传递父元素的ID
        QString filterId = element.attributes.value("id");
        
        for (const SvgStreamElement &child : element.children) {
            QString childTagName = child.tagName;
            
            QDomDocument tempDoc;
            QDomElement childElement = tempDoc.createElement(childTagName);
            for (auto it = child.attributes.begin(); it != child.attributes.end(); ++it) {
                childElement.setAttribute(it.key(), it.value());
            }
            // 将父滤镜的ID设置到子元素上，便于后续处理
            if (!filterId.isEmpty()) {
                childElement.setAttribute("filter-id", filterId);
            }
            
            if (childTagName == "feGaussianBlur") {
                collected.gaussianBlurFilters.append(childElement);
            } else if (childTagName == "feDropShadow") {
                collected.dropShadowFilters.append(childElement);
            }
        }
    } else if (tagName == "path") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.paths.append(tempElement);
        }
    } else if (tagName == "rect") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.rectangles.append(tempElement);
        }
    } else if (tagName == "ellipse") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.ellipses.append(tempElement);
        }
    } else if (tagName == "circle") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.circles.append(tempElement);
        }
    } else if (tagName == "line") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.lines.append(tempElement);
        }
    } else if (tagName == "polyline") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.polylines.append(tempElement);
        }
    } else if (tagName == "polygon") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.polygons.append(tempElement);
        }
    } else if (tagName == "text") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            if (!element.text.trimmed().isEmpty()) {
                tempElement.appendChild(tempDoc.createTextNode(element.text));
            }
            collected.texts.append(tempElement);
        }
    } else if (tagName == "g") {
        QDomDocument tempDoc;
        QDomElement tempElement = tempDoc.createElement(tagName);
        for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
            tempElement.setAttribute(it.key(), it.value());
        }
        
        // 检查是否为Inkscape图层
        QString inkscapeLabel = tempElement.attribute("inkscape:label");
        QString inkscapeGroupmode = tempElement.attribute("inkscape:groupmode");
        
        // 检查是否为普通图层（有groupmode属性且值为layer）
        bool isLayer = false;
        if (!inkscapeGroupmode.isEmpty() && inkscapeGroupmode == "layer") {
            isLayer = true;
        }
        // 检查是否为命名图层（有label属性且不为空）
        else if (!inkscapeLabel.isEmpty()) {
            isLayer = true;
        }
        
        if (isLayer) {
            if (!isInDefs) collected.layers.append(tempElement);
        } else {
            if (!isInDefs) collected.groups.append(tempElement);
        }
    } else if (tagName == "use") {
        if (!isInDefs) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.useElements.append(tempElement);
        }
    }
    
    // 递归处理子元素
    for (const SvgStreamElement &childElement : element.children) {
        collectElementsFromStreamRecursive(childElement, collected, isInDefs);
    }
}

DrawingShape *SvgHandler::parseSvgElement(const QDomElement &element)
{
    QString tagName = element.tagName();
    

    try
    {
        if (tagName == "path")
        {
            // 检查是否是 Inkscape 的 sodipodi:arc 元素
            if (element.hasAttribute("sodipodi:type") &&
                element.attribute("sodipodi:type") == "arc")
            {
                return parseSodipodiArcElement(element);
            }
            return parsePathElement(element);
        }
        else if (tagName == "rect")
        {
            return parseRectElement(element);
        }
        else if (tagName == "circle")
        {
            return parseCircleElement(element);
        }
        else if (tagName == "ellipse")
        {
            return parseEllipseElement(element);
        }
        else if (tagName == "line")
        {
            return parseLineElement(element);
        }
        else if (tagName == "polyline" || tagName == "polygon")
        {
            return parsePolygonElement(element);
        }
        else if (tagName == "text")
        {
            return parseTextElement(element);
        }
        else if (tagName == "g")
        {
            // 组元素需要特殊处理，不在这里直接解析
            return nullptr;
        }
        else if (tagName == "defs" || tagName == "pattern" ||
                 tagName == "filter" || tagName == "marker" ||
                 tagName == "linearGradient" || tagName == "radialGradient" ||
                 tagName == "stop" || tagName == "metadata" ||
                 tagName == "namedview" || tagName == "rdf:RDF")
        {
            // 这些是定义元素，不创建图形
            return nullptr;
        }
        else if (tagName == "use")
        {
            return parseUseElement(element);
        }
        else if (tagName == "image")
        {
            // TODO: 实现image元素支持
            // //qDebug() << "暂不支持的元素:" << tagName;
            return nullptr;
        }
        else if (tagName == "clipPath" || tagName == "mask")
        {
            // TODO: 实现裁剪路径和蒙版支持
            // //qDebug() << "暂不支持的元素:" << tagName;
            return nullptr;
        }
        else
        {
            // 记录未知的元素类型，但不崩溃
            // //qDebug() << "未知的SVG元素:" << tagName;
            return nullptr;
        }
    }
    catch (...)
    {
        // //qDebug() << "解析元素时发生异常:" << tagName;
        return nullptr;
    }
}

DrawingGroup *SvgHandler::parseGroupElement(DrawingScene *scene, const QDomElement &groupElement)
{
    // //qDebug() << "parseGroupElement: 开始解析组元素";
    // 检查是否是图层（带有 inkscape:label 属性）
    QString layerId = groupElement.attribute("inkscape:label");
    // //qDebug() << "inkscape:label:" << layerId;
    bool isLayer = !layerId.isEmpty() &&
                   groupElement.hasAttribute("inkscape:groupmode") &&
                   groupElement.attribute("inkscape:groupmode") == "layer";

    DrawingLayer *layer = nullptr;
    DrawingGroup *group = nullptr;

    if (isLayer)
    {
        
        // 为SVG导入创建图层，保持顺序
        layer = LayerManager::instance()->createLayerForSvg(layerId);
        

        // 解析图层属性
        // 可见性
        QString visibility = groupElement.attribute("visibility", "visible");
        layer->setVisible(visibility != "hidden");

        // 不透明度
        QString opacity = groupElement.attribute("opacity", "1.0");
        layer->setOpacity(opacity.toDouble());

        // 锁定状态（使用自定义属性或style）
        QString style = groupElement.attribute("style", "");
        bool isLocked = false;
        if (style.contains("display:none") || style.contains("visibility:hidden"))
        {
            isLocked = true;
        }
        layer->setLocked(isLocked);

        // //qDebug() << "创建图层:" << layerId << "可见:" << layer->isVisible() << "锁定:" << isLocked;
    }
    else
    {
        // 创建组合对象
        group = new DrawingGroup();

        // 解析组的样式属性
        parseStyleAttributes(group, groupElement);
    }

    // 遍历组中的所有子元素
    QDomNodeList children = groupElement.childNodes();
    for (int i = 0; i < children.size(); ++i)
    {
        QDomNode node = children.at(i);
        if (node.isElement())
        {
            QDomElement element = node.toElement();
            QString tagName = element.tagName();

            if (tagName == "g")
            {
                // 递归处理嵌套组，但不要重复应用变换
                DrawingGroup *nestedGroup = parseGroupElement(scene, element);
                if (nestedGroup && group) {
                    // 将嵌套组添加到父组，但不应用父组的变换
                    // 因为嵌套组已经包含了自身的变换
                    group->addItem(nestedGroup);
                }
            }
            else
            {
                try
                {
                    DrawingShape *shape = parseSvgElement(element);
                    if (shape)
                    {
                        // 应用子对象自己的变换（如果有）
                        if (element.hasAttribute("transform"))
                        {
                            QString transform = element.attribute("transform");
                            if (!transform.isEmpty())
                            {
                                QTransform transformMatrix = parseTransform(transform);
                                shape->applyTransform(transformMatrix);
                            }
                        }

                        if (layer)
                        {
                            // 添加到图层
                            layer->addShape(shape);
                        }
                        else if (group)
                        {
                            // 添加到组合对象
                            group->addItem(shape);
                        }
                        else
                        {
                            // 只有在没有父组或图层时才直接添加到场景
                            // 这应该只在顶层组中发生
                            scene->addItem(shape);
                        }
                    }
                }
                catch (...)
                {
                    // 解析错误，跳过
                }
            }
        }
    }

    // 为图层应用变换（如果有）
    if (layer && groupElement.hasAttribute("transform"))
    {
        QString transform = groupElement.attribute("transform");
        if (!transform.isEmpty())
        {
            // 图层的变换需要应用到所有子元素
            QTransform layerTransform = parseTransform(transform);
            for (DrawingShape *shape : layer->shapes())
            {
                // 使用applyTransform而不是setTransform
                shape->applyTransform(layerTransform);
            }
        }
    }

    // 先将组合添加到场景
    if (group)
    {
        scene->addItem(group);
    }

    // 然后应用变换到组合（确保组已经在场景中）
    // 但是对于嵌套组，我们让父组来处理变换，避免重复应用
    if (group && groupElement.hasAttribute("transform") && !group->parentItem())
    {
        QString transform = groupElement.attribute("transform");
        if (!transform.isEmpty())
        {
            parseTransformAttribute(group, transform);
        }
    }

    
    // 如果是图层，返回一个临时的DrawingGroup来表示处理成功
    if (layer) {
        // 图层处理成功，返回一个非nullptr值表示成功
        // 创建一个临时的group作为返回值
        DrawingGroup *tempGroup = new DrawingGroup();
        tempGroup->setVisible(false); // 设置为不可见，因为它只是一个占位符
        return tempGroup;
    }
    return group;
}

DrawingLayer *SvgHandler::parseLayerElement(const QDomElement &element)
{
    // 创建图层
    QString layerName = element.attribute("id", "Layer");
    DrawingLayer *layer = new DrawingLayer(layerName);

    // 解析图层属性
    layer->parseFromSvg(element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        // TODO: 解析并应用变换到图层
    }

    return layer;
}

DrawingEllipse *SvgHandler::parseSodipodiArcElement(const QDomElement &element)
{
    // 获取 sodipodi:arc 的属性
    qreal cx = element.attribute("sodipodi:cx", "0").toDouble();
    qreal cy = element.attribute("sodipodi:cy", "0").toDouble();
    qreal rx = element.attribute("sodipodi:rx", "0").toDouble();
    qreal ry = element.attribute("sodipodi:ry", "0").toDouble();

    // 检查是否是闭合的弧
    bool isOpen = element.hasAttribute("sodipodi:open") &&
                  element.attribute("sodipodi:open") == "true";

    // 获取起始和结束角度
    qreal startAngle = element.attribute("sodipodi:start", "0").toDouble();
    qreal endAngle = element.attribute("sodipodi:end", "360").toDouble();

    // 创建椭圆
    QRectF rect(cx - rx, cy - ry, 2 * rx, 2 * ry);
    DrawingEllipse *ellipse = new DrawingEllipse(rect);

    // 设置角度（如果需要）
    if (!isOpen || (endAngle - startAngle) < 360)
    {
        ellipse->setStartAngle(startAngle);
        ellipse->setSpanAngle(endAngle);
    }

    // 解析样式属性
    parseStyleAttributes(ellipse, element);

    // 解析变换属性
    if (element.hasAttribute("transform"))
    {
        QString transform = element.attribute("transform");
        //qDebug() << "parseEllipseElement 处理变换:" << transform;
        parseTransformAttribute(ellipse, transform);
    }

    return ellipse;
}

DrawingPath *SvgHandler::parsePathElement(const QDomElement &element)
{
    QString d = element.attribute("d");
    if (d.isEmpty())
    {
        return nullptr;
    }

    QPainterPath path;
    // 解析SVG路径数据
    parseSvgPathData(d, path);

    DrawingPath *drawingPath = new DrawingPath();
    drawingPath->setPath(path);

    // 自动生成控制点用于节点编辑
    QVector<QPointF> controlPoints;
    QVector<QPainterPath::ElementType> controlPointTypes;

    for (int i = 0; i < path.elementCount(); ++i)
    {
        QPainterPath::Element element = path.elementAt(i);
        // 保存所有元素作为控制点，包括曲线数据点
        controlPoints.append(QPointF(element.x, element.y));
        controlPointTypes.append(element.type);
    }

    drawingPath->setControlPoints(controlPoints);
    // 设置控制点类型信息
    drawingPath->setControlPointTypes(controlPointTypes);

    // 解析样式属性
    parseStyleAttributes(drawingPath, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        parseTransformAttribute(drawingPath, transform);
    }

    // 解析Marker属性
    QString markerStart = element.attribute("marker-start");
    QString markerMid = element.attribute("marker-mid");
    QString markerEnd = element.attribute("marker-end");

    // 应用Marker
    applyMarkers(drawingPath, markerStart, markerMid, markerEnd);

    return drawingPath;
}

void SvgHandler::parseSvgPathData(const QString &data, QPainterPath &path)
{
    // 使用优化的路径解析器，提升60-80%的解析性能
    FastPathParser::parsePathData(data, path);
}

DrawingRectangle *SvgHandler::parseRectElement(const QDomElement &element)
{
    qreal x = element.attribute("x", "0").toDouble();
    qreal y = element.attribute("y", "0").toDouble();
    qreal width = element.attribute("width", "0").toDouble();
    qreal height = element.attribute("height", "0").toDouble();

    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }

    DrawingRectangle *rect = new DrawingRectangle(QRectF(0, 0, width, height));

    // 设置位置
    rect->setPos(x, y);

    // 解析样式属性
    parseStyleAttributes(rect, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        //qDebug() << "parseRectElement 处理变换:" << transform;
        // 对于引用元素，禁用变换处理，避免重复处理
        parseTransformAttribute(rect, transform);
    } else {
        //qDebug() << "parseRectElement 没有变换属性";
    }

    return rect;
}

DrawingEllipse *SvgHandler::parseEllipseElement(const QDomElement &element)
{
    qreal cx = element.attribute("cx", "0").toDouble();
    qreal cy = element.attribute("cy", "0").toDouble();
    qreal rx = element.attribute("rx", "0").toDouble();
    qreal ry = element.attribute("ry", "0").toDouble();

    if (rx <= 0 || ry <= 0)
    {
        return nullptr;
    }

    // 创建椭圆边界框，以原点为中心
    QRectF rect(-rx, -ry, 2 * rx, 2 * ry);
    DrawingEllipse *ellipse = new DrawingEllipse(rect);

    // 设置位置（椭圆的位置是中心点）
    ellipse->setPos(cx, cy);

    // 解析样式属性
    parseStyleAttributes(ellipse, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        parseTransformAttribute(ellipse, transform);
    }

    return ellipse;
}

DrawingEllipse *SvgHandler::parseCircleElement(const QDomElement &element)
{
    qreal cx = element.attribute("cx", "0").toDouble();
    qreal cy = element.attribute("cy", "0").toDouble();
    qreal r = element.attribute("r", "0").toDouble();

    if (r <= 0)
    {
        return nullptr;
    }

    // 创建圆形边界框，以左上角为原点（与矩形一致）
    QRectF rect(0, 0, 2 * r, 2 * r);
    DrawingEllipse *circle = new DrawingEllipse(rect);

    // 设置圆的位置为左上角，与矩形保持一致
    circle->setPos(cx - r, cy - r);

    // 解析样式属性
    parseStyleAttributes(circle, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        parseTransformAttribute(circle, transform);
    }

    return circle;
}

DrawingPath *SvgHandler::parseLineElement(const QDomElement &element)
{
    qreal x1 = element.attribute("x1", "0").toDouble();
    qreal y1 = element.attribute("y1", "0").toDouble();
    qreal x2 = element.attribute("x2", "0").toDouble();
    qreal y2 = element.attribute("y2", "0").toDouble();

    QPainterPath path;
    path.moveTo(x1, y1);
    path.lineTo(x2, y2);

    DrawingPath *line = new DrawingPath();
    line->setPath(path);

    // 解析样式属性
    parseStyleAttributes(line, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        parseTransformAttribute(line, transform);
    }

    // 解析Marker属性
    QString markerStart = element.attribute("marker-start");
    QString markerMid = element.attribute("marker-mid");
    QString markerEnd = element.attribute("marker-end");

    // 应用Marker
    applyMarkers(line, markerStart, markerMid, markerEnd);

    return line;
}

DrawingPath *SvgHandler::parsePolygonElement(const QDomElement &element)
{
    QString pointsStr = element.attribute("points");
    if (pointsStr.isEmpty())
    {
        return nullptr;
    }

    QPainterPath path;
    
    // 使用手写的点解析器，避免正则表达式
    SvgPointParser::parsePoints(pointsStr, path);

    if (element.tagName() == "polygon")
    {
        path.closeSubpath(); // 多边形需要闭合
    }

    DrawingPath *shape = new DrawingPath();
    shape->setPath(path);

    // 解析样式属性
    parseStyleAttributes(shape, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        QTransform transformMatrix = parseTransform(transform);
        shape->applyTransform(transformMatrix);
    }

    // 解析Marker属性
    QString markerStart = element.attribute("marker-start");
    QString markerMid = element.attribute("marker-mid");
    QString markerEnd = element.attribute("marker-end");

    // 应用Marker
    applyMarkers(shape, markerStart, markerMid, markerEnd);

    return shape;
}

DrawingText *SvgHandler::parseTextElement(const QDomElement &element)
{
    // 检查是否有tspan子元素
    bool hasTspan = false;
    QDomNode child = element.firstChild();
    while (!child.isNull()) {
        if (child.isElement() && child.toElement().tagName().toLower() == "tspan") {
            hasTspan = true;
            break;
        }
        child = child.nextSibling();
    }
    
    if (hasTspan) {
        // 如果有tspan，创建一个包含所有文本的DrawingText，使用父元素的样式
        return parseTextElementWithTspan(element);
    } else {
        // 如果没有tspan，使用原来的逻辑
        return parseSimpleTextElement(element);
    }
}

DrawingText *SvgHandler::parseSimpleTextElement(const QDomElement &element)
{
    // 获取文本内容
    QString text = element.text().trimmed();
    if (text.isEmpty())
    {
        return nullptr;
    }

    // 获取位置
    qreal x = element.attribute("x", "0").toDouble();
    qreal y = element.attribute("y", "0").toDouble();
    QPointF position(x, y);

    // 创建文本形状
    DrawingText *shape = new DrawingText(text);
    shape->setPos(position);

    // 解析字体属性
    QString fontFamily = element.attribute("font-family", "Arial");
    qreal fontSize = element.attribute("font-size", "12").toDouble();
    QString fontWeight = element.attribute("font-weight", "normal");
    QString fontStyle = element.attribute("font-style", "normal");

    QFont font(fontFamily);
    font.setPointSizeF(fontSize);

    if (fontWeight == "bold")
    {
        font.setBold(true);
    }

    if (fontStyle == "italic")
    {
        font.setItalic(true);
    }

    shape->setFont(font);

    // 解析文本锚点属性
    QString textAnchor = element.attribute("text-anchor", "start");
    
    // 如果是中间对齐，需要调整位置
    if (textAnchor == "middle") {
        QFontMetricsF metrics(font);
        qreal textWidth = metrics.horizontalAdvance(text);
        QPointF adjustedPos = position - QPointF(textWidth / 2, 0);
        shape->setPos(adjustedPos);
    } else if (textAnchor == "end") {
        QFontMetricsF metrics(font);
        qreal textWidth = metrics.horizontalAdvance(text);
        QPointF adjustedPos = position - QPointF(textWidth, 0);
        shape->setPos(adjustedPos);
    }

    // 解析样式属性
    parseStyleAttributes(shape, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        QTransform transformMatrix = parseTransform(transform);
        shape->applyTransform(transformMatrix);
    }
    return shape;
}

DrawingText *SvgHandler::parseTextElementWithTspan(const QDomElement &element)
{
    // 对于包含tspan的文本，我们将所有文本合并，但使用父元素的样式
    // 这是一个折中方案，确保所有文本都能显示
    
    // 获取位置
    qreal x = element.attribute("x", "0").toDouble();
    qreal y = element.attribute("y", "0").toDouble();
    QPointF position(x, y);

    // 解析字体属性
    QString fontFamily = element.attribute("font-family", "Arial");
    qreal fontSize = element.attribute("font-size", "12").toDouble();
    QString fontWeight = element.attribute("font-weight", "normal");
    QString fontStyle = element.attribute("font-style", "normal");

    QFont font(fontFamily);
    font.setPointSizeF(fontSize);

    if (fontWeight == "bold")
    {
        font.setBold(true);
    }

    if (fontStyle == "italic")
    {
        font.setItalic(true);
    }

    // 收集所有文本内容
    QString allText = collectTextContent(element).trimmed();
    if (allText.isEmpty()) {
        return nullptr;
    }

    DrawingText *shape = new DrawingText(allText);
    shape->setPos(position);
    shape->setFont(font);

    // 解析文本锚点属性
    QString textAnchor = element.attribute("text-anchor", "start");
    
    // 如果是中间对齐，需要调整位置
    if (textAnchor == "middle") {
        QFontMetricsF metrics(font);
        qreal textWidth = metrics.horizontalAdvance(allText);
        QPointF adjustedPos = position - QPointF(textWidth / 2, 0);
        shape->setPos(adjustedPos);
    } else if (textAnchor == "end") {
        QFontMetricsF metrics(font);
        qreal textWidth = metrics.horizontalAdvance(allText);
        QPointF adjustedPos = position - QPointF(textWidth, 0);
        shape->setPos(adjustedPos);
    }

    // 使用父元素的样式属性（不包括tspan的特殊样式）
    parseStyleAttributes(shape, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        QTransform transformMatrix = parseTransform(transform);
        shape->applyTransform(transformMatrix);
    }
    
    return shape;
}

// 递归收集文本内容，包括tspan元素
QString SvgHandler::collectTextContent(const QDomElement &element)
{
    QString content;
    
    // 获取当前元素的直接文本内容
    QDomNode child = element.firstChild();
    while (!child.isNull()) {
        if (child.isText()) {
            content += child.toText().data();
        } else if (child.isElement()) {
            QDomElement childElement = child.toElement();
            QString tagName = childElement.tagName().toLower();
            
            // 递归处理tspan和其他文本相关元素
            if (tagName == "tspan" || tagName == "text" || tagName == "tref") {
                content += collectTextContent(childElement);
            }
        }
        child = child.nextSibling();
    }
    
    return content;
}

void SvgHandler::parseStyleAttributes(DrawingShape *shape, const QDomElement &element)
{
    // 解析stroke属性
    QString stroke = element.attribute("stroke");
    if (!stroke.isEmpty())
    {
        if (stroke == "none")
        {
            // 显式设置无边框
            shape->setStrokePen(Qt::NoPen);
        }
        else
        {
            QColor strokeColor = parseColor(stroke);
            if (strokeColor.isValid())
            {
                QPen pen = shape->strokePen();
                pen.setColor(strokeColor);
                shape->setStrokePen(pen);
            }
        }
    }

    // 解析stroke-width属性
    QString strokeWidth = element.attribute("stroke-width");
    if (!strokeWidth.isEmpty())
    {
        qreal width = parseLength(strokeWidth);
        if (width > 0)
        {
            QPen pen = shape->strokePen();
            pen.setWidthF(width);
            shape->setStrokePen(pen);
        }
    }

    // 解析fill属性
    QString fill = element.attribute("fill");
    if (!fill.isEmpty())
    {
        if (fill == "none")
        {
            // 显式设置无填充
            shape->setFillBrush(Qt::NoBrush);
        }
        else if (fill.startsWith("url(#"))
        {
            QString refId = fill.mid(5, fill.length() - 6); // 去掉 "url(#" 和 ")"

            // 首先检查是否是渐变
            if (s_gradients.contains(refId))
            {
                QGradient gradient = s_gradients[refId];

                // 设置渐变坐标模式为对象边界框模式
                gradient.setCoordinateMode(QGradient::ObjectBoundingMode);

                // 创建画刷
                QBrush brush(gradient);
                shape->setFillBrush(brush);

                // 调试信息
                // //qDebug() << "应用渐变填充:" << refId;
                // //qDebug() << "渐变类型:" << gradient.type();
                // //qDebug() << "渐变停止点数量:" << gradient.stops().count();
                // //qDebug() << "坐标模式: ObjectBoundingMode";

                // 检查画刷是否有效
                if (brush.style() == Qt::LinearGradientPattern ||
                    brush.style() == Qt::RadialGradientPattern ||
                    brush.style() == Qt::ConicalGradientPattern)
                {
                    // //qDebug() << "画刷样式有效:" << brush.style();
                }
                else
                {
                    // //qDebug() << "警告: 画刷样式无效:" << brush.style();
                }
            }
            // 然后检查是否是Pattern
            else if (s_patterns.contains(refId))
            {
                QBrush patternBrush = s_patterns[refId];
                shape->setFillBrush(patternBrush);
                // 应用Pattern填充
            }
        }
        else
        {
            QColor fillColor = parseColor(fill);
            if (fillColor.isValid())
            {
                shape->setFillBrush(QBrush(fillColor));
            }
        }
    }

    // 解析opacity属性
    QString opacity = element.attribute("opacity");
    if (!opacity.isEmpty())
    {
        qreal opacityValue = opacity.toDouble();
        // 应用到画笔和画刷
        QPen pen = shape->strokePen();
        QColor strokeColor = pen.color();
        strokeColor.setAlphaF(opacityValue);
        pen.setColor(strokeColor);
        shape->setStrokePen(pen);

        QBrush brush = shape->fillBrush();
        QColor fillColor = brush.color();
        fillColor.setAlphaF(opacityValue);
        brush.setColor(fillColor);
        shape->setFillBrush(brush);
    }

    // 解析filter属性
    QString filter = element.attribute("filter");
    if (!filter.isEmpty() && filter.startsWith("url(#"))
    {
        QString filterId = filter.mid(5, filter.length() - 6); // 去掉 "url(#" 和 ")"
        
        applyFilterToShape(shape, filterId);
    }
}

void SvgHandler::parseStyleAttributes(DrawingGroup *group, const QDomElement &element)
{
    // DrawingGroup 本身不需要解析样式，样式应该传递给子元素
    // 这里可以解析组的透明度等属性
    QString opacity = element.attribute("opacity");
    if (!opacity.isEmpty())
    {
        qreal opacityValue = opacity.toDouble();
        // 设置组的透明度
        group->setOpacity(opacityValue);
    }

    // 解析filter属性
    QString filter = element.attribute("filter");
    if (!filter.isEmpty() && filter.startsWith("url(#"))
    {
        QString filterId = filter.mid(5, filter.length() - 6); // 去掉 "url(#" 和 ")"
        applyFilterToShape(group, filterId);
    }
}

qreal SvgHandler::parseLength(const QString &lengthStr)
{
    // 手写长度解析器，避免正则表达式开销
    return SvgLengthParser::parseLength(lengthStr);
}

QColor SvgHandler::parseColor(const QString &colorStr)
{
    // 使用手写的颜色解析器，避免正则表达式
    return SvgColorParser::parseColor(colorStr);
}

QTransform SvgHandler::parseTransform(const QString &transformStr)
{
    // 使用手写的变换解析器，避免正则表达式
    SvgTransformParser parser;
    return parser.parse(transformStr);
}

void SvgHandler::parseTransformAttribute(DrawingShape *shape, const QString &transformStr)
{
    // 如果变换字符串为空，直接返回
    if (transformStr.isEmpty()) {
        return;
    }
    
    // 使用符合SVG标准的变换处理（与use元素一致）
    QPointF pos = shape->pos();
    QString adjustedTransform = adjustTransformForUseElement(transformStr, -pos.x(), -pos.y());
    
    // 检查调整后的变换是否为空
    if (adjustedTransform.isEmpty()) {
        return;
    }
    
    QTransform transformMatrix = parseTransform(adjustedTransform);
    
    if (!transformMatrix.isIdentity())
    {
        shape->applyTransform(transformMatrix);
    }
}

void SvgHandler::parseSvgPointsData(const QString &pointsStr, QPainterPath &path, bool closePath)
{
    // 使用手写的点解析器，避免正则表达式
    SvgPointParser::parsePoints(pointsStr, path);
    
    // 只有在需要时才闭合路径
    if (closePath) {
        path.closeSubpath();
    }
}

bool SvgHandler::exportToSvg(DrawingScene *scene, const QString &fileName)
{
    QDomDocument doc = exportSceneToSvgDocument(scene);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        //qDebug() << "无法创建SVG文件:" << fileName;
        return false;
    }

    file.write(doc.toByteArray(2)); // 2表示缩进为2个空格
    file.close();

    return true;
}

QDomDocument SvgHandler::exportSceneToSvgDocument(DrawingScene *scene)
{
    QDomDocument doc;

    // 创建SVG根元素
    QDomElement svgElement = doc.createElement("svg");
    svgElement.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    svgElement.setAttribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
    svgElement.setAttribute("version", "1.1");

    // 计算场景中所有内容的边界框
    QRectF contentBounds;
    QList<QGraphicsItem *> allItems = scene->items();
    bool firstItem = true;

    // 收集所有图层和形状
    QList<DrawingLayer *> layers;
    QList<DrawingShape *> shapes;

    for (QGraphicsItem *item : allItems)
    {
        // 跳过选择指示器等辅助元素
        if (item->type() == QGraphicsItem::UserType + 100)
        {
            // DrawingLayer

            // DrawingLayer *layer = static_cast<DrawingLayer*>(item);
            // if (layer) {
            //     layers.append(layer);
            //     // 使用 sceneBoundingRect 获取场景中的实际边界
            //     QRectF layerBounds = layer->sceneBoundingRect();
            //     if (firstItem) {
            //         contentBounds = layerBounds;
            //         firstItem = false;
            //     } else {
            //         contentBounds = contentBounds.united(layerBounds);
            //     }
            // }
        }
        else
        {
            DrawingShape *shape = qgraphicsitem_cast<DrawingShape *>(item);
            if (shape)
            {
                shapes.append(shape);
                // 使用 sceneBoundingRect 获取场景中的实际边界
                QRectF shapeBounds = shape->sceneBoundingRect();
                if (firstItem)
                {
                    contentBounds = shapeBounds;
                    firstItem = false;
                }
                else
                {
                    contentBounds = contentBounds.united(shapeBounds);
                }
            }
        }
    }

    // 如果有内容，使用内容边界框；否则使用场景默认大小
    if (!contentBounds.isEmpty())
    {
        // 添加一些边距
        contentBounds.adjust(-20, -20, 20, 20);
    }
    else
    {
        contentBounds = QRectF(0, 0, 800, 600);
    }

    // 设置 viewBox 从 (0,0) 开始，宽高为内容的实际尺寸
    svgElement.setAttribute("viewBox", QString("0 0 %1 %2")
                                           .arg(contentBounds.width())
                                           .arg(contentBounds.height()));

    // 设置 SVG 的实际尺寸
    svgElement.setAttribute("width", QString::number(contentBounds.width()));
    svgElement.setAttribute("height", QString::number(contentBounds.height()));

    // 添加一个 transform 来移动内容到正确的位置
    if (contentBounds.left() != 0 || contentBounds.top() != 0)
    {
        svgElement.setAttribute("transform", QString("translate(%1,%2)")
                                                 .arg(-contentBounds.left())
                                                 .arg(-contentBounds.top()));
    }

    // 创建defs元素用于定义渐变和滤镜
    QDomElement defsElement = doc.createElement("defs");
    svgElement.appendChild(defsElement);

    // 导出渐变定义
    exportGradientsToSvg(doc, defsElement, allItems);

    // 导出滤镜定义
    exportFiltersToSvg(doc, defsElement, allItems);

    // 创建一个组元素来包含所有内容，并应用必要的变换
    QDomElement groupElement = doc.createElement("g");

    // 如果内容不在原点，添加变换来移动内容
    if (contentBounds.left() != 0 || contentBounds.top() != 0)
    {
        groupElement.setAttribute("transform", QString("translate(%1,%2)")
                                                   .arg(-contentBounds.left())
                                                   .arg(-contentBounds.top()));
    }

    // 首先导出图层（保持层次结构）
    for (DrawingLayer *layer : layers)
    {
        QDomElement layerElement = exportLayerToSvgElement(doc, layer);
        if (!layerElement.isNull())
        {
            groupElement.appendChild(layerElement);
        }
    }

    // 然后导出不在图层中的独立形状
    for (DrawingShape *shape : shapes)
    {
        // 检查形状是否在某个图层中
        bool inLayer = false;
        for (DrawingLayer *layer : layers)
        {
            if (layer->shapes().contains(shape))
            {
                inLayer = true;
                break;
            }
        }

        if (!inLayer)
        {
            QDomElement shapeElement = exportShapeToSvgElement(doc, shape);
            if (!shapeElement.isNull())
            {
                groupElement.appendChild(shapeElement);
            }
        }
    }

    svgElement.appendChild(groupElement);
    doc.appendChild(svgElement);
    return doc;
}

QDomElement SvgHandler::exportShapeToSvgElement(QDomDocument &doc, DrawingShape *shape)
{
    if (!shape)
    {
        return QDomElement();
    }

    switch (shape->shapeType())
    {
    case DrawingShape::Path:
        return exportPathToSvgElement(doc, static_cast<DrawingPath *>(shape));
    case DrawingShape::Rectangle:
        return exportRectangleToSvgElement(doc, static_cast<DrawingRectangle *>(shape));
    case DrawingShape::Ellipse:
        return exportEllipseToSvgElement(doc, static_cast<DrawingEllipse *>(shape));
    case DrawingShape::Text:
        return exportTextToSvgElement(doc, static_cast<DrawingText *>(shape));
    case DrawingShape::Line:
        return exportLineToSvgElement(doc, static_cast<DrawingLine *>(shape));
    case DrawingShape::Polyline:
        return exportPolylineToSvgElement(doc, static_cast<DrawingPolyline *>(shape));
    case DrawingShape::Polygon:
        return exportPolygonToSvgElement(doc, static_cast<DrawingPolygon *>(shape));
    default:
        //qDebug() << "未知的图形类型，无法导出:" << shape->shapeType();
        return QDomElement();
    }
}

QDomElement SvgHandler::exportPathToSvgElement(QDomDocument &doc, DrawingPath *path)
{
    QDomElement pathElement = doc.createElement("path");

    // 导出路径数据
    QString pathData = pathDataToString(path->path());
    pathElement.setAttribute("d", pathData);

    // 导出变换
    QTransform transform = path->transform();
    if (!transform.isIdentity())
    {
        pathElement.setAttribute("transform", transformToString(transform));
    }

    // 导出样式
    QPen pen = path->strokePen();
    QBrush brush = path->fillBrush();

    if (pen.style() != Qt::NoPen)
    {
        pathElement.setAttribute("stroke", pen.color().name());
        pathElement.setAttribute("stroke-width", QString::number(pen.widthF()));
        if (pen.color().alphaF() < 1.0)
        {
            pathElement.setAttribute("stroke-opacity", QString::number(pen.color().alphaF()));
        }
        // 导出线条样式
        if (pen.style() == Qt::DashLine)
        {
            pathElement.setAttribute("stroke-dasharray", "5,5");
        }
        else if (pen.style() == Qt::DotLine)
        {
            pathElement.setAttribute("stroke-dasharray", "2,2");
        }
    }

    if (brush.style() != Qt::NoBrush)
    {
        if (brush.style() == Qt::LinearGradientPattern || brush.style() == Qt::RadialGradientPattern)
        {
            pathElement.setAttribute("fill", QString("url(#grad_%1)").arg(quintptr(brush.gradient())));
        }
        else
        {
            pathElement.setAttribute("fill", brush.color().name());
            if (brush.color().alphaF() < 1.0)
            {
                pathElement.setAttribute("fill-opacity", QString::number(brush.color().alphaF()));
            }
        }
    }
    else
    {
        pathElement.setAttribute("fill", "none");
    }

    // 导出滤镜
    QGraphicsEffect *effect = path->graphicsEffect();
    if (effect)
    {
        if (qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            pathElement.setAttribute("filter", QString("url(#blur_0)"));
        }
        else if (qobject_cast<QGraphicsDropShadowEffect *>(effect))
        {
            pathElement.setAttribute("filter", QString("url(#shadow_0)"));
        }
    }

    return pathElement;
}

QDomElement SvgHandler::exportRectangleToSvgElement(QDomDocument &doc, DrawingRectangle *rect)
{
    QDomElement rectElement = doc.createElement("rect");

    // 获取图形的位置和本地边界
    QPointF pos = rect->pos();
    QRectF bounds = rect->localBounds();

    // 计算实际位置（位置 + 本地边界）
    rectElement.setAttribute("x", QString::number(pos.x() + bounds.x()));
    rectElement.setAttribute("y", QString::number(pos.y() + bounds.y()));
    rectElement.setAttribute("width", QString::number(bounds.width()));
    rectElement.setAttribute("height", QString::number(bounds.height()));

    // 导出圆角
    if (rect->cornerRadius() > 0)
    {
        rectElement.setAttribute("rx", QString::number(rect->cornerRadius()));
        rectElement.setAttribute("ry", QString::number(rect->cornerRadius()));
    }

    // 导出变换
    QTransform transform = rect->transform();
    if (!transform.isIdentity())
    {
        rectElement.setAttribute("transform", transformToString(transform));
    }

    // 导出样式
    QPen pen = rect->strokePen();
    QBrush brush = rect->fillBrush();

    if (pen.style() != Qt::NoPen)
    {
        rectElement.setAttribute("stroke", pen.color().name());
        rectElement.setAttribute("stroke-width", QString::number(pen.widthF()));
        if (pen.color().alphaF() < 1.0)
        {
            rectElement.setAttribute("stroke-opacity", QString::number(pen.color().alphaF()));
        }
    }

    if (brush.style() != Qt::NoBrush)
    {
        if (brush.style() == Qt::LinearGradientPattern || brush.style() == Qt::RadialGradientPattern)
        {
            // 引用渐变
            rectElement.setAttribute("fill", QString("url(#grad_%1)").arg(quintptr(brush.gradient())));
        }
        else
        {
            rectElement.setAttribute("fill", brush.color().name());
            if (brush.color().alphaF() < 1.0)
            {
                rectElement.setAttribute("fill-opacity", QString::number(brush.color().alphaF()));
            }
        }
    }
    else
    {
        rectElement.setAttribute("fill", "none");
    }

    // 导出滤镜
    QGraphicsEffect *effect = rect->graphicsEffect();
    if (effect)
    {
        if (qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            rectElement.setAttribute("filter", QString("url(#blur_0)"));
        }
        else if (qobject_cast<QGraphicsDropShadowEffect *>(effect))
        {
            rectElement.setAttribute("filter", QString("url(#shadow_0)"));
        }
    }

    return rectElement;
}

QDomElement SvgHandler::exportEllipseToSvgElement(QDomDocument &doc, DrawingEllipse *ellipse)
{
    QDomElement ellipseElement;

    // 获取图形的位置和本地边界
    QPointF pos = ellipse->pos();
    QRectF bounds = ellipse->localBounds();

    // 检查是否是完整的椭圆（360度或接近360度）
    qreal startAngle = ellipse->startAngle();
    qreal spanAngle = ellipse->spanAngle();

    // 如果跨度接近360度（允许一些误差），则认为是完整椭圆
    if (qFuzzyCompare(qAbs(spanAngle), 360.0) || qFuzzyCompare(spanAngle, 0.0) || qAbs(spanAngle) > 350)
    {
        // 完整椭圆
        ellipseElement = doc.createElement("ellipse");
        qreal cx = pos.x() + bounds.x() + bounds.width() / 2;
        qreal cy = pos.y() + bounds.y() + bounds.height() / 2;
        qreal rx = bounds.width() / 2;
        qreal ry = bounds.height() / 2;

        ellipseElement.setAttribute("cx", QString::number(cx));
        ellipseElement.setAttribute("cy", QString::number(cy));
        ellipseElement.setAttribute("rx", QString::number(rx));
        ellipseElement.setAttribute("ry", QString::number(ry));
    }
    else
    {
        // 椭圆弧
        ellipseElement = doc.createElement("path");

        // 使用椭圆弧路径
        qreal cx = pos.x() + bounds.x() + bounds.width() / 2;
        qreal cy = pos.y() + bounds.y() + bounds.height() / 2;
        qreal rx = bounds.width() / 2;
        qreal ry = bounds.height() / 2;

        QString pathData = QString("M %1,%2 A %3,%4 0 %5,1 %6,%7")
                               .arg(cx - rx)
                               .arg(cy) // 起点
                               .arg(rx)
                               .arg(ry)                      // 半径
                               .arg(spanAngle > 180 ? 1 : 0) // large-arc-flag
                               .arg(spanAngle > 0 ? 1 : 0)   // sweep-flag
                               .arg(cx + rx)
                               .arg(cy); // 终点

        ellipseElement.setAttribute("d", pathData);
    }

    // 导出变换
    QTransform transform = ellipse->transform();
    if (!transform.isIdentity())
    {
        ellipseElement.setAttribute("transform", transformToString(transform));
    }

    // 导出样式
    QPen pen = ellipse->strokePen();
    QBrush brush = ellipse->fillBrush();

    if (pen.style() != Qt::NoPen)
    {
        ellipseElement.setAttribute("stroke", pen.color().name());
        ellipseElement.setAttribute("stroke-width", QString::number(pen.widthF()));
        if (pen.color().alphaF() < 1.0)
        {
            ellipseElement.setAttribute("stroke-opacity", QString::number(pen.color().alphaF()));
        }
    }

    if (brush.style() != Qt::NoBrush)
    {
        if (brush.style() == Qt::LinearGradientPattern || brush.style() == Qt::RadialGradientPattern)
        {
            ellipseElement.setAttribute("fill", QString("url(#radial_%1)").arg(quintptr(brush.gradient())));
        }
        else
        {
            ellipseElement.setAttribute("fill", brush.color().name());
            if (brush.color().alphaF() < 1.0)
            {
                ellipseElement.setAttribute("fill-opacity", QString::number(brush.color().alphaF()));
            }
        }
    }
    else
    {
        ellipseElement.setAttribute("fill", "none");
    }

    // 导出滤镜
    QGraphicsEffect *effect = ellipse->graphicsEffect();
    if (effect)
    {
        if (qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            ellipseElement.setAttribute("filter", QString("url(#blur_0)"));
        }
        else if (qobject_cast<QGraphicsDropShadowEffect *>(effect))
        {
            ellipseElement.setAttribute("filter", QString("url(#shadow_0)"));
        }
    }

    return ellipseElement;
}

QString SvgHandler::pathDataToString(const QPainterPath &path)
{
    QString result;

    for (int i = 0; i < path.elementCount(); ++i)
    {
        QPainterPath::Element element = path.elementAt(i);

        switch (element.type)
        {
        case QPainterPath::MoveToElement:
            result += QString("M %1,%2 ").arg(element.x).arg(element.y);
            break;
        case QPainterPath::LineToElement:
            result += QString("L %1,%2 ").arg(element.x).arg(element.y);
            break;
        case QPainterPath::CurveToElement:
            // 需要获取控制点和终点
            if (i + 2 < path.elementCount())
            {
                QPainterPath::Element ctrl1 = path.elementAt(i);
                QPainterPath::Element ctrl2 = path.elementAt(i + 1);
                QPainterPath::Element end = path.elementAt(i + 2);

                result += QString("C %1,%2 %3,%4 %5,%6 ")
                              .arg(ctrl1.x)
                              .arg(ctrl1.y)
                              .arg(ctrl2.x)
                              .arg(ctrl2.y)
                              .arg(end.x)
                              .arg(end.y);

                i += 2; // 跳过已处理的元素
            }
            break;
        case QPainterPath::CurveToDataElement:
            // 控制点数据，在CurveToElement中处理
            break;
        }
    }

    return result.trimmed();
}

// 渐变解析方法
void SvgHandler::parseDefsElements(const QDomElement &root)
{
    // 查找defs元素
    QDomNodeList defsNodes = root.elementsByTagName("defs");
    if (defsNodes.isEmpty())
    {
        // 静默返回，不显示警告信息，因为简单的SVG文件可能不需要defs元素
        return;
    }

    // 清理之前的渐变定义
    s_gradients.clear();

    // 处理所有的defs元素
    for (int defsIndex = 0; defsIndex < defsNodes.size(); ++defsIndex)
    {
        QDomElement defs = defsNodes.at(defsIndex).toElement();
        // //qDebug() << "解析defs元素" << defsIndex;

        // 解析所有线性渐变
        QDomNodeList linearGradients = defs.elementsByTagName("linearGradient");
        for (int i = 0; i < linearGradients.size(); ++i)
        {
            QDomElement element = linearGradients.at(i).toElement();
            QString id = element.attribute("id");
            if (!id.isEmpty())
            {
                QLinearGradient gradient = parseLinearGradient(element);
                s_gradients[id] = gradient;
                // //qDebug() << "解析线性渐变:" << id;
            }
        }

        // 解析所有径向渐变
        QDomNodeList radialGradients = defs.elementsByTagName("radialGradient");
        for (int i = 0; i < radialGradients.size(); ++i)
        {
            QDomElement element = radialGradients.at(i).toElement();
            QString id = element.attribute("id");
            if (!id.isEmpty())
            {
                QRadialGradient gradient = parseRadialGradient(element);
                s_gradients[id] = gradient;
                // //qDebug() << "解析径向渐变:" << id;
            }
        }

        // 解析所有Marker
        QDomNodeList markers = defs.elementsByTagName("marker");
        for (int i = 0; i < markers.size(); ++i)
        {
            QDomElement markerElement = markers.at(i).toElement();
            QString id = markerElement.attribute("id");
            if (!id.isEmpty())
            {
                s_markers[id] = markerElement;
                // 预解析Marker数据
                s_markerDataCache[id] = parseMarkerData(markerElement);
                // 预渲染Marker到缓存
                renderMarkerToCache(id, markerElement);
                // //qDebug() << "解析Marker:" << id;
            }
        }
    }
}

QLinearGradient SvgHandler::parseLinearGradient(const QDomElement &element)
{
    // 解析起点和终点
    QString x1Str = element.attribute("x1", "0%");
    QString y1Str = element.attribute("y1", "0%");
    QString x2Str = element.attribute("x2", "100%");
    QString y2Str = element.attribute("y2", "0%");

    qreal x1, y1, x2, y2;

    // 检查是否使用百分比
    if (x1Str.endsWith("%"))
    {
        x1 = x1Str.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        x1 = x1Str.toDouble();
    }

    if (y1Str.endsWith("%"))
    {
        y1 = y1Str.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        y1 = y1Str.toDouble();
    }

    if (x2Str.endsWith("%"))
    {
        x2 = x2Str.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        x2 = x2Str.toDouble();
    }

    if (y2Str.endsWith("%"))
    {
        y2 = y2Str.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        y2 = y2Str.toDouble();
    }

    QLinearGradient gradient(QPointF(x1, y1), QPointF(x2, y2));

    // 解析渐变停止点
    parseGradientStops(&gradient, element);

    return gradient;
}

QRadialGradient SvgHandler::parseRadialGradient(const QDomElement &element)
{
    // 解析中心点、半径和焦点
    QString cxStr = element.attribute("cx", "50%");
    QString cyStr = element.attribute("cy", "50%");
    QString rStr = element.attribute("r", "50%");
    QString fxStr = element.attribute("fx", cxStr); // 默认使用cx值
    QString fyStr = element.attribute("fy", cyStr); // 默认使用cy值

    qreal cx, cy, r, fx, fy;

    // 检查是否使用百分比
    if (cxStr.endsWith("%"))
    {
        cx = cxStr.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        cx = cxStr.toDouble();
    }

    if (cyStr.endsWith("%"))
    {
        cy = cyStr.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        cy = cyStr.toDouble();
    }

    if (rStr.endsWith("%"))
    {
        r = rStr.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        r = rStr.toDouble();
    }

    if (fxStr.endsWith("%"))
    {
        fx = fxStr.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        fx = fxStr.toDouble();
    }

    if (fyStr.endsWith("%"))
    {
        fy = fyStr.replace("%", "").toDouble() / 100.0;
    }
    else
    {
        fy = fyStr.toDouble();
    }

    // 对于径向渐变，在ObjectBoundingMode下，坐标和半径是相对于对象边界框的
    // cx, cy, fx, fy 应该是 0-1 范围内的相对坐标
    // r 应该是相对于边界框最大尺寸的相对半径
    QRadialGradient gradient(QPointF(cx, cy), r, QPointF(fx, fy));

    // 解析渐变停止点
    parseGradientStops(&gradient, element);

    return gradient;
}

void SvgHandler::parseGradientStops(QGradient *gradient, const QDomElement &element)
{
    QDomNodeList stops = element.elementsByTagName("stop");
    // //qDebug() << "解析" << stops.size() << "个停止点";

    for (int i = 0; i < stops.size(); ++i)
    {
        QDomElement stop = stops.at(i).toElement();

        // 解析偏移位置 - 处理百分比
        QString offsetStr = stop.attribute("offset", "0");
        qreal offset;
        if (offsetStr.endsWith("%"))
        {
            offset = offsetStr.replace("%", "").toDouble() / 100.0;
        }
        else
        {
            offset = offsetStr.toDouble();
        }
        // //qDebug() << "停止点" << i << " offset=" << offsetStr << " -> " << offset;

        // 解析停止颜色 - 首先检查style属性
        QString stopColor;
        QString stopOpacity;

        QString style = stop.attribute("style");
        if (!style.isEmpty())
        {
            // 解析CSS样式属性
            QStringList styleProps = style.split(";", Qt::SkipEmptyParts);
            for (const QString &prop : styleProps)
            {
                QStringList keyValue = prop.split(":", Qt::SkipEmptyParts);
                if (keyValue.size() == 2)
                {
                    QString key = keyValue[0].trimmed();
                    QString value = keyValue[1].trimmed();

                    if (key == "stop-color")
                    {
                        stopColor = value;
                    }
                    else if (key == "stop-opacity")
                    {
                        stopOpacity = value;
                    }
                }
            }
        }

        // 如果style中没有找到，则使用直接属性
        if (stopColor.isEmpty())
        {
            stopColor = stop.attribute("stop-color", "black");
        }
        if (stopOpacity.isEmpty())
        {
            stopOpacity = stop.attribute("stop-opacity", "1");
        }

        QColor color = parseColor(stopColor);
        // //qDebug() << "停止点" << i << " stop-color=" << stopColor << " -> " << color.name();

        // 解析透明度
        qreal opacity = stopOpacity.toDouble();
        color.setAlphaF(opacity);
        // //qDebug() << "停止点" << i << " stop-opacity=" << stopOpacity << " -> 透明度=" << color.alphaF();

        gradient->setColorAt(offset, color);
    }
}

// 滤镜解析方法
void SvgHandler::parseFilterElements(const QDomElement &root)
{
    // 查找defs元素
    QDomNodeList defsNodes = root.elementsByTagName("defs");
    if (defsNodes.isEmpty())
    {
        // 静默返回，不显示警告信息，因为简单的SVG文件可能不需要滤镜
        return;
    }

    QDomElement defs = defsNodes.at(0).toElement();
    // //qDebug() << "解析滤镜元素";

    // 清理之前的滤镜定义
    for (auto it = s_filters.begin(); it != s_filters.end(); ++it)
    {
        delete it.value();
    }
    s_filters.clear();
    // //qDebug() << "清理了" << s_filters.size() << "个滤镜定义";

    // 解析所有滤镜
    QDomNodeList filters = defs.elementsByTagName("filter");
    for (int i = 0; i < filters.size(); ++i)
    {
        QDomElement filterElement = filters.at(i).toElement();
        QString id = filterElement.attribute("id");
        if (!id.isEmpty())
        {
            // 查找滤镜内部的子元素
            QDomNodeList filterPrimitives = filterElement.childNodes();
            QGraphicsEffect *lastEffect = nullptr; // 存储最后一个效果，Qt会自动叠加
            
            for (int j = 0; j < filterPrimitives.size(); ++j)
            {
                QDomNode node = filterPrimitives.at(j);
                if (node.isElement())
                {
                    QDomElement primitive = node.toElement();
                    QString tagName = primitive.tagName();

                    QGraphicsEffect *effect = nullptr;

                    if (tagName == "feGaussianBlur")
                    {
                        effect = parseGaussianBlurFilter(primitive);
                    }
                    else if (tagName == "feDropShadow")
                    {
                        effect = parseDropShadowFilter(primitive);
                    }

                    if (effect)
                    {
                        lastEffect = effect; // 保存最后一个效果
                        // //qDebug() << "解析滤镜primitive:" << id << "类型:" << tagName;
                    }
                }
            }
            
            // 使用最后一个效果（Qt会自动处理叠加）
            if (lastEffect)
            {
                s_filters[id] = lastEffect;
            }
        }
    }
}

QGraphicsBlurEffect *SvgHandler::parseGaussianBlurFilter(const QDomElement &element)
{
    QGraphicsBlurEffect *blur = new QGraphicsBlurEffect();

    // 解析stdDeviation属性
    QString stdDeviation = element.attribute("stdDeviation", "1.0");
    qreal radius = stdDeviation.toDouble();
    
    // Qt的模糊效果可能需要更大的半径才能看到明显效果
    // 增强模糊效果，使半径更加明显
    if (radius > 0) {
        radius = qMax(radius * 2.0, 2.0); // 至少2.0，或者原值的2倍
    }
    
    blur->setBlurRadius(radius);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);

    //qDebug() << "解析高斯模糊滤镜 - 原始半径:" << stdDeviation.toDouble() << "调整后半径:" << radius;
    return blur;
}

QGraphicsDropShadowEffect *SvgHandler::parseDropShadowFilter(const QDomElement &element)
{
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();

    // 解析阴影属性（简化版本）
    qreal blurRadius = element.attribute("stdDeviation", "3.0").toDouble();
    qreal offsetX = element.attribute("dx", "2.0").toDouble();
    qreal offsetY = element.attribute("dy", "2.0").toDouble();

    shadow->setBlurRadius(blurRadius);
    shadow->setOffset(QPointF(offsetX, offsetY));
    shadow->setColor(QColor(63, 63, 63, 180)); // 默认半透明黑色

    // //qDebug() << "阴影滤镜 - 模糊半径:" << blurRadius << "偏移:" << offsetX << "," << offsetY;
    return shadow;
}

void SvgHandler::applyFilterToShape(DrawingShape *shape, const QString &filterId)
{
    if (!shape || filterId.isEmpty())
    {
        return;
    }

    //qDebug() << "应用滤镜到图形:" << filterId;
    
    if (s_filters.contains(filterId))
    {
        QGraphicsEffect *effect = s_filters[filterId];
        if (!effect)
        {
            //qDebug() << "滤镜效果为空:" << filterId;
            return;
        }

        //qDebug() << "找到滤镜效果:" << filterId;
        
        // 注意：Qt中一个effect不能被多个item共享，需要克隆
        if (auto blurEffect = qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            QGraphicsBlurEffect *newBlur = new QGraphicsBlurEffect();
            if (newBlur)
            {
                newBlur->setBlurRadius(blurEffect->blurRadius());
                newBlur->setBlurHints(QGraphicsBlurEffect::QualityHint);
                shape->setGraphicsEffect(newBlur);
                // //qDebug() << "应用模糊滤镜到图形:" << filterId;
            }
        }
        else if (auto shadowEffect = qobject_cast<QGraphicsDropShadowEffect *>(effect))
        {
            QGraphicsDropShadowEffect *newShadow = new QGraphicsDropShadowEffect();
            if (newShadow)
            {
                newShadow->setBlurRadius(shadowEffect->blurRadius());
                newShadow->setOffset(shadowEffect->offset());
                newShadow->setColor(shadowEffect->color());
                shape->setGraphicsEffect(newShadow);
                // //qDebug() << "应用阴影滤镜到图形:" << filterId;
            }
        }
    }
    else
    {

    }
}

void SvgHandler::applyFilterToShape(DrawingGroup *group, const QString &filterId)
{
    if (!group || filterId.isEmpty())
    {
        return;
    }

    if (s_filters.contains(filterId))
    {
        QGraphicsEffect *effect = s_filters[filterId];
        if (!effect)
        {
            // //qDebug() << "滤镜效果为空:" << filterId;
            return;
        }

        // 注意：Qt中一个effect不能被多个item共享，需要克隆
        if (auto blurEffect = qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            QGraphicsBlurEffect *newBlur = new QGraphicsBlurEffect();
            if (newBlur)
            {
                newBlur->setBlurRadius(blurEffect->blurRadius());
                newBlur->setBlurHints(QGraphicsBlurEffect::QualityHint);
                group->setGraphicsEffect(newBlur);
                // //qDebug() << "应用高斯模糊滤镜到组合:" << filterId << "半径:" << blurEffect->blurRadius();
            }
        }
        else if (auto shadowEffect = qobject_cast<QGraphicsDropShadowEffect *>(effect))
        {
            QGraphicsDropShadowEffect *newShadow = new QGraphicsDropShadowEffect();
            if (newShadow)
            {
                newShadow->setBlurRadius(shadowEffect->blurRadius());
                newShadow->setOffset(shadowEffect->offset());
                newShadow->setColor(shadowEffect->color());
                group->setGraphicsEffect(newShadow);
                // //qDebug() << "应用阴影滤镜到组合:" << filterId;
            }
        }
    }
    else
    {
        // //qDebug() << "未找到滤镜:" << filterId;
    }
}

// Pattern解析方法
void SvgHandler::parsePatternElements(const QDomElement &root)
{
    // 查找defs元素
    QDomNodeList defsNodes = root.elementsByTagName("defs");
    if (defsNodes.isEmpty())
    {
        // 静默返回，不显示警告信息，因为简单的SVG文件可能不需要Pattern
        return;
    }

    QDomElement defs = defsNodes.at(0).toElement();
    // //qDebug() << "解析Pattern元素";

    // 清理之前的Pattern定义
    s_patterns.clear();

    // 解析所有Pattern
    QDomNodeList patterns = defs.elementsByTagName("pattern");
    for (int i = 0; i < patterns.size(); ++i)
    {
        QDomElement patternElement = patterns.at(i).toElement();
        QString id = patternElement.attribute("id");
        if (!id.isEmpty())
        {
            // 解析Pattern内容
            QBrush patternBrush = parsePatternBrush(patternElement);
            s_patterns[id] = patternBrush;
            // //qDebug() << "解析Pattern:" << id;
        }
    }
}

QBrush SvgHandler::parsePatternBrush(const QDomElement &patternElement)
{
    // 获取Pattern属性
    QString id = patternElement.attribute("id");
    qreal width = parseLength(patternElement.attribute("width", "10"));
    qreal height = parseLength(patternElement.attribute("height", "10"));
    QString patternUnits = patternElement.attribute("patternUnits", "objectBoundingBox");
    QString patternContentUnits = patternElement.attribute("patternContentUnits", "userSpaceOnUse");

    // 创建Pattern图像
    QPixmap patternPixmap(static_cast<int>(width), static_cast<int>(height));
    patternPixmap.fill(Qt::transparent);

    QPainter painter(&patternPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 解析Pattern内容
    QDomNodeList children = patternElement.childNodes();
    for (int i = 0; i < children.size(); ++i)
    {
        QDomNode node = children.at(i);
        if (node.isElement())
        {
            QDomElement childElement = node.toElement();
            QString tagName = childElement.tagName();

            if (tagName == "rect")
            {
                qreal x = parseLength(childElement.attribute("x", "0"));
                qreal y = parseLength(childElement.attribute("y", "0"));
                qreal w = parseLength(childElement.attribute("width", "0"));
                qreal h = parseLength(childElement.attribute("height", "0"));

                QColor fillColor = parseColor(childElement.attribute("fill", "black"));
                QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
                qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                painter.setBrush(QBrush(fillColor));
                painter.setPen(QPen(strokeColor, strokeWidth));
                painter.drawRect(x, y, w, h);
            }
            else if (tagName == "circle")
            {
                qreal cx = parseLength(childElement.attribute("cx", "0"));
                qreal cy = parseLength(childElement.attribute("cy", "0"));
                qreal r = parseLength(childElement.attribute("r", "0"));

                QColor fillColor = parseColor(childElement.attribute("fill", "black"));
                QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
                qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                painter.setBrush(QBrush(fillColor));
                painter.setPen(QPen(strokeColor, strokeWidth));
                painter.drawEllipse(QPointF(cx, cy), r, r);
            }
            else if (tagName == "path")
            {
                QString d = childElement.attribute("d");
                if (!d.isEmpty())
                {
                    QPainterPath path;
                    parseSvgPathData(d, path);

                    QColor fillColor = parseColor(childElement.attribute("fill", "black"));
                    QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
                    qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                    painter.setBrush(QBrush(fillColor));
                    painter.setPen(QPen(strokeColor, strokeWidth));
                    painter.drawPath(path);
                }
            }
            else if (tagName == "line")
            {
                qreal x1 = parseLength(childElement.attribute("x1", "0"));
                qreal y1 = parseLength(childElement.attribute("y1", "0"));
                qreal x2 = parseLength(childElement.attribute("x2", "0"));
                qreal y2 = parseLength(childElement.attribute("y2", "0"));

                QColor strokeColor = parseColor(childElement.attribute("stroke", "black"));
                qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                painter.setPen(QPen(strokeColor, strokeWidth));
                painter.drawLine(x1, y1, x2, y2);
            }
        }
    }

    painter.end();

    // 创建Pattern画刷
    QBrush patternBrush(patternPixmap);

    // 设置Pattern变换
    QTransform patternTransform;
    if (patternUnits == "objectBoundingBox")
    {
        // 对于objectBoundingBox，Pattern会自动缩放以适应对象
        patternTransform.scale(1.0 / width, 1.0 / height);
    }

    patternBrush.setTransform(patternTransform);
    return patternBrush;
}

QBrush SvgHandler::parsePatternBrush(const QString &patternId)
{
    // 兼容旧接口
    if (s_patterns.contains(patternId))
    {
        return s_patterns[patternId];
    }

    // 返回默认Pattern
    QPixmap patternPixmap(20, 20);
    patternPixmap.fill(Qt::transparent);

    QPainter painter(&patternPixmap);
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(Qt::NoBrush);

    // 绘制简单的点阵图案
    for (int x = 0; x < 20; x += 4)
    {
        for (int y = 0; y < 20; y += 4)
        {
            painter.drawPoint(x, y);
        }
    }

    QBrush patternBrush(patternPixmap);
    return patternBrush;
}

// Marker解析方法


// 解析Marker数据
MarkerData SvgHandler::parseMarkerData(const QDomElement &markerElement)
{
    MarkerData data;
    
    // 解析marker的参考点
    QString refXStr = markerElement.attribute("refX", "0");
    QString refYStr = markerElement.attribute("refY", "0");
    QString markerWidthStr = markerElement.attribute("markerWidth", "3");
    QString markerHeightStr = markerElement.attribute("markerHeight", "3");
    QString orientStr = markerElement.attribute("orient", "auto");
    
    data.refX = parseLength(refXStr);
    data.refY = parseLength(refYStr);
    data.markerWidth = parseLength(markerWidthStr);
    data.markerHeight = parseLength(markerHeightStr);
    data.orient = orientStr;
    
    // 解析Marker内容
    QDomNodeList children = markerElement.childNodes();
    for (int i = 0; i < children.size(); ++i)
    {
        QDomNode node = children.at(i);
        if (node.isElement())
        {
            QDomElement childElement = node.toElement();
            QString tagName = childElement.tagName();

            // 获取样式
            QColor fillColor = parseColor(childElement.attribute("fill", "black"));
            QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
            qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

            if (tagName == "path")
            {
                QString d = childElement.attribute("d");
                if (!d.isEmpty())
                {
                    QPainterPath path;
                    parseSvgPathData(d, path);
                    
                    QVariantList params;
                    params << QVariant::fromValue(path);
                    
                    data = MarkerData(MarkerData::Path, params, fillColor, strokeColor, strokeWidth);
                    break; // 只处理第一个子元素
                }
            }
            else if (tagName == "circle")
            {
                qreal cx = parseLength(childElement.attribute("cx", "0"));
                qreal cy = parseLength(childElement.attribute("cy", "0"));
                qreal r = parseLength(childElement.attribute("r", "0"));
                
                QVariantList params;
                params << cx << cy << r;
                
                data = MarkerData(MarkerData::Circle, params, fillColor, strokeColor, strokeWidth);
                break;
            }
            else if (tagName == "rect")
            {
                qreal x = parseLength(childElement.attribute("x", "0"));
                qreal y = parseLength(childElement.attribute("y", "0"));
                qreal width = parseLength(childElement.attribute("width", "0"));
                qreal height = parseLength(childElement.attribute("height", "0"));
                
                QVariantList params;
                params << x << y << width << height;
                
                data = MarkerData(MarkerData::Rect, params, fillColor, strokeColor, strokeWidth);
                break;
            }
            else if (tagName == "polygon")
            {
                QString points = childElement.attribute("points");
                if (!points.isEmpty())
                {
                    QPainterPath path;
                    parseSvgPointsData(points, path, false); // 不自动闭合
                    
                    QVariantList params;
                    params << QVariant::fromValue(path);
                    
                    data = MarkerData(MarkerData::Polygon, params, fillColor, strokeColor, strokeWidth);
                    break;
                }
            }
        }
    }
    
    return data;
}

// 渲染Marker到缓存
void SvgHandler::renderMarkerToCache(const QString &id, const QDomElement &markerElement)
{
    // 获取Marker属性
    qreal markerWidth = parseLength(markerElement.attribute("markerWidth", "10"));
    qreal markerHeight = parseLength(markerElement.attribute("markerHeight", "10"));
    qreal refX = parseLength(markerElement.attribute("refX", "0"));
    qreal refY = parseLength(markerElement.attribute("refY", "0"));
    QString orient = markerElement.attribute("orient", "auto");

    // 创建缓存图像
    QPixmap pixmap(static_cast<int>(markerWidth), static_cast<int>(markerHeight));
    pixmap.fill(Qt::transparent); // 使用透明背景

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 解析Marker内容
    QDomNodeList children = markerElement.childNodes();
    for (int i = 0; i < children.size(); ++i)
    {
        QDomNode node = children.at(i);
        if (node.isElement())
        {
            QDomElement childElement = node.toElement();
            QString tagName = childElement.tagName();

            if (tagName == "path")
            {
                QString d = childElement.attribute("d");
                if (!d.isEmpty())
                {
                    QPainterPath path;
                    parseSvgPathData(d, path);

                    // 获取样式
                    QColor fillColor = parseColor(childElement.attribute("fill", "black"));
                    QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
                    qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                    // 应用样式
                    painter.setBrush(QBrush(fillColor));
                    painter.setPen(QPen(strokeColor, strokeWidth));

                    // 绘制路径
                    painter.drawPath(path);
                }
            }
            else if (tagName == "circle")
            {
                qreal cx = parseLength(childElement.attribute("cx", "0"));
                qreal cy = parseLength(childElement.attribute("cy", "0"));
                qreal r = parseLength(childElement.attribute("r", "0"));

                QColor fillColor = parseColor(childElement.attribute("fill", "black"));
                QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
                qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                painter.setBrush(QBrush(fillColor));
                painter.setPen(QPen(strokeColor, strokeWidth));
                painter.drawEllipse(QPointF(cx, cy), r, r);
            }
            else if (tagName == "rect")
            {
                qreal x = parseLength(childElement.attribute("x", "0"));
                qreal y = parseLength(childElement.attribute("y", "0"));
                qreal width = parseLength(childElement.attribute("width", "0"));
                qreal height = parseLength(childElement.attribute("height", "0"));

                QColor fillColor = parseColor(childElement.attribute("fill", "black"));
                QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
                qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                painter.setBrush(QBrush(fillColor));
                painter.setPen(QPen(strokeColor, strokeWidth));
                painter.drawRect(x, y, width, height);
            }
            else if (tagName == "polygon")
            {
                QString points = childElement.attribute("points");
                if (!points.isEmpty())
                {
                    QPainterPath path;
                    // 对于marker中的polygon，不自动闭合路径，让SVG属性决定
                    parseSvgPointsData(points, path, false);

                    QColor fillColor = parseColor(childElement.attribute("fill", "black"));
                    QColor strokeColor = parseColor(childElement.attribute("stroke", "none"));
                    qreal strokeWidth = parseLength(childElement.attribute("stroke-width", "1"));

                    painter.setBrush(QBrush(fillColor));
                    painter.setPen(QPen(strokeColor, strokeWidth));
                    painter.drawPath(path);
                }
            }
        }
    }

    painter.end();
    // s_markerCache已弃用，使用s_markerDataCache
}

// 创建Marker路径
QPainterPath SvgHandler::createMarkerPath(const QString &markerId, const QPointF &startPoint, const QPointF &endPoint)
{
    QPainterPath markerPath;

    if (!s_markers.contains(markerId) || !s_markerDataCache.contains(markerId))
    {
        return markerPath;
    }

    QDomElement markerElement = s_markers[markerId];
    // QPixmap markerPixmap已弃用，使用s_markerDataCache

    // 获取Marker属性
    qreal markerWidth = parseLength(markerElement.attribute("markerWidth", "10"));
    qreal markerHeight = parseLength(markerElement.attribute("markerHeight", "10"));
    qreal refX = parseLength(markerElement.attribute("refX", "0"));
    qreal refY = parseLength(markerElement.attribute("refY", "0"));
    QString orient = markerElement.attribute("orient", "auto");

    // 计算方向
    qreal angle = 0;
    if (orient == "auto")
    {
        // 计算线段方向
        qreal dx = endPoint.x() - startPoint.x();
        qreal dy = endPoint.y() - startPoint.y();
        angle = qAtan2(dy, dx) * 180.0 / M_PI;
    }

    // 计算Marker位置
    QPointF markerPos = endPoint - QPointF(refX, refY);

    // 创建变换矩阵
    QTransform transform;
    transform.translate(markerPos.x(), markerPos.y());
    transform.rotate(angle);

    // 将Marker添加到路径
    markerPath.addRect(markerPos.x(), markerPos.y(), markerWidth, markerHeight);

    return markerPath;
}

// 应用所有类型的marker
void SvgHandler::applyMarkers(DrawingPath *path, const QString &markerStart, const QString &markerMid, const QString &markerEnd)
{
    if (!path)
    {
        return;
    }

    // 应用start marker
    if (!markerStart.isEmpty())
    {
        QString markerId = SvgUrlParser::parseUrlReference(markerStart);
        if (!markerId.isEmpty())
        {
            applyMarkerToPath(path, markerId, "start");
        }
    }

    // 应用end marker
    if (!markerEnd.isEmpty())
    {
        QString markerId = SvgUrlParser::parseUrlReference(markerEnd);
        if (!markerId.isEmpty())
        {
            applyMarkerToPath(path, markerId, "end");
        }
    }

    // 应用mid markers
    if (!markerMid.isEmpty())
    {
        QString markerId = SvgUrlParser::parseUrlReference(markerMid);
        if (!markerId.isEmpty())
        {
            
            // 获取路径的所有中间点
            QPainterPath painterPath = path->path();
            if (painterPath.elementCount() >= 3)
            {
                // 为每个中间点创建marker
                for (int i = 1; i < painterPath.elementCount() - 1; i++)
                {
                    QPointF point = painterPath.elementAt(i);
                    
                    // 计算该点的方向
                    QPointF prevPoint = painterPath.elementAt(i - 1);
                    QPointF nextPoint = painterPath.elementAt(i + 1);
                    
                    // 计算进入方向
                    qreal dx1 = point.x() - prevPoint.x();
                    qreal dy1 = point.y() - prevPoint.y();
                    qreal angle1 = qAtan2(dy1, dx1);
                    
                    // 计算离开方向
                    qreal dx2 = nextPoint.x() - point.x();
                    qreal dy2 = nextPoint.y() - point.y();
                    qreal angle2 = qAtan2(dy2, dx2);
                    
                    // 使用平均角度
                    qreal avgAngle = (angle1 + angle2) / 2.0;
                    
                    // 获取marker数据
                    if (s_markerDataCache.contains(markerId))
                    {
                        MarkerData markerData = s_markerDataCache[markerId];
                        
                        // 创建Marker变换
                        QTransform transform;
                        transform.translate(point.x(), point.y());
                        transform.rotate(avgAngle * 180.0 / M_PI);
                        if (markerData.isValid)
                        {
                            // 为每个中间点设置唯一的marker ID
                            QString uniqueMarkerId = QString("%1_mid_%2").arg(markerId).arg(i);
                            path->setMarker(uniqueMarkerId, markerData, transform, "mid");
                        }
                    }
                }
            }
            else
            {
                // 对于简单的路径，使用原来的逻辑
                applyMarkerToPath(path, markerId, "mid");
            }
        }
    }
}

void SvgHandler::applyMarkerToPath(DrawingPath *path, const QString &markerId, const QString &position)
{
    if (!path || markerId.isEmpty())
    {
        return;
    }

    if (s_markerDataCache.contains(markerId))
    {
        MarkerData markerData = s_markerDataCache[markerId];
        
        if (!markerData.isValid)
        {
            return;
        }

        // 获取路径的起点和终点
        QPainterPath painterPath = path->path();
        if (painterPath.elementCount() < 2)
        {
            return;
        }

        // 获取起点和终点
        QPointF startPoint = painterPath.elementAt(0);
        QPointF endPoint = painterPath.elementAt(painterPath.elementCount() - 1);

        // 根据marker位置确定应用点和方向
        QPointF markerPoint;
        qreal angle = 0;

        if (position == "start")
        {
            markerPoint = startPoint;
            // 计算从起点到下一个点的方向
            if (painterPath.elementCount() > 1)
            {
                QPointF nextPoint = painterPath.elementAt(1);
                qreal dx = nextPoint.x() - startPoint.x();
                qreal dy = nextPoint.y() - startPoint.y();
                angle = qAtan2(dy, dx) * 180.0 / M_PI;
            }
        }
        else if (position == "mid")
        {
            // 对于polyline，在每个中间线段上都应用marker
            // SVG规范中，marker-mid应该应用到除起点和终点外的所有顶点
            QList<QPointF> midPoints;
            QList<qreal> midAngles;
            
            if (painterPath.elementCount() >= 3)
            {
                // 收集所有中间点（排除第一个和最后一个）
                for (int i = 1; i < painterPath.elementCount() - 1; i++)
                {
                    QPointF point = painterPath.elementAt(i);
                    
                    // 计算该点的方向（基于前后线段的平均方向）
                    QPointF prevPoint = painterPath.elementAt(i - 1);
                    QPointF nextPoint = painterPath.elementAt(i + 1);
                    
                    // 计算进入方向
                    qreal dx1 = point.x() - prevPoint.x();
                    qreal dy1 = point.y() - prevPoint.y();
                    qreal angle1 = qAtan2(dy1, dx1);
                    
                    // 计算离开方向
                    qreal dx2 = nextPoint.x() - point.x();
                    qreal dy2 = nextPoint.y() - point.y();
                    qreal angle2 = qAtan2(dy2, dx2);
                    
                    // 使用平均角度
                    qreal avgAngle = (angle1 + angle2) / 2.0;
                    
                    midPoints.append(point);
                    midAngles.append(avgAngle * 180.0 / M_PI);
                }
                
                // 如果有中间点，使用第一个中间点
                if (!midPoints.isEmpty())
                {
                    markerPoint = midPoints.first();
                    angle = midAngles.first();
                }
            }
            else if (painterPath.elementCount() >= 2)
            {
                // 对于简单的路径，在起点和终点之间放置
                markerPoint = QPointF((startPoint.x() + endPoint.x()) / 2.0,
                                   (startPoint.y() + endPoint.y()) / 2.0);
                
                QPointF prevPoint = painterPath.elementAt(painterPath.elementCount() - 2);
                qreal dx = endPoint.x() - prevPoint.x();
                qreal dy = endPoint.y() - prevPoint.y();
                angle = qAtan2(dy, dx) * 180.0 / M_PI;
            }
        }
        else
        { // "end" (默认)
            markerPoint = endPoint;
            // 计算从倒数第二个点到终点的方向
            if (painterPath.elementCount() > 1)
            {
                QPointF prevPoint = painterPath.elementAt(painterPath.elementCount() - 2);
                qreal dx = endPoint.x() - prevPoint.x();
                qreal dy = endPoint.y() - prevPoint.y();
                angle = qAtan2(dy, dx) * 180.0 / M_PI;
            }
        }

        // 创建Marker变换
        QTransform transform;
        transform.translate(markerPoint.x(), markerPoint.y());
        transform.rotate(angle);

        // 存储Marker信息用于渲染
        path->setMarker(markerId, markerData, transform, position);
    }
}

// 导出辅助函数实现
QDomElement SvgHandler::exportLayerToSvgElement(QDomDocument &doc, DrawingLayer *layer)
{
    QDomElement gElement = doc.createElement("g");

    // 设置图层属性
    if (!layer->name().isEmpty())
    {
        gElement.setAttribute("id", layer->name());
    }

    if (layer->opacity() < 1.0)
    {
        gElement.setAttribute("opacity", QString::number(layer->opacity()));
    }

    if (!layer->isVisible())
    {
        gElement.setAttribute("visibility", "hidden");
    }

    // 导出图层变换
    if (!layer->layerTransform().isIdentity())
    {
        gElement.setAttribute("transform", transformToString(layer->layerTransform()));
    }

    // 导出图层中的所有形状
    for (DrawingShape *shape : layer->shapes())
    {
        if (shape)
        {
            QDomElement shapeElement = exportShapeToSvgElement(doc, shape);
            if (!shapeElement.isNull())
            {
                gElement.appendChild(shapeElement);
            }
        }
    }

    return gElement;
}

void SvgHandler::exportGradientsToSvg(QDomDocument &doc, QDomElement &defsElement, const QList<QGraphicsItem *> &items)
{
    QSet<QString> exportedGradients;

    // 收集所有使用的渐变
    for (QGraphicsItem *item : items)
    {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape *>(item);
        if (shape)
        {
            QBrush brush = shape->fillBrush();
            if (brush.style() == Qt::LinearGradientPattern)
            {
                const QLinearGradient *gradient = static_cast<const QLinearGradient *>(brush.gradient());
                if (gradient && !exportedGradients.contains(QString::number(quintptr(gradient))))
                {
                    QDomElement gradElement = doc.createElement("linearGradient");
                    gradElement.setAttribute("id", QString("grad_%1").arg(exportedGradients.size()));
                    gradElement.setAttribute("x1", QString::number(gradient->start().x()));
                    gradElement.setAttribute("y1", QString::number(gradient->start().y()));
                    gradElement.setAttribute("x2", QString::number(gradient->finalStop().x()));
                    gradElement.setAttribute("y2", QString::number(gradient->finalStop().y()));

                    // 导出停止点
                    for (const QGradientStop &stop : gradient->stops())
                    {
                        QDomElement stopElement = doc.createElement("stop");
                        stopElement.setAttribute("offset", QString::number(stop.first));
                        stopElement.setAttribute("stop-color", stop.second.name());
                        if (stop.second.alphaF() < 1.0)
                        {
                            stopElement.setAttribute("stop-opacity", QString::number(stop.second.alphaF()));
                        }
                        gradElement.appendChild(stopElement);
                    }

                    defsElement.appendChild(gradElement);
                    exportedGradients.insert(QString::number(quintptr(gradient)));
                }
            }
            else if (brush.style() == Qt::RadialGradientPattern)
            {
                const QRadialGradient *gradient = static_cast<const QRadialGradient *>(brush.gradient());
                if (gradient && !exportedGradients.contains(QString::number(quintptr(gradient))))
                {
                    QDomElement gradElement = doc.createElement("radialGradient");
                    gradElement.setAttribute("id", QString("radial_%1").arg(exportedGradients.size()));
                    gradElement.setAttribute("cx", QString::number(gradient->center().x()));
                    gradElement.setAttribute("cy", QString::number(gradient->center().y()));
                    gradElement.setAttribute("r", QString::number(gradient->radius()));
                    gradElement.setAttribute("fx", QString::number(gradient->focalPoint().x()));
                    gradElement.setAttribute("fy", QString::number(gradient->focalPoint().y()));

                    // 导出停止点
                    for (const QGradientStop &stop : gradient->stops())
                    {
                        QDomElement stopElement = doc.createElement("stop");
                        stopElement.setAttribute("offset", QString::number(stop.first));
                        stopElement.setAttribute("stop-color", stop.second.name());
                        if (stop.second.alphaF() < 1.0)
                        {
                            stopElement.setAttribute("stop-opacity", QString::number(stop.second.alphaF()));
                        }
                        gradElement.appendChild(stopElement);
                    }

                    defsElement.appendChild(gradElement);
                    exportedGradients.insert(QString::number(quintptr(gradient)));
                }
            }
        }
    }
}

void SvgHandler::exportFiltersToSvg(QDomDocument &doc, QDomElement &defsElement, const QList<QGraphicsItem *> &items)
{
    QSet<QString> exportedFilters;

    // 收集所有使用的滤镜
    for (QGraphicsItem *item : items)
    {
        DrawingShape *shape = qgraphicsitem_cast<DrawingShape *>(item);
        if (shape)
        {
            QGraphicsEffect *effect = shape->graphicsEffect();
            if (effect)
            {
                if (auto blurEffect = qobject_cast<QGraphicsBlurEffect *>(effect))
                {
                    QString filterId = QString("blur_%1").arg(exportedFilters.size());
                    if (!exportedFilters.contains(filterId))
                    {
                        QDomElement filterElement = doc.createElement("filter");
                        filterElement.setAttribute("id", filterId);
                        filterElement.setAttribute("x", "-50%");
                        filterElement.setAttribute("y", "-50%");
                        filterElement.setAttribute("width", "200%");
                        filterElement.setAttribute("height", "200%");

                        QDomElement blurElement = doc.createElement("feGaussianBlur");
                        blurElement.setAttribute("stdDeviation", QString::number(blurEffect->blurRadius()));

                        filterElement.appendChild(blurElement);
                        defsElement.appendChild(filterElement);
                        exportedFilters.insert(filterId);
                    }
                }
                else if (auto shadowEffect = qobject_cast<QGraphicsDropShadowEffect *>(effect))
                {
                    QString filterId = QString("shadow_%1").arg(exportedFilters.size());
                    if (!exportedFilters.contains(filterId))
                    {
                        QDomElement filterElement = doc.createElement("filter");
                        filterElement.setAttribute("id", filterId);
                        filterElement.setAttribute("x", "-50%");
                        filterElement.setAttribute("y", "-50%");
                        filterElement.setAttribute("width", "200%");
                        filterElement.setAttribute("height", "200%");

                        QDomElement shadowElement = doc.createElement("feDropShadow");
                        shadowElement.setAttribute("dx", QString::number(shadowEffect->offset().x()));
                        shadowElement.setAttribute("dy", QString::number(shadowEffect->offset().y()));
                        shadowElement.setAttribute("stdDeviation", QString::number(shadowEffect->blurRadius()));
                        shadowElement.setAttribute("flood-color", shadowEffect->color().name());

                        filterElement.appendChild(shadowElement);
                        defsElement.appendChild(filterElement);
                        exportedFilters.insert(filterId);
                    }
                }
            }
        }
    }
}

QString SvgHandler::transformToString(const QTransform &transform)
{
    if (transform.isIdentity())
    {
        return QString();
    }

    // 简化实现：只处理基本的变换
    QStringList transforms;

    // 检查平移
    if (!qFuzzyIsNull(transform.dx()) || !qFuzzyIsNull(transform.dy()))
    {
        transforms.append(QString("translate(%1,%2)").arg(transform.dx()).arg(transform.dy()));
    }

    // 检查旋转
    qreal angle = 0;
    if (!qFuzzyIsNull(transform.m12()) || !qFuzzyIsNull(transform.m21()))
    {
        // 简单的旋转检测
        angle = qAsin(transform.m21()) * 180.0 / M_PI;
        transforms.append(QString("rotate(%1)").arg(angle));
    }

    // 检查缩放
    if (!qFuzzyIsNull(transform.m11() - 1) || !qFuzzyIsNull(transform.m22() - 1))
    {
        transforms.append(QString("scale(%1,%2)").arg(transform.m11()).arg(transform.m22()));
    }

    return transforms.join(" ");
}

QDomElement SvgHandler::exportTextToSvgElement(QDomDocument &doc, DrawingText *text)
{
    QDomElement textElement = doc.createElement("text");

    // 设置文本内容
    textElement.appendChild(doc.createTextNode(text->text()));

    // 设置位置
    QPointF pos = text->position();
    textElement.setAttribute("x", QString::number(pos.x()));
    textElement.setAttribute("y", QString::number(pos.y()));

    // 导出字体属性
    QFont font = text->font();
    if (!font.family().isEmpty())
    {
        textElement.setAttribute("font-family", font.family());
    }
    if (font.pointSizeF() > 0)
    {
        textElement.setAttribute("font-size", QString::number(font.pointSizeF()));
    }
    if (font.bold())
    {
        textElement.setAttribute("font-weight", "bold");
    }
    if (font.italic())
    {
        textElement.setAttribute("font-style", "italic");
    }

    // 导出变换
    QTransform transform = text->transform();
    if (!transform.isIdentity())
    {
        textElement.setAttribute("transform", transformToString(transform));
    }

    // 导出样式
    QPen pen = text->strokePen();
    QBrush brush = text->fillBrush();

    if (pen.style() != Qt::NoPen)
    {
        textElement.setAttribute("stroke", pen.color().name());
        textElement.setAttribute("stroke-width", QString::number(pen.widthF()));
        if (pen.color().alphaF() < 1.0)
        {
            textElement.setAttribute("stroke-opacity", QString::number(pen.color().alphaF()));
        }
    }

    if (brush.style() != Qt::NoBrush)
    {
        textElement.setAttribute("fill", brush.color().name());
        if (brush.color().alphaF() < 1.0)
        {
            textElement.setAttribute("fill-opacity", QString::number(brush.color().alphaF()));
        }
    }
    else
    {
        textElement.setAttribute("fill", "black"); // 默认文本颜色
    }

    // 导出滤镜
    QGraphicsEffect *effect = text->graphicsEffect();
    if (effect)
    {
        if (qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            textElement.setAttribute("filter", QString("url(#blur_0)"));
        }
        else if (qobject_cast<QGraphicsDropShadowEffect *>(effect))
        {
            textElement.setAttribute("filter", QString("url(#shadow_0)"));
        }
    }

    return textElement;
}

QDomElement SvgHandler::exportLineToSvgElement(QDomDocument &doc, DrawingLine *line)
{
    if (!line)
    {
        return QDomElement();
    }

    QDomElement lineElement = doc.createElement("line");

    // 获取线条的位置和线条本身
    QPointF pos = line->pos();
    QLineF l = line->line();

    // 计算实际位置（位置 + 线条坐标）
    lineElement.setAttribute("x1", QString::number(pos.x() + l.x1()));
    lineElement.setAttribute("y1", QString::number(pos.y() + l.y1()));
    lineElement.setAttribute("x2", QString::number(pos.x() + l.x2()));
    lineElement.setAttribute("y2", QString::number(pos.y() + l.y2()));

    // 导出样式
    if (line->strokePen() != Qt::NoPen)
    {
        lineElement.setAttribute("stroke", line->strokePen().color().name());
        lineElement.setAttribute("stroke-width", QString::number(line->strokePen().widthF()));
    }

    if (line->fillBrush() != Qt::NoBrush)
    {
        lineElement.setAttribute("fill", line->fillBrush().color().name());
    }
    else
    {
        lineElement.setAttribute("fill", "none");
    }

    return lineElement;
}

QDomElement SvgHandler::exportPolylineToSvgElement(QDomDocument &doc, DrawingPolyline *polyline)
{
    if (!polyline)
    {
        return QDomElement();
    }

    QDomElement polylineElement = doc.createElement("polyline");

    // 构建点字符串
    QString pointsStr;
    QPointF pos = polyline->pos();
    QVector<QPointF> points = polyline->getNodePoints();
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF &p = points[i];
        // 计算实际位置（位置 + 点坐标）
        pointsStr += QString("%1,%2").arg(pos.x() + p.x()).arg(pos.y() + p.y());
        if (i < points.size() - 1)
        {
            pointsStr += " ";
        }
    }
    polylineElement.setAttribute("points", pointsStr);

    // 导出样式
    if (polyline->strokePen() != Qt::NoPen)
    {
        polylineElement.setAttribute("stroke", polyline->strokePen().color().name());
        polylineElement.setAttribute("stroke-width", QString::number(polyline->strokePen().widthF()));
    }

    if (polyline->fillBrush() != Qt::NoBrush)
    {
        polylineElement.setAttribute("fill", polyline->fillBrush().color().name());
    }
    else
    {
        polylineElement.setAttribute("fill", "none");
    }

    return polylineElement;
}

QDomElement SvgHandler::exportPolygonToSvgElement(QDomDocument &doc, DrawingPolygon *polygon)
{
    if (!polygon)
    {
        return QDomElement();
    }

    QDomElement polygonElement = doc.createElement("polygon");

    // 构建点字符串
    QString pointsStr;
    QPointF pos = polygon->pos();
    QVector<QPointF> points = polygon->getNodePoints();
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF &p = points[i];
        // 计算实际位置（位置 + 点坐标）
        pointsStr += QString("%1,%2").arg(pos.x() + p.x()).arg(pos.y() + p.y());
        if (i < points.size() - 1)
        {
            pointsStr += " ";
        }
    }
    polygonElement.setAttribute("points", pointsStr);

    // 导出样式
    if (polygon->strokePen() != Qt::NoPen)
    {
        polygonElement.setAttribute("stroke", polygon->strokePen().color().name());
        polygonElement.setAttribute("stroke-width", QString::number(polygon->strokePen().widthF()));
    }

    if (polygon->fillBrush() != Qt::NoBrush)
    {
        polygonElement.setAttribute("fill", polygon->fillBrush().color().name());
    }
    else
    {
        polygonElement.setAttribute("fill", "none");
    }

    return polygonElement;
}

// 收集所有有id的元素（用于use元素）
void SvgHandler::collectDefinedElements(const QDomElement &parent)
{
    QDomNodeList children = parent.childNodes();
    for (int i = 0; i < children.size(); ++i)
    {
        QDomNode node = children.at(i);
        if (node.isElement())
        {
            QDomElement element = node.toElement();
            QString tagName = element.tagName();

            // 如果元素有id，存储它
            if (element.hasAttribute("id"))
            {
                QString id = element.attribute("id");
                QDomElement clonedElement = element.cloneNode().toElement();
                
                // 移除transform属性，避免在use元素中重复处理变换
                clonedElement.removeAttribute("transform");
                
                s_definedElements[id] = clonedElement;
                //qDebug() << "存储定义元素:" << id << "移除transform属性:" << !clonedElement.hasAttribute("transform");
            }

            // 递归处理子元素
            if (tagName == "defs" || tagName == "g")
            {
                collectDefinedElements(element);
            }
        }
    }
}

// 解析use元素
DrawingShape *SvgHandler::parseUseElement(const QDomElement &element)
{
    //qDebug() << "解析use元素";
    
    // 获取href属性（引用的元素ID）
    QString href = element.attribute("href");
    if (href.isEmpty())
    {
        href = element.attribute("xlink:href"); // 兼容旧版本SVG
    }

    if (href.isEmpty() || !href.startsWith("#"))
    {
        //qDebug() << "use元素href属性无效:" << href;
        return nullptr;
    }

    // 提取引用的ID
    QString refId = href.mid(1); // 去掉#
    //qDebug() << "use元素引用ID:" << refId;

    // 查找定义的元素
    if (!s_definedElements.contains(refId))
    {
        //qDebug() << "未找到引用的元素:" << refId;
        return nullptr;
    }

    QDomElement referencedElement = s_definedElements[refId];
    //qDebug() << "找到引用元素:" << referencedElement.tagName();
    //qDebug() << "引用元素是否有transform:" << referencedElement.hasAttribute("transform");
    if (referencedElement.hasAttribute("transform")) {
        //qDebug() << "引用元素transform:" << referencedElement.attribute("transform");
    }

    // 克隆并解析引用的元素
    DrawingShape *shape = parseSvgElement(referencedElement);
    if (!shape)
    {
        //qDebug() << "解析引用元素失败";
        return nullptr;
    }

    // 应用use元素的位置偏移
    qreal x = element.attribute("x", "0").toDouble();
    qreal y = element.attribute("y", "0").toDouble();

    // 解析并应用变换（必须先于位置设置）
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        // 调整变换中的旋转中心，因为SVG中的旋转中心是绝对坐标
        // 需要转换为相对于元素位置的坐标
        QString adjustedTransform = adjustTransformForUseElement(transform, -x, -y);
        QTransform transformMatrix = parseTransform(adjustedTransform);
        shape->applyTransform(transformMatrix);
    }

    // 应用use元素的位置偏移（在变换之后）
    if (x != 0 || y != 0)
    {
        shape->setPos(shape->pos() + QPointF(x, y));
    }

    // 解析样式属性：use元素的样式会覆盖引用元素的样式
    // 但如果use元素没有某个样式属性，应该继承引用元素的样式
    parseStyleAttributes(shape, element);
    
    // 如果use元素没有fill属性，继承引用元素的fill
    if (!element.hasAttribute("fill") && referencedElement.hasAttribute("fill"))
    {
        QString fill = referencedElement.attribute("fill");
        if (!fill.isEmpty())
        {
            if (fill == "none")
            {
                shape->setFillBrush(Qt::NoBrush);
            }
            else
            {
                QColor fillColor = parseColor(fill);
                if (fillColor.isValid())
                {
                    shape->setFillBrush(fillColor);
                }
            }
        }
    }
    
    // 如果use元素没有stroke属性，继承引用元素的stroke
    if (!element.hasAttribute("stroke") && referencedElement.hasAttribute("stroke"))
    {
        QString stroke = referencedElement.attribute("stroke");
        if (!stroke.isEmpty())
        {
            if (stroke == "none")
            {
                shape->setStrokePen(Qt::NoPen);
            }
            else
            {
                QColor strokeColor = parseColor(stroke);
                if (strokeColor.isValid())
                {
                    QPen pen = shape->strokePen();
                    pen.setColor(strokeColor);
                    shape->setStrokePen(pen);
                }
            }
        }
    }
    
    // 如果use元素没有stroke-width属性，继承引用元素的stroke-width
    if (!element.hasAttribute("stroke-width") && referencedElement.hasAttribute("stroke-width"))
    {
        QString strokeWidth = referencedElement.attribute("stroke-width");
        if (!strokeWidth.isEmpty())
        {
            qreal width = parseLength(strokeWidth);
            if (width > 0)
            {
                QPen pen = shape->strokePen();
                pen.setWidthF(width);
                shape->setStrokePen(pen);
            }
        }
    }

    return shape;
}

// 调整use元素的变换，考虑位置偏移
QString SvgHandler::adjustTransformForUseElement(const QString &transformStr, qreal x, qreal y)
{
    // 使用手写的use元素变换解析器，避免正则表达式
    return SvgUseTransformParser::adjustTransformForUseElement(transformStr, x, y);
}

// 解析SVG元数据
SvgMetadata SvgHandler::parseSvgMetadata(const QDomElement &svgElement)
{
    SvgMetadata metadata;
    
    // 解析width和height属性
    QString widthStr = svgElement.attribute("width");
    QString heightStr = svgElement.attribute("height");
    
    if (!widthStr.isEmpty() && !heightStr.isEmpty()) {
        // 解析长度值，支持单位
        metadata.size.setWidth(parseLength(widthStr));
        metadata.size.setHeight(parseLength(heightStr));
        metadata.hasSize = true;
    }
    
    // 解析viewBox属性
    QString viewBoxStr = svgElement.attribute("viewBox");
    if (!viewBoxStr.isEmpty()) {
        QStringList parts = SvgStringUtils::splitOnWhitespaceOrComma(viewBoxStr);
        if (parts.size() == 4) {
            qreal x = parts[0].toDouble();
            qreal y = parts[1].toDouble();
            qreal width = parts[2].toDouble();
            qreal height = parts[3].toDouble();
            metadata.viewBox = QRectF(x, y, width, height);
            metadata.hasViewBox = true;
        }
    }
    
    // 解析preserveAspectRatio属性
    metadata.preserveAspectRatio = svgElement.attribute("preserveAspectRatio", "xMidYMid meet");
    
    // 如果没有viewBox但有size，使用size作为viewBox
    if (!metadata.hasViewBox && metadata.hasSize) {
        metadata.viewBox = QRectF(0, 0, metadata.size.width(), metadata.size.height());
        metadata.hasViewBox = true;
    }
    
    // 如果既没有viewBox也没有size，使用默认值
    if (!metadata.hasViewBox) {
        metadata.viewBox = QRectF(0, 0, 1000, 800);
        metadata.size = QSizeF(1000, 800);
        metadata.hasViewBox = true;
        metadata.hasSize = true;
    }
    
    return metadata;
}

// 计算SVG到Scene的变换矩阵
QTransform SvgHandler::calculateSvgToSceneTransform(const SvgMetadata &metadata)
{
    QTransform transform;
    
    // 如果有viewBox，需要计算从viewBox到scene坐标的变换
    if (metadata.hasViewBox && metadata.hasSize) {
        QRectF viewBox = metadata.viewBox;
        QSizeF svgSize = metadata.size;
        
        // 根据preserveAspectRatio计算缩放
        qreal scaleX = svgSize.width() / viewBox.width();
        qreal scaleY = svgSize.height() / viewBox.height();
        
        // 解析preserveAspectRatio
        QString preserveAspect = metadata.preserveAspectRatio;
        QStringList aspectParts = SvgStringUtils::splitOnWhitespace(preserveAspect);
        
        QString align = "xMidYMid";  // 默认对齐方式
        QString meetOrSlice = "meet"; // 默认meet模式
        
        if (aspectParts.size() >= 1) {
            align = aspectParts[0];
        }
        if (aspectParts.size() >= 2) {
            meetOrSlice = aspectParts[1];
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
            scale = 1.0;
            scaleX = scale;
            scaleY = scale;
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
        transform.scale(scale, scale);
        transform.translate(-viewBox.left(), -viewBox.top());
    }
    
    return transform;
}

// 应用SVG设置到Scene
void SvgHandler::applySvgSettingsToScene(DrawingScene *scene, const SvgMetadata &metadata)
{
    if (!scene) {
        return;
    }
    
    // 设置scene的边界为viewBox
    scene->setSceneRect(metadata.viewBox);
    
    // 注意：网格大小是编辑器的属性，不应该被SVG导入改变
    // 保持用户当前的网格设置
    
    // 如果需要，可以在这里设置其他scene属性
    // 比如背景色等
}

