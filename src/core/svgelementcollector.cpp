#include <QDebug>
#include "svgelementcollector.h"

SvgElementCollector::CollectedElements SvgElementCollector::collect(const QDomElement &root)
{
    CollectedElements collected;
    collectRecursive(root, collected, false, false);
    return collected;
}

void SvgElementCollector::collectRecursive(const QDomElement &element, 
                                          CollectedElements &collected, 
                                          bool isInDefs,
                                          bool isInGroup)
{
    QString tagName = element.tagName();
    
    // 处理defs元素
    if (tagName == "defs") {
        isInDefs = true;
    }
    
    // 收集有id的元素（用于use元素引用）
    QString id = element.attribute("id");
    if (!id.isEmpty()) {
        collected.definedElements[id] = element.cloneNode().toElement();
    }
    
    // 根据元素类型分类收集
    if (tagName == "linearGradient") {
        collected.linearGradients.append(element);
    } else if (tagName == "radialGradient") {
        collected.radialGradients.append(element);
    } else if (tagName == "pattern") {
        collected.patterns.append(element);
    } else if (tagName == "marker") {
        collected.markers.append(element);
    } else if (tagName == "filter") {
        // 处理滤镜元素，收集其子元素并传递父元素的ID
        QString filterId = element.attribute("id");
        QDomNodeList children = element.childNodes();
        for (int i = 0; i < children.size(); ++i) {
            QDomNode child = children.at(i);
            if (child.isElement()) {
                QDomElement childElement = child.toElement();
                QString childTagName = childElement.tagName();
                
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
        }
    } else if (tagName == "path") {
        if (!isInDefs && !isInGroup) collected.paths.append(element);
    } else if (tagName == "rect") {
        if (!isInDefs && !isInGroup) collected.rectangles.append(element);
    } else if (tagName == "ellipse") {
        if (!isInDefs && !isInGroup) collected.ellipses.append(element);
    } else if (tagName == "circle") {
        if (!isInDefs && !isInGroup) collected.circles.append(element);
    } else if (tagName == "line") {
        if (!isInDefs && !isInGroup) collected.lines.append(element);
    } else if (tagName == "polyline") {
        if (!isInDefs && !isInGroup) collected.polylines.append(element);
    } else if (tagName == "polygon") {
        if (!isInDefs && !isInGroup) collected.polygons.append(element);
    } else if (tagName == "text") {
        if (!isInDefs && !isInGroup) collected.texts.append(element);
    } else if (tagName == "g") {
        if (isLayerElement(element)) {
            collected.layers.append(element);
        } else {
            if (!isInDefs) collected.groups.append(element);
        }
    } else if (tagName == "use") {
        if (!isInDefs && !isInGroup) collected.useElements.append(element);
    }
    
    // 递归处理子元素
    bool newIsInGroup = isInGroup || (tagName == "g");
    QDomNodeList children = element.childNodes();
    for (int i = 0; i < children.size(); ++i) {
        QDomNode child = children.at(i);
        if (child.isElement()) {
            collectRecursive(child.toElement(), collected, isInDefs, newIsInGroup);
        }
    }
}

bool SvgElementCollector::isLayerElement(const QDomElement &element)
{
    // 检查是否为Inkscape图层
    QString inkscapeLabel = element.attribute("inkscape:label");
    QString inkscapeGroupmode = element.attribute("inkscape:groupmode");
    
    // 检查是否为普通图层（有groupmode属性且值为layer）
    if (!inkscapeGroupmode.isEmpty() && inkscapeGroupmode == "layer") {
        return true;
    }
    
    // 检查是否为命名图层（有label属性且不为空）
    if (!inkscapeLabel.isEmpty()) {
        return true;
    }
    
    // 可以添加其他SVG编辑器的图层检测逻辑
    return false;
}

void SvgElementCollector::processGradientElement(const QDomElement &element, 
                                               CollectedElements &collected)
{
    QString tagName = element.tagName();
    if (tagName == "linearGradient") {
        collected.linearGradients.append(element);
    } else if (tagName == "radialGradient") {
        collected.radialGradients.append(element);
    }
}

void SvgElementCollector::processFilterElement(const QDomElement &element, 
                                             CollectedElements &collected)
{
    QString tagName = element.tagName();
    if (tagName == "feGaussianBlur") {
        collected.gaussianBlurFilters.append(element);
    } else if (tagName == "feDropShadow") {
        collected.dropShadowFilters.append(element);
    }
}

void SvgElementCollector::processShapeElement(const QDomElement &element, 
                                            CollectedElements &collected, 
                                            bool isInDefs)
{
    QString tagName = element.tagName();
    
    // 如果在defs中，只收集定义元素，不收集图形元素
    if (isInDefs) return;
    
    if (tagName == "path") {
        collected.paths.append(element);
    } else if (tagName == "rect") {
        collected.rectangles.append(element);
    } else if (tagName == "ellipse") {
        collected.ellipses.append(element);
    } else if (tagName == "circle") {
        collected.circles.append(element);
    } else if (tagName == "line") {
        collected.lines.append(element);
    } else if (tagName == "polyline") {
        collected.polylines.append(element);
    } else if (tagName == "polygon") {
        collected.polygons.append(element);
    } else if (tagName == "text") {
        collected.texts.append(element);
    } else if (tagName == "use") {
        collected.useElements.append(element);
    }
}