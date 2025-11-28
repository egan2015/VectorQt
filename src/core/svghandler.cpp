#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QPointF>
#include <QTransform>
#include <QDebug>
#include "svghandler.h"
#include "drawing-shape.h"
#include "drawing-layer.h"
#include "drawing-group.h"
#include "layer-manager.h"
#include "../ui/drawingscene.h"

// 渐变存储
static QHash<QString, QGradient> s_gradients;

// 滤镜存储
static QHash<QString, QGraphicsEffect *> s_filters;

// Pattern存储
static QHash<QString, QBrush> s_patterns;

// Marker存储
static QHash<QString, QDomElement> s_markers;

// Marker渲染缓存
static QHash<QString, QPixmap> s_markerCache;

// 定义的元素存储（用于use元素）
static QHash<QString, QDomElement> s_definedElements;

bool SvgHandler::importFromSvg(DrawingScene *scene, const QString &fileName)
{
    // qDebug() << "开始导入SVG文件:" << fileName;

    // 设置SVG导入标志，防止创建默认图层
    LayerManager::instance()->setSvgImporting(true);
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "无法打开SVG文件:" << fileName;
        LayerManager::instance()->setSvgImporting(false);
        return false;
    }

    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorCol;

    if (!doc.setContent(&file, &errorMsg, &errorLine, &errorCol))
    {
        qDebug() << "解析SVG文件失败:" << errorMsg << "行:" << errorLine << "列:" << errorCol;
        LayerManager::instance()->setSvgImporting(false);
        return false;
    }

    file.close();
    // qDebug() << "SVG文档解析成功，开始解析文档";

    bool result = parseSvgDocument(scene, doc);
    // qDebug() << "SVG导入完成，结果:" << result;
    return result;
}

bool SvgHandler::parseSvgDocument(DrawingScene *scene, const QDomDocument &doc)
{
    QDomElement root = doc.documentElement();
    if (root.tagName() != "svg")
    {
        qDebug() << "不是有效的SVG文档，标签名:" << root.tagName();
        return false;
    }

    // 清空之前存储的定义元素
    s_definedElements.clear();

    // 首先收集所有定义的元素（用于use元素）
    collectDefinedElements(root);

    // 解析defs元素中的渐变定义
    parseDefsElements(root);

    // 解析滤镜定义
    parseFilterElements(root);

    // 解析Pattern定义
    parsePatternElements(root);

    // 解析Marker定义
    parseMarkerElements(root);

    // 遍历SVG文档中的所有元素
    QDomNodeList children = root.childNodes();
    int elementCount = 0;
    for (int i = 0; i < children.size(); ++i)
    {
        QDomNode node = children.at(i);
        if (node.isElement())
        {
            QDomElement element = node.toElement();
            QString tagName = element.tagName();

            // 跳过defs元素，因为已经在parseDefsElements中处理了
            if (tagName == "defs")
            {
                continue;
            }

            // qDebug() << "解析SVG元素:" << tagName;

            if (tagName == "g")
            {
                // 处理组元素
                DrawingGroup *group = parseGroupElement(scene, element);
                if (group)
                {
                    elementCount++;
                }
            }
            else
            {
                DrawingShape *shape = parseSvgElement(element);
                if (shape)
                {
                    scene->addItem(shape);
                    elementCount++;
                    // qDebug() << "添加形状到场景，当前元素数:" << elementCount;
                }
                else
                {
                    // qDebug() << "无法解析元素:" << tagName;
                }
            }
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

    // qDebug() << "总共解析了" << elementCount << "个元素";
    return elementCount > 0;
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
            // qDebug() << "暂不支持的元素:" << tagName;
            return nullptr;
        }
        else if (tagName == "clipPath" || tagName == "mask")
        {
            // TODO: 实现裁剪路径和蒙版支持
            // qDebug() << "暂不支持的元素:" << tagName;
            return nullptr;
        }
        else
        {
            // 记录未知的元素类型，但不崩溃
            // qDebug() << "未知的SVG元素:" << tagName;
            return nullptr;
        }
    }
    catch (...)
    {
        // qDebug() << "解析元素时发生异常:" << tagName;
        return nullptr;
    }
}

