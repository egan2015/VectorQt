#include <QDebug>
#include <QTextStream>
#include <QFile>
#include <QXmlStreamReader>

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPointF>
#include <QTransform>
#include <QRegularExpression>
#include <QStack>
#include "svgstreamhandler.h"
#include "fastpathparser.h"
#include "svgelementcollector.h"
#include "drawing-shape.h"
#include "drawing-layer.h"
#include "drawing-group.h"
#include "layer-manager.h"
#include "../ui/drawingscene.h"
#include "svghandler.h"  // 为了复用函数

// 使用SvgHandler中的全局存储
extern QHash<QString, QGradient> s_gradients;
extern QHash<QString, QGraphicsEffect *> s_filters;
extern QHash<QString, QBrush> s_patterns;
extern QHash<QString, QDomElement> s_markers;
extern QHash<QString, MarkerData> s_markerDataCache;
extern QHash<QString, QDomElement> s_definedElements;

// SvgStreamParser类实现
bool SvgStreamParser::parseSvgFile(const QString &fileName, SvgStreamElement &rootElement)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    QXmlStreamReader reader(&file);
    
    if (!reader.readNextStartElement())
    {
        return false;
    }

    if (reader.name() != "svg")
    {
        return false;
    }

    rootElement = parseElement(reader);
    
    if (reader.hasError())
    {
        return false;
    }

    return true;
}

SvgStreamElement SvgStreamParser::parseElement(QXmlStreamReader &reader)
{
    SvgStreamElement element;
    element.tagName = reader.name().toString();
    
    // 解析属性
    QXmlStreamAttributes attributes = reader.attributes();
    for (const QXmlStreamAttribute &attr : attributes)
    {
        element.attributes[attr.name().toString()] = attr.value().toString();
    }
    
    // 解析子元素和文本内容
    while (reader.readNext())
    {
        if (reader.isEndElement())
        {
            break;
        }
        
        if (reader.isStartElement())
        {
            SvgStreamElement childElement = parseElement(reader);
            element.children.append(childElement);
        }
        else if (reader.isCharacters())
        {
            element.text += reader.text().toString();
        }
    }
    
    return element;
}

// SvgStreamHandler主接口实现 - 使用真正的流式解析
bool SvgStreamHandler::importFromSvg(DrawingScene *scene, const QString &fileName)
{
    // 设置SVG导入标志，防止创建默认图层
    LayerManager::instance()->setSvgImporting(true);
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LayerManager::instance()->setSvgImporting(false);
        return false;
    }

    QXmlStreamReader reader(&file);
    
    if (!reader.readNextStartElement())
    {
        LayerManager::instance()->setSvgImporting(false);
        return false;
    }

    if (reader.name() != "svg")
    {
        LayerManager::instance()->setSvgImporting(false);
        return false;
    }

    // 使用栈结构进行真正的流式解析
    bool result = parseSvgStream(scene, reader);
    LayerManager::instance()->setSvgImporting(false);
    return result;
}

// 从流式解析的元素解析SVG元数据
SvgMetadata SvgStreamHandler::parseSvgMetadataFromElement(const SvgStreamElement &svgElement)
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
        QStringList parts = viewBoxStr.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
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

// 应用SVG设置到Scene
void SvgStreamHandler::applySvgSettingsToScene(DrawingScene *scene, const SvgMetadata &metadata)
{
    if (!scene) {
        return;
    }
    
    // 设置scene的边界为viewBox
    scene->setSceneRect(metadata.viewBox);
}

// 计算SVG到Scene的变换矩阵 - 使用SvgTransformManager
QTransform SvgStreamHandler::calculateSvgToSceneTransform(const SvgMetadata &metadata)
{
    return SvgTransformManager::calculateSvgToSceneTransform(metadata);
}

// 从流式解析结果收集元素，返回与DOM版本相同的数据结构
SvgElementCollector::CollectedElements SvgStreamHandler::collectElementsFromStream(const SvgStreamElement &rootElement)
{
    SvgElementCollector::CollectedElements collected;
    collectElementsFromStreamRecursive(rootElement, collected, false, false);
    return collected;
}

