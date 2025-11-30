#ifndef SVGELEMENTCOLLECTOR_H
#define SVGELEMENTCOLLECTOR_H

#include <QDomElement>
#include <QList>
#include <QHash>

/**
 * SVG元素收集器 - 单次遍历DOM树收集所有需要的元素
 * 优化DOM遍历性能，减少多次遍历的开销
 */
class SvgElementCollector
{
public:
    /**
     * 收集的元素结构
     */
    struct CollectedElements {
        // 定义元素（在defs中或具有id的元素）
        QHash<QString, QDomElement> definedElements;
        
        // 渐变元素
        QList<QDomElement> linearGradients;
        QList<QDomElement> radialGradients;
        
        // 滤镜元素
        QList<QDomElement> gaussianBlurFilters;
        QList<QDomElement> dropShadowFilters;
        
        // 图案元素
        QList<QDomElement> patterns;
        
        // 标记元素
        QList<QDomElement> markers;
        
        // 主要图形元素（按类型分组）
        QList<QDomElement> paths;
        QList<QDomElement> rectangles;
        QList<QDomElement> ellipses;
        QList<QDomElement> circles;
        QList<QDomElement> lines;
        QList<QDomElement> polylines;
        QList<QDomElement> polygons;
        QList<QDomElement> texts;
        QList<QDomElement> groups;
        QList<QDomElement> useElements;
        
        // 图层元素
        QList<QDomElement> layers;
    };

    /**
     * 从SVG根元素收集所有需要的元素
     * @param root SVG根元素
     * @return 收集的元素结构
     */
    static CollectedElements collect(const QDomElement &root);

private:
    /**
     * 递归收集元素
     * @param element 当前元素
     * @param collected 收集的结果
     * @param isInDefs 是否在defs元素中
     */
    static void collectRecursive(const QDomElement &element, 
                                CollectedElements &collected, 
                                bool isInDefs = false);

    /**
     * 判断元素是否为图层
     * @param element 元素
     * @return 是否为图层
     */
    static bool isLayerElement(const QDomElement &element);

    /**
     * 处理渐变元素
     * @param element 渐变元素
     * @param collected 收集的结果
     */
    static void processGradientElement(const QDomElement &element, 
                                     CollectedElements &collected);

    /**
     * 处理滤镜元素
     * @param element 滤镜元素
     * @param collected 收集的结果
     */
    static void processFilterElement(const QDomElement &element, 
                                   CollectedElements &collected);

    /**
     * 处理图形元素
     * @param element 图形元素
     * @param collected 收集的结果
     * @param isInDefs 是否在defs中
     */
    static void processShapeElement(const QDomElement &element, 
                                  CollectedElements &collected, 
                                  bool isInDefs);
};

#endif // SVGELEMENTCOLLECTOR_H