DrawingGroup *SvgHandler::parseGroupElement(DrawingScene *scene, const QDomElement &groupElement)
{
    // 检查是否是图层（带有 inkscape:label 属性）
    QString layerId = groupElement.attribute("inkscape:label");
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

        // qDebug() << "创建图层:" << layerId << "可见:" << layer->isVisible() << "锁定:" << isLocked;
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
                // 递归处理嵌套组
                DrawingGroup *nestedGroup = parseGroupElement(scene, element);
                if (nestedGroup && group)
                {
                    // 将嵌套组添加到父组中
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
                                parseTransformAttribute(shape, transform);
                            }
                        }

                        if (layer)
                        {
                            // 添加到图层
                            layer->addShape(shape);
                            // qDebug() << "添加形状到图层" << layerId << "，元素:" << tagName;
                        }
                        else if (group)
                        {
                            // 先添加到场景以获得正确的场景坐标
                            scene->addItem(shape);
                            // 然后添加到组合对象（会自动转换为相对坐标）
                            group->addItem(shape);
                            // qDebug() << "添加形状到组合对象，元素:" << tagName;
                        }
                        else
                        {
                            // 直接添加到场景
                            scene->addItem(shape);
                            // qDebug() << "从组中添加形状到场景，元素:" << tagName;
                        }
                        // 元素已添加
                    }
                    else
                    {
                        // qDebug() << "无法解析元素:" << tagName << "，跳过";
                    }
                }
                catch (...)
                {
                    // qDebug() << "解析元素时发生错误:" << tagName << "，跳过";
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
            for (DrawingShape *shape : layer->shapes())
            {
                parseTransformAttribute(shape, transform);
            }
        }
    }

    // 应用变换到组合
    if (group && groupElement.hasAttribute("transform"))
    {
        QString transform = groupElement.attribute("transform");
        if (!transform.isEmpty())
        {
            parseTransformAttribute(group, transform);
        }
    }

    // 最后将组合添加到场景
    if (group)
    {
        scene->addItem(group);
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
        parseTransformAttribute(ellipse, element.attribute("transform"));
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
    // 优化后的路径解析器，使用状态机模式提高性能
    QVector<qreal> numbers;
    numbers.reserve(64); // 预分配空间，减少重新分配

    QPointF currentPoint(0, 0);
    QPointF pathStart(0, 0); // 记录子路径起点，用于Z命令
    QPointF lastControlPoint(0, 0);

    int i = 0;
    const int length = data.length();

    while (i < length)
    {
        // 跳过空白字符
        while (i < length && data[i].isSpace())
        {
            i++;
        }

        if (i >= length)
            break;

        QChar cmd = data[i];
        if (cmd.isLetter())
        {
            // 处理命令
            bool isRelative = cmd.isLower();
            cmd = cmd.toUpper();
            i++;

            // 解析数字参数
            numbers.clear();
            while (i < length)
            {
                // 跳过逗号和空白
                while (i < length && (data[i] == ',' || data[i].isSpace()))
                {
                    i++;
                }
                if (i >= length || data[i].isLetter())
                    break;

                // 解析数字（支持负号和小数点）
                int start = i;
                if (data[i] == '-' || data[i] == '+')
                    i++;
                while (i < length && (data[i].isDigit() || data[i] == '.'))
                {
                    i++;
                }

                if (start < i)
                {
                    numbers.append(data.mid(start, i - start).toDouble());
                }
                else
                {
                    i++; // 防止死循环
                }
            }

            // 执行命令
            int numCount = numbers.size();
            int numIndex = 0;

            switch (cmd.toLatin1())
            {
            case 'M': // 移动到
                while (numIndex + 1 < numCount)
                {
                    qreal x = numbers[numIndex];
                    qreal y = numbers[numIndex + 1];
                    if (isRelative)
                    {
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }
                    path.moveTo(x, y);
                    currentPoint = QPointF(x, y);
                    pathStart = currentPoint; // 更新子路径起点
                    lastControlPoint = currentPoint;
                    numIndex += 2;
                }
                break;

            case 'L': // 线条到
                while (numIndex + 1 < numCount)
                {
                    qreal x = numbers[numIndex];
                    qreal y = numbers[numIndex + 1];
                    if (isRelative)
                    {
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }
                    path.lineTo(x, y);
                    currentPoint = QPointF(x, y);
                    lastControlPoint = currentPoint;
                    numIndex += 2;
                }
                break;

            case 'H': // 水平线
                while (numIndex < numCount)
                {
                    qreal x = numbers[numIndex];
                    if (isRelative)
                    {
                        x += currentPoint.x();
                    }
                    path.lineTo(x, currentPoint.y());
                    currentPoint.setX(x);
                    lastControlPoint = currentPoint;
                    numIndex += 1;
                }
                break;

            case 'V': // 垂直线
                while (numIndex < numCount)
                {
                    qreal y = numbers[numIndex];
                    if (isRelative)
                    {
                        y += currentPoint.y();
                    }
                    path.lineTo(currentPoint.x(), y);
                    currentPoint.setY(y);
                    lastControlPoint = currentPoint;
                    numIndex += 1;
                }
                break;

            case 'C': // 三次贝塞尔曲线
                while (numIndex + 5 < numCount)
                {
                    qreal x1 = numbers[numIndex];
                    qreal y1 = numbers[numIndex + 1];
                    qreal x2 = numbers[numIndex + 2];
                    qreal y2 = numbers[numIndex + 3];
                    qreal x = numbers[numIndex + 4];
                    qreal y = numbers[numIndex + 5];

                    if (isRelative)
                    {
                        x1 += currentPoint.x();
                        y1 += currentPoint.y();
                        x2 += currentPoint.x();
                        y2 += currentPoint.y();
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }

                    path.cubicTo(x1, y1, x2, y2, x, y);
                    currentPoint = QPointF(x, y);
                    lastControlPoint = QPointF(x2, y2);
                    numIndex += 6;
                }
                break;

            case 'S': // 平滑三次贝塞尔曲线
                while (numIndex + 3 < numCount)
                {
                    qreal x2 = numbers[numIndex];
                    qreal y2 = numbers[numIndex + 1];
                    qreal x = numbers[numIndex + 2];
                    qreal y = numbers[numIndex + 3];

                    QPointF controlPoint;
                    if (path.elementCount() > 0 &&
                        (path.elementAt(path.elementCount() - 1).type == QPainterPath::CurveToDataElement ||
                         path.elementAt(path.elementCount() - 2).type == QPainterPath::CurveToDataElement))
                    {
                        // 使用对称控制点
                        controlPoint = QPointF(2 * currentPoint.x() - lastControlPoint.x(),
                                               2 * currentPoint.y() - lastControlPoint.y());
                    }
                    else
                    {
                        controlPoint = currentPoint;
                    }

                    if (isRelative)
                    {
                        x2 += currentPoint.x();
                        y2 += currentPoint.y();
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }

                    path.cubicTo(controlPoint.x(), controlPoint.y(), x2, y2, x, y);
                    currentPoint = QPointF(x, y);
                    lastControlPoint = QPointF(x2, y2);
                    numIndex += 4;
                }
                break;

            case 'Q': // 二次贝塞尔曲线
                while (numIndex + 3 < numCount)
                {
                    qreal x1 = numbers[numIndex];
                    qreal y1 = numbers[numIndex + 1];
                    qreal x = numbers[numIndex + 2];
                    qreal y = numbers[numIndex + 3];

                    if (isRelative)
                    {
                        x1 += currentPoint.x();
                        y1 += currentPoint.y();
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }

                    path.quadTo(x1, y1, x, y);
                    currentPoint = QPointF(x, y);
                    lastControlPoint = QPointF(x1, y1);
                    numIndex += 4;
                }
                break;

            case 'T': // 平滑二次贝塞尔曲线
                while (numIndex + 1 < numCount)
                {
                    qreal x = numbers[numIndex];
                    qreal y = numbers[numIndex + 1];

                    QPointF controlPoint;
                    if (path.elementCount() > 0 &&
                        path.elementAt(path.elementCount() - 1).type == QPainterPath::CurveToDataElement)
                    {
                        controlPoint = QPointF(2 * currentPoint.x() - lastControlPoint.x(),
                                               2 * currentPoint.y() - lastControlPoint.y());
                    }
                    else
                    {
                        controlPoint = currentPoint;
                    }

                    if (isRelative)
                    {
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }

                    path.quadTo(controlPoint.x(), controlPoint.y(), x, y);
                    currentPoint = QPointF(x, y);
                    lastControlPoint = controlPoint;
                    numIndex += 2;
                }
                break;

            case 'A': // 椭圆弧（简化实现，转换为贝塞尔曲线）
                while (numIndex + 6 < numCount)
                {
                    // 参数：rx, ry, x-axis-rotation, large-arc-flag, sweep-flag, x, y
                    qreal rx = numbers[numIndex];
                    qreal ry = numbers[numIndex + 1];
                    qreal xAxisRotation = numbers[numIndex + 2] * M_PI / 180.0;
                    bool largeArcFlag = numbers[numIndex + 3] > 0.5;
                    bool sweepFlag = numbers[numIndex + 4] > 0.5;
                    qreal x = numbers[numIndex + 5];
                    qreal y = numbers[numIndex + 6];

                    if (isRelative)
                    {
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }

                    // 简化实现：使用直线代替椭圆弧
                    // TODO: 实现完整的椭圆弧到贝塞尔曲线的转换
                    path.lineTo(x, y);
                    currentPoint = QPointF(x, y);
                    lastControlPoint = currentPoint;
                    numIndex += 7;
                }
                break;

            case 'Z': // 闭合路径
                path.closeSubpath();
                currentPoint = pathStart; // 回到子路径起点
                lastControlPoint = currentPoint;
                break;

            default:
                // 未知命令，跳过
                break;
            }
        }
        else
        {
            // 没有命令字符，跳过
            i++;
        }
    }
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
        qDebug() << "矩形变换:" << transform;
        parseTransformAttribute(rect, transform);
        qDebug() << "矩形变换后matrix:" << rect->transform();
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

    QRectF rect(0, 0, 2 * rx, 2 * ry);
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

    QRectF rect(cx - r, cy - r, 2 * r, 2 * r);
    DrawingEllipse *circle = new DrawingEllipse(rect);

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
    QStringList pointsList = pointsStr.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    for (int i = 0; i < pointsList.size(); i += 2)
    {
        if (i + 1 < pointsList.size())
        {
            qreal x = pointsList[i].toDouble();
            qreal y = pointsList[i + 1].toDouble();

            if (i == 0)
            {
                path.moveTo(x, y);
            }
            else
            {
                path.lineTo(x, y);
            }
        }
    }

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
        parseTransformAttribute(shape, transform);
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
    // 获取文本内容
    QString text = element.text().trimmed();
    if (text.isEmpty())
    {
        // qDebug() << "文本元素内容为空";
        return nullptr;
    }

    // 获取位置
    qreal x = element.attribute("x", "0").toDouble();
    qreal y = element.attribute("y", "0").toDouble();
    QPointF position(x, y);

    // 创建文本形状
    DrawingText *shape = new DrawingText(text);
    shape->setPosition(position);

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

    // 解析样式属性
    parseStyleAttributes(shape, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        parseTransformAttribute(shape, transform);
    }

    // qDebug() << "解析文本元素:" << text << "位置:" << position;
    return shape;
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
                // qDebug() << "应用渐变填充:" << refId;
                // qDebug() << "渐变类型:" << gradient.type();
                // qDebug() << "渐变停止点数量:" << gradient.stops().count();
                // qDebug() << "坐标模式: ObjectBoundingMode";

                // 检查画刷是否有效
                if (brush.style() == Qt::LinearGradientPattern ||
                    brush.style() == Qt::RadialGradientPattern ||
                    brush.style() == Qt::ConicalGradientPattern)
                {
                    // qDebug() << "画刷样式有效:" << brush.style();
                }
                else
                {
                    // qDebug() << "警告: 画刷样式无效:" << brush.style();
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

void SvgHandler::parseTransformAttribute(DrawingShape *shape, const QString &transformStr)
{
    // 解析SVG变换字符串，如 "translate(10,20) rotate(45) scale(2,1)"
    QRegularExpression regex("(\\S+)\\s*\\(\\s*([^)]+)\\s*\\)");
    QRegularExpressionMatchIterator iter = regex.globalMatch(transformStr);

    QTransform currentTransform = shape->transform();

    while (iter.hasNext())
    {
        QRegularExpressionMatch match = iter.next();
        QString func = match.captured(1);
        QString paramsStr = match.captured(2);

        QStringList params = paramsStr.split(QRegularExpression("\\s*,\\s*|\\s+"), Qt::SkipEmptyParts);

        if (func == "translate" && params.size() >= 2)
        {
            qreal tx = params[0].toDouble();
            qreal ty = params.size() > 1 ? params[1].toDouble() : 0.0;
            currentTransform.translate(tx, ty);
        }
        else if (func == "rotate" && params.size() >= 1)
        {
            qreal angle = params[0].toDouble();
            qreal cx = 0, cy = 0;
            if (params.size() >= 3)
            {
                cx = params[1].toDouble();
                cy = params[2].toDouble();
            }
            currentTransform.translate(cx, cy);
            currentTransform.rotate(angle);
            currentTransform.translate(-cx, -cy);
        }
        else if (func == "scale" && params.size() >= 1)
        {
            qreal sx = params[0].toDouble();
            qreal sy = params.size() > 1 ? params[1].toDouble() : sx;
            // 缩放通常以原点为中心，除非特别指定
            currentTransform.scale(sx, sy);
        }
        else if (func == "matrix" && params.size() >= 6)
        {
            // 解析6参数仿射变换矩阵
            qreal a = params[0].toDouble();
            qreal b = params[1].toDouble();
            qreal c = params[2].toDouble();
            qreal d = params[3].toDouble();
            qreal e = params[4].toDouble();
            qreal f = params[5].toDouble();

            QTransform qtTransform(a, b, c, d, e, f);
            currentTransform = qtTransform;
        }
    }

    // 应用组合变换到形状
    shape->applyTransform(currentTransform);
}

QColor SvgHandler::parseColor(const QString &colorStr)
{
    if (colorStr.startsWith("#"))
    {
        return QColor(colorStr);
    }
    else if (colorStr.startsWith("rgb("))
    {
        // 解析rgb(r,g,b)格式
        QRegularExpression regex(R"(rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\))");
        QRegularExpressionMatch match = regex.match(colorStr);
        if (match.hasMatch())
        {
            int r = match.captured(1).toInt();
            int g = match.captured(2).toInt();
            int b = match.captured(3).toInt();
            QColor color(r, g, b);
            if (color.isValid())
            {
                return color;
            }
        }

        // 尝试更宽松的匹配，允许小数
        QRegularExpression regexFloat(R"(rgb\s*\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*\))");
        match = regexFloat.match(colorStr);
        if (match.hasMatch())
        {
            int r = qRound(match.captured(1).toDouble());
            int g = qRound(match.captured(2).toDouble());
            int b = qRound(match.captured(3).toDouble());
            QColor color(r, g, b);
            if (color.isValid())
            {
                return color;
            }
        }
    }
    else if (colorStr.startsWith("rgba("))
    {
        // 解析rgba(r,g,b,a)格式
        QRegularExpression regex(R"(rgba\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([\d.]+)\s*\))");
        QRegularExpressionMatch match = regex.match(colorStr);
        if (match.hasMatch())
        {
            int r = match.captured(1).toInt();
            int g = match.captured(2).toInt();
            int b = match.captured(3).toInt();
            qreal a = match.captured(4).toDouble();            // a是0-1范围的透明度值
            return QColor(r, g, b, static_cast<int>(a * 255)); // 转换为0-255范围
        }
    }

    // 尝试直接使用颜色名称
    QColor color(colorStr);
    if (color.isValid())
    {
        return color;
    }

    // 默认返回黑色
    return QColor(Qt::black);
}

qreal SvgHandler::parseLength(const QString &lengthStr)
{
    // 解析长度值，处理单位（px, pt, cm, mm, in等）
    QRegularExpression regex("([0-9.]+)([a-z%]*)");
    QRegularExpressionMatch match = regex.match(lengthStr);

    if (match.hasMatch())
    {
        qreal value = match.captured(1).toDouble();
        QString unit = match.captured(2);

        // 简单处理，假设都是像素单位
        return value;
    }

    // 如果没有匹配到，尝试直接转换为数字
    bool ok;
    qreal value = lengthStr.toDouble(&ok);
    if (ok)
    {
        return value;
    }

    return 10.0; // 默认值
}

bool SvgHandler::exportToSvg(DrawingScene *scene, const QString &fileName)
{
    QDomDocument doc = exportSceneToSvgDocument(scene);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "无法创建SVG文件:" << fileName;
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
        qDebug() << "未知的图形类型，无法导出:" << shape->shapeType();
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

    QDomElement defs = defsNodes.at(0).toElement();
    // qDebug() << "解析defs元素";

    // 清理之前的渐变定义
    s_gradients.clear();

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
            // qDebug() << "解析线性渐变:" << id;
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
            // qDebug() << "解析径向渐变:" << id;
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

    // qDebug() << "线性渐变参数: x1=" << x1 << " y1=" << y1 << " x2=" << x2 << " y2=" << y2;

    QLinearGradient gradient(QPointF(x1, y1), QPointF(x2, y2));

    // 解析渐变停止点
    parseGradientStops(&gradient, element);

    // 调试：检查渐变停止点
    auto stops = gradient.stops();
    // qDebug() << "渐变停止点数量:" << stops.count();
    for (int i = 0; i < stops.count(); ++i)
    {
        auto stop = stops.at(i);
        // qDebug() << "停止点" << i << ": 位置=" << stop.first
        // << "颜色=" << stop.second.name()
        // << "透明度=" << stop.second.alphaF();
    }

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

    // qDebug() << "径向渐变参数: cx=" << cx << " cy=" << cy << " r=" << r << " fx=" << fx << " fy=" << fy;

    QRadialGradient gradient(QPointF(cx, cy), r, QPointF(fx, fy));

    // 解析渐变停止点
    parseGradientStops(&gradient, element);

    // 调试：检查渐变停止点
    auto stops = gradient.stops();
    // qDebug() << "渐变停止点数量:" << stops.count();
    for (int i = 0; i < stops.count(); ++i)
    {
        auto stop = stops.at(i);
        // qDebug() << "停止点" << i << ": 位置=" << stop.first
        // << "颜色=" << stop.second.name()
        // << "透明度=" << stop.second.alphaF();
    }

    return gradient;
}

void SvgHandler::parseGradientStops(QGradient *gradient, const QDomElement &element)
{
    QDomNodeList stops = element.elementsByTagName("stop");
    // qDebug() << "解析" << stops.size() << "个停止点";

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
        // qDebug() << "停止点" << i << " offset=" << offsetStr << " -> " << offset;

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
        // qDebug() << "停止点" << i << " stop-color=" << stopColor << " -> " << color.name();

        // 解析透明度
        qreal opacity = stopOpacity.toDouble();
        color.setAlphaF(opacity);
        // qDebug() << "停止点" << i << " stop-opacity=" << stopOpacity << " -> 透明度=" << color.alphaF();

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
    // qDebug() << "解析滤镜元素";

    // 清理之前的滤镜定义
    for (auto it = s_filters.begin(); it != s_filters.end(); ++it)
    {
        delete it.value();
    }
    s_filters.clear();
    // qDebug() << "清理了" << s_filters.size() << "个滤镜定义";

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
                        s_filters[id] = effect;
                        // qDebug() << "解析滤镜:" << id << "类型:" << tagName;
                        break; // 一个滤镜只处理第一个原始元素
                    }
                }
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
    blur->setBlurRadius(radius);

    // qDebug() << "高斯模糊滤镜 - 半径:" << radius;
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

    // qDebug() << "阴影滤镜 - 模糊半径:" << blurRadius << "偏移:" << offsetX << "," << offsetY;
    return shadow;
}