void SvgStreamHandler::collectElementsFromStreamRecursive(const SvgStreamElement &element, 
                                                         SvgElementCollector::CollectedElements &collected, 
                                                         bool isInDefs,
                                                         bool isInGroup)
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
        if (!isInDefs && !isInGroup) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.paths.append(tempElement);
        }
    } else if (tagName == "rect") {
        if (!isInDefs && !isInGroup) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.rectangles.append(tempElement);
        }
    } else if (tagName == "ellipse") {
        if (!isInDefs && !isInGroup) {
            QDomDocument tempDoc;
            QDomElement tempElement = tempDoc.createElement(tagName);
            for (auto it = element.attributes.begin(); it != element.attributes.end(); ++it) {
                tempElement.setAttribute(it.key(), it.value());
            }
            collected.ellipses.append(tempElement);
        }
    } else if (tagName == "circle") {
        if (!isInDefs && !isInGroup) {
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
        
        // 添加子元素 - 这是关键修复！
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
    bool newIsInGroup = isInGroup || (tagName == "g");
    for (const SvgStreamElement &childElement : element.children) {
        collectElementsFromStreamRecursive(childElement, collected, isInDefs, newIsInGroup);
    }
}

// 辅助方法：将元素添加到正确的位置
static void addShapeToCorrectPlace(DrawingScene *scene, DrawingShape *shape, 
                                   QStack<DrawingShape*> &elementStack, int &elementCount,
                                   const QTransform &svgToSceneTransform)
{
    // 应用SVG到Scene的变换
    if (!svgToSceneTransform.isIdentity()) {
        shape->setTransform(svgToSceneTransform * shape->transform());
    }
    
    if (elementStack.isEmpty()) {
        // 检查是否有当前活动图层
        DrawingLayer *activeLayer = LayerManager::instance()->activeLayer();
        if (activeLayer && LayerManager::instance()->layerCount() > 1) {
            // 有图层，添加到图层
            activeLayer->addShape(shape);
        } else {
            // 没有图层，直接添加到scene
            scene->addItem(shape);
            shape->setZValue(elementCount++);
        }
    } else {
        DrawingShape *parent = elementStack.top();
        if (DrawingGroup *group = dynamic_cast<DrawingGroup*>(parent)) {
            group->addItem(shape);
        }
    }
}

// 真正的基于栈的流式解析
bool SvgStreamHandler::parseSvgStream(DrawingScene *scene, QXmlStreamReader &reader)
{
    // 元素栈：存储当前嵌套的父元素
    QStack<DrawingShape*> elementStack;
    int elementCount = 0;
    bool inDefs = false;  // 跟踪是否在defs元素中
    
    // 解析SVG元数据
    SvgMetadata metadata;
    if (reader.attributes().hasAttribute("width")) {
        metadata.size.setWidth(parseLength(reader.attributes().value("width").toString()));
        metadata.hasSize = true;
    }
    if (reader.attributes().hasAttribute("height")) {
        metadata.size.setHeight(parseLength(reader.attributes().value("height").toString()));
        metadata.hasSize = true;
    }
    if (reader.attributes().hasAttribute("viewBox")) {
        QString viewBoxStr = reader.attributes().value("viewBox").toString();
        QStringList parts = viewBoxStr.split(' ', Qt::SkipEmptyParts);
        if (parts.size() == 4) {
            qreal x = parts[0].toDouble();
            qreal y = parts[1].toDouble();
            qreal width = parts[2].toDouble();
            qreal height = parts[3].toDouble();
            metadata.viewBox = QRectF(x, y, width, height);
            metadata.hasViewBox = true;
        }
    }
    
    // 应用SVG设置到Scene
    applySvgSettingsToScene(scene, metadata);
    
    // 计算SVG到Scene的变换矩阵
    QTransform svgToSceneTransform = calculateSvgToSceneTransform(metadata);
    
    while (!reader.atEnd())
    {
        QXmlStreamReader::TokenType token = reader.readNext();
        
        if (token == QXmlStreamReader::StartElement)
        {
            QString tagName = reader.name().toString();
            
            // 处理defs元素
            if (tagName == "defs") {
                inDefs = true;
                continue;
            }
            
            // 跳过defs中的所有元素（暂时不处理渐变）
            if (inDefs) {
                continue;
            }
            
            if (tagName == "g")
            {
                // 检查是否为Inkscape图层
                QString inkscapeLabel = reader.attributes().value("inkscape:label").toString();
                QString inkscapeGroupmode = reader.attributes().value("inkscape:groupmode").toString();
                bool isLayer = (!inkscapeGroupmode.isEmpty() && inkscapeGroupmode == "layer") || 
                               (!inkscapeLabel.isEmpty());
                
                if (isLayer) {
                    // 创建Inkscape图层
                    DrawingLayer *layer = LayerManager::instance()->createLayerForSvg(inkscapeLabel);
                    
                    // 解析图层属性
                    QString visibility = reader.attributes().value("visibility", "visible").toString();
                    layer->setVisible(visibility != "hidden");
                    
                    QString opacity = reader.attributes().value("opacity", "1.0").toString();
                    layer->setOpacity(opacity.toDouble());
                    
                    // 处理图层变换
                    if (reader.attributes().hasAttribute("transform")) {
                        QString transform = reader.attributes().value("transform").toString();
                        // TODO: 应用变换到图层
                    }
                    
                    // 图层不推入栈，后续元素直接添加到当前活动图层
                } else {
                    // 普通group
                    DrawingGroup *group = new DrawingGroup();
                    parseStyleAttributes(group, reader.attributes());
                    
                    // 处理变换
                    if (reader.attributes().hasAttribute("transform")) {
                        QString transform = reader.attributes().value("transform").toString();
                        parseTransformAttribute(group, transform);
                    }
                    
                    if (elementStack.isEmpty()) {
                        // 顶级group，直接添加到scene
                        scene->addItem(group);
                        group->setZValue(elementCount++);
                    } else {
                        // 嵌套group，添加到父元素
                        DrawingShape *parent = elementStack.top();
                        if (DrawingGroup *parentGroup = dynamic_cast<DrawingGroup*>(parent)) {
                            parentGroup->addItem(group);
                        }
                    }
                    elementStack.push(group);
                }
            }
            else if (tagName == "rect")
            {
                DrawingRectangle *rect = parseRectElementFromAttributes(reader.attributes());
                if (rect) {
                    addShapeToCorrectPlace(scene, rect, elementStack, elementCount, svgToSceneTransform);
                }
            }
            else if (tagName == "circle")
            {
                DrawingEllipse *circle = parseCircleElementFromAttributes(reader.attributes());
                if (circle) {
                    addShapeToCorrectPlace(scene, circle, elementStack, elementCount, svgToSceneTransform);
                }
            }
            else if (tagName == "ellipse")
            {
                DrawingEllipse *ellipse = parseEllipseElementFromAttributes(reader.attributes());
                if (ellipse) {
                    addShapeToCorrectPlace(scene, ellipse, elementStack, elementCount, svgToSceneTransform);
                }
            }
            else if (tagName == "path")
            {
                DrawingPath *path = parsePathElementFromAttributes(reader.attributes());
                if (path) {
                    addShapeToCorrectPlace(scene, path, elementStack, elementCount, svgToSceneTransform);
                }
            }
            else if (tagName == "text")
            {
                DrawingText *text = parseTextElementFromAttributes(reader);
                if (text) {
                    addShapeToCorrectPlace(scene, text, elementStack, elementCount, svgToSceneTransform);
                }
            }
            // 可以继续添加其他元素类型...
        }
        else if (token == QXmlStreamReader::EndElement)
        {
            QString tagName = reader.name().toString();
            
            // 处理defs结束
            if (tagName == "defs") {
                inDefs = false;
            }
            // 处理group结束
            else if (tagName == "g" && !elementStack.isEmpty()) {
                elementStack.pop(); // group结束，出栈
            }
        }
    }
    
    return true;
}

// 从属性直接创建rect元素
DrawingRectangle* SvgStreamHandler::parseRectElementFromAttributes(const QXmlStreamAttributes &attributes)
{
    qreal x = attributes.hasAttribute("x") ? attributes.value("x").toDouble() : 0;
    qreal y = attributes.hasAttribute("y") ? attributes.value("y").toDouble() : 0;
    qreal width = attributes.hasAttribute("width") ? attributes.value("width").toDouble() : 0;
    qreal height = attributes.hasAttribute("height") ? attributes.value("height").toDouble() : 0;
    
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    
    DrawingRectangle *rect = new DrawingRectangle(QRectF(0, 0, width, height));
    rect->setPos(x, y);
    
    // 解析样式
    parseStyleAttributes(rect, attributes);
    
    return rect;
}

// 从属性直接创建circle元素
DrawingEllipse* SvgStreamHandler::parseCircleElementFromAttributes(const QXmlStreamAttributes &attributes)
{
    qreal cx = attributes.hasAttribute("cx") ? attributes.value("cx").toDouble() : 0;
    qreal cy = attributes.hasAttribute("cy") ? attributes.value("cy").toDouble() : 0;
    qreal r = attributes.hasAttribute("r") ? attributes.value("r").toDouble() : 0;
    
    if (r <= 0) {
        return nullptr;
    }
    
    DrawingEllipse *circle = new DrawingEllipse(QRectF(-r, -r, 2*r, 2*r));
    circle->setPos(cx, cy);
    
    // 解析样式
    parseStyleAttributes(circle, attributes);
    
    return circle;
}

// 从属性直接创建ellipse元素
DrawingEllipse* SvgStreamHandler::parseEllipseElementFromAttributes(const QXmlStreamAttributes &attributes)
{
    qreal cx = attributes.hasAttribute("cx") ? attributes.value("cx").toDouble() : 0;
    qreal cy = attributes.hasAttribute("cy") ? attributes.value("cy").toDouble() : 0;
    qreal rx = attributes.hasAttribute("rx") ? attributes.value("rx").toDouble() : 0;
    qreal ry = attributes.hasAttribute("ry") ? attributes.value("ry").toDouble() : rx;
    
    if (rx <= 0 || ry <= 0) {
        return nullptr;
    }
    
    DrawingEllipse *ellipse = new DrawingEllipse(QRectF(-rx, -ry, 2*rx, 2*ry));
    ellipse->setPos(cx, cy);
    
    // 解析样式
    parseStyleAttributes(ellipse, attributes);
    
    return ellipse;
}

// 从属性直接创建path元素
DrawingPath* SvgStreamHandler::parsePathElementFromAttributes(const QXmlStreamAttributes &attributes)
{
    if (!attributes.hasAttribute("d")) {
        return nullptr;
    }
    
    QString d = attributes.value("d").toString();
    QPainterPath path;
    parseSvgPathData(d, path);
    
    DrawingPath *drawingPath = new DrawingPath();
    drawingPath->setPath(path);
    
    // 自动生成控制点
    QVector<QPointF> controlPoints;
    QVector<QPainterPath::ElementType> controlPointTypes;
    for (int i = 0; i < path.elementCount(); ++i) {
        QPainterPath::Element element = path.elementAt(i);
        controlPoints.append(QPointF(element.x, element.y));
        controlPointTypes.append(element.type);
    }
    drawingPath->setControlPoints(controlPoints);
    drawingPath->setControlPointTypes(controlPointTypes);
    
    // 解析样式
    parseStyleAttributes(drawingPath, attributes);
    
    return drawingPath;
}

// 从属性直接创建text元素
DrawingText* SvgStreamHandler::parseTextElementFromAttributes(const QXmlStreamReader &reader)
{
    // 读取文本内容
    QString text = reader.text().toString().trimmed();
    if (text.isEmpty()) {
        return nullptr;
    }
    
    // 获取位置
    qreal x = reader.attributes().hasAttribute("x") ? reader.attributes().value("x").toDouble() : 0;
    qreal y = reader.attributes().hasAttribute("y") ? reader.attributes().value("y").toDouble() : 0;
    QPointF position(x, y);
    
    DrawingText *shape = new DrawingText(text);
    shape->setPos(position);
    
    // 解析字体属性
    QString fontFamily = reader.attributes().hasAttribute("font-family") ? 
                        reader.attributes().value("font-family").toString() : "Arial";
    qreal fontSize = reader.attributes().hasAttribute("font-size") ? 
                     reader.attributes().value("font-size").toDouble() : 12;
    QString fontWeight = reader.attributes().hasAttribute("font-weight") ? 
                         reader.attributes().value("font-weight").toString() : "normal";
    QString fontStyle = reader.attributes().hasAttribute("font-style") ? 
                        reader.attributes().value("font-style").toString() : "normal";

    QFont font(fontFamily);
    font.setPointSizeF(fontSize);
    if (fontWeight == "bold") font.setBold(true);
    if (fontStyle == "italic") font.setItalic(true);
    shape->setFont(font);

    // 解析文本锚点
    QString textAnchor = reader.attributes().hasAttribute("text-anchor") ? 
                        reader.attributes().value("text-anchor").toString() : "start";
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

    // 解析样式
    parseStyleAttributes(shape, reader.attributes());
    
    return shape;
}

// 流式解析版本的样式解析
void SvgStreamHandler::parseStyleAttributes(DrawingShape *shape, const QXmlStreamAttributes &attributes)
{
    // 解析stroke属性
    if (attributes.hasAttribute("stroke")) {
        QString stroke = attributes.value("stroke").toString();
        if (!stroke.isEmpty()) {
            if (stroke == "none") {
                shape->setStrokePen(Qt::NoPen);
            } else {
                QColor strokeColor = parseColor(stroke);
                if (strokeColor.isValid()) {
                    QPen pen = shape->strokePen();
                    pen.setColor(strokeColor);
                    shape->setStrokePen(pen);
                }
            }
        }
    }
    
    // 解析stroke-width属性
    if (attributes.hasAttribute("stroke-width")) {
        QString strokeWidth = attributes.value("stroke-width").toString();
        if (!strokeWidth.isEmpty()) {
            qreal width = parseLength(strokeWidth);
            if (width > 0) {
                QPen pen = shape->strokePen();
                pen.setWidthF(width);
                shape->setStrokePen(pen);
            }
        }
    }
    
    // 解析fill属性
    if (attributes.hasAttribute("fill")) {
        QString fill = attributes.value("fill").toString();
        if (!fill.isEmpty()) {
            if (fill == "none") {
                shape->setFillBrush(Qt::NoBrush);
            } else {
                QColor fillColor = parseColor(fill);
                if (fillColor.isValid()) {
                    shape->setFillBrush(fillColor);
                }
            }
        }
    }
    
    // 解析opacity属性
    if (attributes.hasAttribute("opacity")) {
        qreal opacity = attributes.value("opacity").toDouble();
        shape->setOpacity(opacity);
    }
    
    // 解析style属性
    if (attributes.hasAttribute("style")) {
        QString style = attributes.value("style").toString();
        parseStyleAttributesFromStyleString(shape, style);
    }
}

// 从style字符串解析样式
void SvgStreamHandler::parseStyleAttributesFromStyleString(DrawingShape *shape, const QString &style)
{
    QStringList styleProps = style.split(";", Qt::SkipEmptyParts);
    for (const QString &prop : styleProps) {
        QStringList keyValue = prop.split(":", Qt::SkipEmptyParts);
        if (keyValue.size() == 2) {
            QString key = keyValue[0].trimmed();
            QString value = keyValue[1].trimmed();
            
            if (key == "stroke") {
                if (value == "none") {
                    shape->setStrokePen(Qt::NoPen);
                } else {
                    QColor strokeColor = parseColor(value);
                    if (strokeColor.isValid()) {
                        QPen pen = shape->strokePen();
                        pen.setColor(strokeColor);
                        shape->setStrokePen(pen);
                    }
                }
            } else if (key == "stroke-width") {
                qreal width = parseLength(value);
                if (width > 0) {
                    QPen pen = shape->strokePen();
                    pen.setWidthF(width);
                    shape->setStrokePen(pen);
                }
            } else if (key == "fill") {
                if (value == "none") {
                    shape->setFillBrush(Qt::NoBrush);
                } else {
                    QColor fillColor = parseColor(value);
                    if (fillColor.isValid()) {
                        shape->setFillBrush(fillColor);
                    }
                }
            } else if (key == "opacity") {
                qreal opacity = value.toDouble();
                shape->setOpacity(opacity);
            }
        }
    }
}

// 从流式解析的元素解析SVG文档
bool SvgStreamHandler::parseSvgDocumentFromElement(DrawingScene *scene, const SvgStreamElement &rootElement)
{
    if (rootElement.tagName != "svg")
    {
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

    // 批量处理渐变定义 - 使用SvgHandler的静态函数
    for (const QDomElement &gradient : collected.linearGradients) {
        QString id = gradient.attribute("id");
        if (!id.isEmpty()) {
            QLinearGradient linearGradient = SvgHandler::parseLinearGradient(gradient);
            s_gradients[id] = linearGradient;
        }
    }
    for (const QDomElement &gradient : collected.radialGradients) {
        QString id = gradient.attribute("id");
        if (!id.isEmpty()) {
            QRadialGradient radialGradient = SvgHandler::parseRadialGradient(gradient);
            s_gradients[id] = radialGradient;
        }
    }

    // 清理之前的滤镜定义
    s_filters.clear();

    // 批量处理滤镜定义 - 使用SvgHandler的静态函数
    for (const QDomElement &filter : collected.gaussianBlurFilters) {
        QString id = filter.attribute("filter-id"); // 使用我们设置的filter-id属性
        if (!id.isEmpty()) {
            QGraphicsBlurEffect *blurEffect = SvgHandler::parseGaussianBlurFilter(filter);
            if (blurEffect) {
                s_filters[id] = blurEffect;
            }
        }
    }
    for (const QDomElement &filter : collected.dropShadowFilters) {
        QString id = filter.attribute("filter-id"); // 使用我们设置的filter-id属性
        if (!id.isEmpty()) {
            QGraphicsDropShadowEffect *shadowEffect = SvgHandler::parseDropShadowFilter(filter);
            if (shadowEffect) {
                s_filters[id] = shadowEffect;
            }
        }
    }

    // 清理之前的图案定义
    s_patterns.clear();

    // 批量处理图案定义 - 使用SvgHandler的静态函数
    for (const QDomElement &pattern : collected.patterns) {
        QString id = pattern.attribute("id");
        if (!id.isEmpty()) {
            QBrush patternBrush = SvgHandler::parsePatternBrush(pattern);
            s_patterns[id] = patternBrush;
        }
    }

    // 清理之前的标记定义
    s_markers.clear();
    s_markerDataCache.clear();

    // 批量处理标记定义 - 使用SvgHandler的静态函数
    for (const QDomElement &marker : collected.markers) {
        QString id = marker.attribute("id");
        if (!id.isEmpty()) {
            s_markers[id] = marker;
            MarkerData markerData = SvgHandler::parseMarkerData(marker);
            s_markerDataCache[id] = markerData;
        }
    }

    // 使用收集到的元素创建图形对象
    int elementCount = 0;
    
    qDebug() << "=== SVG元素收集结果 ===";
    qDebug() << "Groups:" << collected.groups.size();
    qDebug() << "Rectangles:" << collected.rectangles.size();
    qDebug() << "Circles:" << collected.circles.size();
    qDebug() << "Paths:" << collected.paths.size();
    qDebug() << "Texts:" << collected.texts.size();
    
    // 首先处理group和layer元素，它们包含子元素
    for (const QDomElement &element : collected.groups) {
        qDebug() << "处理group:" << element.attribute("transform");
        DrawingGroup *shape = parseGroupElement(scene, element);
        qDebug() << "Group结果:" << shape << "子项:" << (shape ? shape->childItems().count() : 0);
        if (shape) {
            scene->addItem(shape);
            shape->setZValue(elementCount++);
            qDebug() << "Group已添加到scene";
        }
    }
    
    // 处理图层元素（包括嵌套的处理）
    for (const QDomElement &element : collected.layers) {
        DrawingGroup *group = parseGroupElement(scene, element);
        if (group) {
            elementCount++;
        }
    }
    
    // 然后处理不属于任何group的顶级图形元素
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

// 以下函数将从原SvgHandler复制过来，确保功能一致性
// TODO: 逐步实现这些函数的复制和适配

// 使用SvgTransformManager的变换函数
QTransform SvgStreamHandler::parseTransform(const QString &transformStr) 
{ 
    return SvgTransformManager::parseTransform(transformStr); 
}

QTransform SvgStreamHandler::parseAdjustedTransform(const QString &transformStr, const QPointF &shapePos) 
{ 
    Q_UNUSED(shapePos); // 简化处理，不再需要特殊的位置调整
    return SvgTransformManager::parseTransform(transformStr); 
}

void SvgStreamHandler::parseTransformAttribute(DrawingShape *shape, const QString &transformStr) 
{ 
    SvgTransformManager::applyTransformToShape(shape, transformStr); 
}

// 基础图形解析函数实现
DrawingRectangle* SvgStreamHandler::parseRectElement(const QDomElement &element)
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
    rect->setPos(x, y);

    // 解析样式属性
    parseStyleAttributes(rect, element);

    // 解析变换属性
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        parseTransformAttribute(rect, transform);
    }

    return rect;
}

DrawingEllipse* SvgStreamHandler::parseCircleElement(const QDomElement &element)
{
    qreal cx = element.attribute("cx", "0").toDouble();
    qreal cy = element.attribute("cy", "0").toDouble();
    qreal r = element.attribute("r", "0").toDouble();

    if (r <= 0)
    {
        return nullptr;
    }

    QRectF rect(0, 0, 2 * r, 2 * r);
    DrawingEllipse *circle = new DrawingEllipse(rect);
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

DrawingEllipse* SvgStreamHandler::parseEllipseElement(const QDomElement &element)
{
    qreal cx = element.attribute("cx", "0").toDouble();
    qreal cy = element.attribute("cy", "0").toDouble();
    qreal rx = element.attribute("rx", "0").toDouble();
    qreal ry = element.attribute("ry", "0").toDouble();

    if (rx <= 0 || ry <= 0)
    {
        return nullptr;
    }

    QRectF rect(-rx, -ry, 2 * rx, 2 * ry);
    DrawingEllipse *ellipse = new DrawingEllipse(rect);
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

DrawingPath* SvgStreamHandler::parseLineElement(const QDomElement &element)
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

DrawingPath* SvgStreamHandler::parsePolygonElement(const QDomElement &element)
{
    QString tagName = element.tagName();
    QString pointsStr = element.attribute("points");
    if (pointsStr.isEmpty())
    {
        return nullptr;
    }

    QPainterPath path;
    parseSvgPointsData(pointsStr, path);

    if (tagName == "polygon")
    {
        path.closeSubpath();
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

// 其他函数继续使用SvgHandler的实现
DrawingShape* SvgStreamHandler::parseSvgElement(const QDomElement &element)
{
    QString tagName = element.tagName();
    
    if (tagName == "path")
    {
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
    else if (tagName == "polyline")
    {
        return parsePolygonElement(element); // polyline和polygon使用相同的解析
    }
    else if (tagName == "polygon")
    {
        return parsePolygonElement(element);
    }
    else if (tagName == "text")
    {
        return parseTextElement(element);
    }
    else if (tagName == "use")
    {
        return parseUseElement(element);
    }
    
    return nullptr;
}
DrawingPath* SvgStreamHandler::parsePathElement(const QDomElement &element)
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
DrawingText* SvgStreamHandler::parseTextElement(const QDomElement &element)
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
    
    // 对于SVG文本，y坐标指定的是基线位置，需要调整
    // 先设置一个临时位置来获取字体度量
    QFont tempFont("Arial", 12); // 临时字体，稍后会被正确设置
    QFontMetricsF metrics(tempFont);
    
    // SVG的y坐标是基线位置，Qt的drawText也是基于基线的
    // 但考虑到可能的字体大小差异，我们先按原位置设置
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
        // 获取字体度量信息来计算文本宽度
        QFontMetricsF metrics(font);
        qreal textWidth = metrics.horizontalAdvance(text);
        QPointF adjustedPos = position - QPointF(textWidth / 2, 0);
        shape->setPos(adjustedPos);
        
    } else if (textAnchor == "end") {
        // 右对齐
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
DrawingGroup* SvgStreamHandler::parseGroupElement(DrawingScene *scene, const QDomElement &groupElement)
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
                if (nestedGroup && group) {
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
        DrawingGroup *tempGroup = new DrawingGroup();
        tempGroup->setVisible(false); // 设置为不可见，因为它只是一个占位符
        return tempGroup;
    }
    
    return group;
}
DrawingLayer* SvgStreamHandler::parseLayerElement(const QDomElement &element) { return nullptr; }
DrawingShape* SvgStreamHandler::parseUseElement(const QDomElement &element)
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

    // 解析并应用变换（必须先于位置设置）
    QString transform = element.attribute("transform");
    if (!transform.isEmpty())
    {
        // 调整变换中的旋转中心，因为SVG中的旋转中心是绝对坐标
        // 需要转换为相对于元素位置的坐标
        QString adjustedTransform = SvgHandler::adjustTransformForUseElement(transform, -x, -y);
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

void SvgStreamHandler::parseStyleAttributes(DrawingShape *shape, const QDomElement &element)
{
    // 解析stroke属性
    QString stroke = element.attribute("stroke");
    if (!stroke.isEmpty())
    {
        if (stroke == "none")
        {
            shape->setStrokePen(Qt::NoPen);
        }
        else
        {
            QColor strokeColor = SvgColorParser::parseColor(stroke);
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
        qreal width = SvgLengthParser::parseLength(strokeWidth);
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
                QColor fillColor = SvgColorParser::parseColor(fill);
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

void SvgStreamHandler::parseStyleAttributes(DrawingGroup *group, const QDomElement &element)
{
    // 组元素的样式解析逻辑
    parseStyleAttributes(static_cast<DrawingShape*>(group), element);
}

void SvgStreamHandler::applyStyleToShape(DrawingShape *shape, const QString &style) 
{ 
    Q_UNUSED(shape); 
    Q_UNUSED(style); 
    // 暂时不实现style属性解析，优先使用单独的属性
}

QColor SvgStreamHandler::parseColor(const QString &colorStr) 
{ 
    return SvgColorParser::parseColor(colorStr); 
}

qreal SvgStreamHandler::parseLength(const QString &lengthStr) 
{ 
    return SvgLengthParser::parseLength(lengthStr); 
}

void SvgStreamHandler::parseSvgPointsData(const QString &pointsStr, QPainterPath &path, bool closePath) 
{ 
    SvgHandler::parseSvgPointsData(pointsStr, path, closePath); 
}

void SvgStreamHandler::parseSvgPathData(const QString &data, QPainterPath &path) 
{ 
    SvgHandler::parseSvgPathData(data, path); 
}

void SvgStreamHandler::applyMarkers(DrawingPath *path, const QString &markerStart, const QString &markerMid, const QString &markerEnd) 
{ 
    SvgHandler::applyMarkers(path, markerStart, markerMid, markerEnd); 
}

QLinearGradient SvgStreamHandler::parseLinearGradient(const QDomElement &element)
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
QRadialGradient SvgStreamHandler::parseRadialGradient(const QDomElement &element)
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

    QRadialGradient gradient(QPointF(cx, cy), r, QPointF(fx, fy));

    // 解析渐变停止点
    parseGradientStops(&gradient, element);

    return gradient;
}
void SvgStreamHandler::parseGradientStops(QGradient *gradient, const QDomElement &element)
{
    QDomNodeList stops = element.elementsByTagName("stop");

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

        // 解析透明度
        qreal opacity = stopOpacity.toDouble();
        color.setAlphaF(opacity);

        gradient->setColorAt(offset, color);
    }
}

QGraphicsBlurEffect* SvgStreamHandler::parseGaussianBlurFilter(const QDomElement &element) { Q_UNUSED(element); return nullptr; }
QGraphicsDropShadowEffect* SvgStreamHandler::parseDropShadowFilter(const QDomElement &element) { Q_UNUSED(element); return nullptr; }
void SvgStreamHandler::applyFilterToShape(DrawingShape *shape, const QString &filterId) { Q_UNUSED(shape); Q_UNUSED(filterId); }
void SvgStreamHandler::applyFilterToShape(DrawingGroup *group, const QString &filterId) { Q_UNUSED(group); Q_UNUSED(filterId); }

QBrush SvgStreamHandler::parsePatternBrush(const QDomElement &patternElement) { Q_UNUSED(patternElement); return QBrush(); }

MarkerData SvgStreamHandler::parseMarkerData(const QDomElement &markerElement) { Q_UNUSED(markerElement); return MarkerData(); }


void SvgStreamHandler::convertEllipticalArcToBezier(QPainterPath &path, const QPointF &start, const QPointF &end, 
                                                   qreal rx, qreal ry, qreal xAxisRotation, 
                                                   bool largeArcFlag, bool sweepFlag) 
{ 
    Q_UNUSED(path); Q_UNUSED(start); Q_UNUSED(end); Q_UNUSED(rx); Q_UNUSED(ry); 
    Q_UNUSED(xAxisRotation); Q_UNUSED(largeArcFlag); Q_UNUSED(sweepFlag); 
}