void SvgHandler::applyFilterToShape(DrawingShape *shape, const QString &filterId)
{
    if (!shape || filterId.isEmpty())
    {
        return;
    }

    if (s_filters.contains(filterId))
    {
        QGraphicsEffect *effect = s_filters[filterId];
        if (!effect)
        {
            // qDebug() << "滤镜效果为空:" << filterId;
            return;
        }

        // 注意：Qt中一个effect不能被多个item共享，需要克隆
        if (auto blurEffect = qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            QGraphicsBlurEffect *newBlur = new QGraphicsBlurEffect();
            if (newBlur)
            {
                newBlur->setBlurRadius(blurEffect->blurRadius());
                shape->setGraphicsEffect(newBlur);
                // qDebug() << "应用高斯模糊滤镜到图形:" << filterId << "半径:" << blurEffect->blurRadius();
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
                // qDebug() << "应用阴影滤镜到图形:" << filterId;
            }
        }
    }
    else
    {
        // qDebug() << "未找到滤镜:" << filterId;
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
            // qDebug() << "滤镜效果为空:" << filterId;
            return;
        }

        // 注意：Qt中一个effect不能被多个item共享，需要克隆
        if (auto blurEffect = qobject_cast<QGraphicsBlurEffect *>(effect))
        {
            QGraphicsBlurEffect *newBlur = new QGraphicsBlurEffect();
            if (newBlur)
            {
                newBlur->setBlurRadius(blurEffect->blurRadius());
                group->setGraphicsEffect(newBlur);
                // qDebug() << "应用高斯模糊滤镜到组合:" << filterId << "半径:" << blurEffect->blurRadius();
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
                // qDebug() << "应用阴影滤镜到组合:" << filterId;
            }
        }
    }
    else
    {
        // qDebug() << "未找到滤镜:" << filterId;
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
    // qDebug() << "解析Pattern元素";

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
            // qDebug() << "解析Pattern:" << id;
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
void SvgHandler::parseMarkerElements(const QDomElement &root)
{
    // 查找defs元素
    QDomNodeList defsNodes = root.elementsByTagName("defs");
    if (defsNodes.isEmpty())
    {
        // 静默返回，不显示警告信息，因为简单的SVG文件可能不需要Marker
        return;
    }

    QDomElement defs = defsNodes.at(0).toElement();
    // qDebug() << "解析Marker元素";

    // 清理之前的Marker定义
    s_markers.clear();
    s_markerCache.clear();

    // 解析所有Marker
    QDomNodeList markers = defs.elementsByTagName("marker");
    for (int i = 0; i < markers.size(); ++i)
    {
        QDomElement markerElement = markers.at(i).toElement();
        QString id = markerElement.attribute("id");
        if (!id.isEmpty())
        {
            s_markers[id] = markerElement;
            // 预渲染Marker到缓存
            renderMarkerToCache(id, markerElement);
            // qDebug() << "解析Marker:" << id;
        }
    }
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
    pixmap.fill(Qt::transparent);

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
        }
    }

    painter.end();
    s_markerCache[id] = pixmap;
}

// 创建Marker路径
QPainterPath SvgHandler::createMarkerPath(const QString &markerId, const QPointF &startPoint, const QPointF &endPoint)
{
    QPainterPath markerPath;

    if (!s_markers.contains(markerId) || !s_markerCache.contains(markerId))
    {
        return markerPath;
    }

    QDomElement markerElement = s_markers[markerId];
    QPixmap markerPixmap = s_markerCache[markerId];

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

    // 应用end marker
    if (!markerEnd.isEmpty())
    {
        QRegularExpression markerRegex("url\\(#([^\\)]+)\\)");
        QRegularExpressionMatch match = markerRegex.match(markerEnd);
        if (match.hasMatch())
        {
            QString markerId = match.captured(1);
            applyMarkerToPath(path, markerId, "end");
        }
    }

    // TODO: 实现start和mid marker
    // 需要扩展DrawingPath来支持多个marker
}

void SvgHandler::applyMarkerToPath(DrawingPath *path, const QString &markerId, const QString &position)
{
    if (!path || markerId.isEmpty())
    {
        return;
    }

    if (s_markers.contains(markerId) && s_markerCache.contains(markerId))
    {
        QDomElement markerElement = s_markers[markerId];
        QPixmap markerPixmap = s_markerCache[markerId];

        // 获取Marker属性
        qreal markerWidth = parseLength(markerElement.attribute("markerWidth", "10"));
        qreal markerHeight = parseLength(markerElement.attribute("markerHeight", "10"));
        qreal refX = parseLength(markerElement.attribute("refX", "0"));
        qreal refY = parseLength(markerElement.attribute("refY", "0"));

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

        // 创建Marker图像变换
        QTransform transform;
        transform.translate(markerPoint.x() - refX, markerPoint.y() - refY);
        transform.rotate(angle);

        // 存储Marker信息用于渲染
        path->setMarker(markerId, markerPixmap, transform);
    }
    else
    {
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
                s_definedElements[id] = element.cloneNode().toElement();
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
    // 获取href属性（引用的元素ID）
    QString href = element.attribute("href");
    if (href.isEmpty())
    {
        href = element.attribute("xlink:href"); // 兼容旧版本SVG
    }

    if (href.isEmpty() || !href.startsWith("#"))
    {
        return nullptr;
    }

    // 提取引用的ID
    QString refId = href.mid(1); // 去掉#

    // 查找定义的元素
    if (!s_definedElements.contains(refId))
    {
        return nullptr;
    }

    QDomElement referencedElement = s_definedElements[refId];

    // 克隆并解析引用的元素
    DrawingShape *shape = parseSvgElement(referencedElement);
    if (!shape)
    {
        return nullptr;
    }

    // 应用use元素的位置偏移
    qreal x = element.attribute("x", "0").toDouble();
    qreal y = element.attribute("y", "0").toDouble();

    // 应用use元素的位置偏移
    if (x != 0 || y != 0)
    {
        shape->setPos(QPointF(x, y));
    }

    // 解析并应用变换
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        qDebug() << "use元素变换:" << transform << "位置:" << QPointF(x, y);
        // 对于use元素，需要调整旋转中心，考虑位置偏移
        QString adjustedTransform = adjustTransformForUseElement(transform, x, y);
        qDebug() << "调整后的变换:" << adjustedTransform;
        parseTransformAttribute(shape, adjustedTransform);
        qDebug() << "use元素变换后matrix:" << shape->transform();
    }

    // 解析样式属性（use元素的样式会覆盖引用元素的样式）
    parseStyleAttributes(shape, element);

    // 处理特定属性的覆盖（例如fill、stroke等）
    if (element.hasAttribute("fill"))
    {
        QString fill = element.attribute("fill");
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

    if (element.hasAttribute("stroke"))
    {
        QString stroke = element.attribute("stroke");
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

    if (element.hasAttribute("stroke-width"))
    {
        qreal width = parseLength(element.attribute("stroke-width"));
        if (width > 0)
        {
            QPen pen = shape->strokePen();
            pen.setWidthF(width);
            shape->setStrokePen(pen);
        }
    }

    return shape;
}

// 调整use元素的变换，考虑位置偏移
QString SvgHandler::adjustTransformForUseElement(const QString &transformStr, qreal x, qreal y)
{
    // 解析SVG变换字符串
    QRegularExpression regex("(\\S+)\\s*\\(\\s*([^)]+)\\s*\\)");
    QRegularExpressionMatchIterator iter = regex.globalMatch(transformStr);

    QString result;

    while (iter.hasNext())
    {
        QRegularExpressionMatch match = iter.next();
        QString func = match.captured(1);
        QString paramsStr = match.captured(2);

        QStringList params = paramsStr.split(QRegularExpression("\\s*,\\s*|\\s+"), Qt::SkipEmptyParts);

        if (!result.isEmpty())
        {
            result += " ";
        }

        if (func == "rotate" && params.size() >= 1)
        {
            qreal angle = params[0].toDouble();
            qreal cx = 0, cy = 0;
            if (params.size() >= 3)
            {
                cx = params[1].toDouble();
                cy = params[2].toDouble();
                // 对于use元素，旋转中心需要减去位置偏移
                // 因为parseTransformAttribute中的旋转实现假设形状在原点
                cx -= x;
                cy -= y;
            }
            result += QString("rotate(%1 %2 %3)").arg(angle).arg(cx).arg(cy);
        }
        else
        {
            // 其他变换保持不变
            result += func + "(" + paramsStr + ")";
        }
    }

    return result;
}
