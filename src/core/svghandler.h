#ifndef SVGHANDLER_H
#define SVGHANDLER_H

#include <QPainterPath>
#include <QGraphicsItem>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QGradient>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QDomDocument>
#include <QDomElement>
#include <QString>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QHash>
#include <QXmlStreamReader>
#include "svgelementcollector.h"

// 前向声明
struct MarkerData;
class FastPathParser;

// 流式SVG解析器元素结构
struct SvgStreamElement {
    QString tagName;
    QHash<QString, QString> attributes;
    QString text;
    QList<SvgStreamElement> children;
};

// 流式SVG解析器类
class SvgStreamParser
{
public:
    static bool parseSvgFile(const QString &fileName, SvgStreamElement &rootElement);
private:
    static SvgStreamElement parseElement(QXmlStreamReader &reader);
};



// 全局存储 - 声明为extern以便其他文件访问
extern QHash<QString, QGradient> s_gradients;
extern QHash<QString, QGraphicsEffect *> s_filters;
extern QHash<QString, QBrush> s_patterns;
extern QHash<QString, QDomElement> s_markers;
extern QHash<QString, MarkerData> s_markerDataCache;
extern QHash<QString, QDomElement> s_definedElements;

class DrawingScene;
class DrawingShape;
class DrawingPath;
class DrawingRectangle;
class DrawingEllipse;
class DrawingText;
class DrawingLayer;
class DrawingGroup;
class DrawingLine;
class DrawingPolyline;
class DrawingPolygon;

/**
 * SVG处理类 - 负责导入和导出SVG文件
 */
/**
 * SVG元数据结构
 */
struct SvgMetadata {
    QSizeF size;                    // SVG的width/height
    QRectF viewBox;                 // viewBox属性
    QString preserveAspectRatio;    // preserveAspectRatio属性
    bool hasViewBox;                // 是否有viewBox
    bool hasSize;                   // 是否有明确的width/height
    
    SvgMetadata() : hasViewBox(false), hasSize(false) {}
};

/**
 * SVG处理类 - 负责导入和导出SVG文件
 */
class SvgHandler
{
public:
    // 从SVG文件导入
    static bool importFromSvg(DrawingScene *scene, const QString &fileName);
    
    // 导出到SVG文件
    static bool exportToSvg(DrawingScene *scene, const QString &fileName);
    
    // 公共的解析函数，供SvgStreamHandler使用
    static DrawingPath* parsePathElement(const QDomElement &element);
    static DrawingRectangle* parseRectElement(const QDomElement &element);
    static DrawingEllipse* parseEllipseElement(const QDomElement &element);
    static DrawingEllipse* parseCircleElement(const QDomElement &element);
    static DrawingPath* parseLineElement(const QDomElement &element);
    static DrawingPath* parsePolygonElement(const QDomElement &element);
    static DrawingText* parseTextElement(const QDomElement &element);
    static DrawingGroup* parseGroupElement(DrawingScene *scene, const QDomElement &groupElement);
    static DrawingShape* parseUseElement(const QDomElement &element);
    
    // 调整use元素的变换，考虑位置偏移
    static QString adjustTransformForUseElement(const QString &transformStr, qreal x, qreal y);
    
    static QLinearGradient parseLinearGradient(const QDomElement &element);
    static QRadialGradient parseRadialGradient(const QDomElement &element);
    static QGraphicsBlurEffect* parseGaussianBlurFilter(const QDomElement &element);
    static QGraphicsDropShadowEffect* parseDropShadowFilter(const QDomElement &element);
    static QBrush parsePatternBrush(const QDomElement &patternElement);
    static MarkerData parseMarkerData(const QDomElement &markerElement);
    
    // 辅助解析函数 - 供SvgStreamHandler使用
    static QColor parseColor(const QString &colorStr);
    static qreal parseLength(const QString &lengthStr);
    static void parseSvgPointsData(const QString &pointsStr, QPainterPath &path, bool closePath = true);
    static void parseSvgPathData(const QString &data, QPainterPath &path);
    static void applyMarkers(DrawingPath *path, const QString &markerStart, const QString &markerMid, const QString &markerEnd);
    
private:
    // 解析SVG元数据
    static SvgMetadata parseSvgMetadata(const QDomElement &svgElement);
    
    // 从流式解析的元素解析SVG元数据
    static SvgMetadata parseSvgMetadataFromElement(const SvgStreamElement &svgElement);
    
    // 计算SVG到Scene的变换矩阵
    static QTransform calculateSvgToSceneTransform(const SvgMetadata &metadata);
    
    // 应用SVG设置到Scene
    static void applySvgSettingsToScene(DrawingScene *scene, const SvgMetadata &metadata);
    
    // 从QPainterPath创建DrawingPath对象
    static DrawingPath* createPathFromPainterPath(const QPainterPath &path, const QString &elementId = QString());

private:
    // 解析SVG文档
    static bool parseSvgDocument(DrawingScene *scene, const QDomDocument &doc);
    
    // 从流式解析的元素解析SVG文档
    static bool parseSvgDocumentFromElement(DrawingScene *scene, const SvgStreamElement &rootElement);
    
    // 解析SVG元素
    static DrawingShape* parseSvgElement(const QDomElement &element);
    
    // 收集所有有id的元素（用于use元素）
    static void collectDefinedElements(const QDomElement &parent);
    
    // 解析变换字符串为QTransform
    static QTransform parseTransform(const QString &transformStr);
    static QTransform parseAdjustedTransform(const QString &transformStr, const QPointF &shapePos);
    
    // 应用样式到图形
    static void applyStyleToShape(DrawingShape *shape, const QString &style);
    
    // 解析 Inkscape sodipodi:arc 元素
    static DrawingEllipse* parseSodipodiArcElement(const QDomElement &element);
    
    // 流式解析相关函数
    static DrawingShape* parseSvgElementFromStream(const SvgStreamElement &element);
    static DrawingPath* parsePathElementFromStream(const SvgStreamElement &element);
    static DrawingRectangle* parseRectElementFromStream(const SvgStreamElement &element);
    static DrawingEllipse* parseCircleElementFromStream(const SvgStreamElement &element);
    static DrawingEllipse* parseEllipseElementFromStream(const SvgStreamElement &element);
    static DrawingPath* parseLineElementFromStream(const SvgStreamElement &element);
    static DrawingPath* parsePolygonElementFromStream(const SvgStreamElement &element);
    static DrawingText* parseTextElementFromStream(const SvgStreamElement &element);
    static DrawingGroup* parseGroupElementFromStream(const SvgStreamElement &element);
    static DrawingShape* parseUseElementFromStream(const SvgStreamElement &element);
    static void parseStyleAttributesFromStream(DrawingShape *shape, const SvgStreamElement &element);
    
    // 流式解析辅助函数
    static SvgElementCollector::CollectedElements collectElementsFromStream(const SvgStreamElement &rootElement);
    static void collectElementsFromStreamRecursive(const SvgStreamElement &element, 
                                                   SvgElementCollector::CollectedElements &collected, 
                                                   bool isInDefs);
    
    // 解析多线元素
    static DrawingPath* parsePolylineElement(const QDomElement &element);
    
    // 递归收集文本内容，包括tspan元素
    static QString collectTextContent(const QDomElement &element);
    
    // 解析简单文本元素（不含tspan）
    static DrawingText* parseSimpleTextElement(const QDomElement &element);
    
    // 解析包含tspan的文本元素
    static DrawingText* parseTextElementWithTspan(const QDomElement &element);
    
    // 解析组元素（现在支持图层）
    static DrawingLayer* parseLayerElement(const QDomElement &element);
    
    // 解析样式属性
    static void parseStyleAttributes(DrawingShape *shape, const QDomElement &element);
    static void parseStyleAttributes(DrawingGroup *group, const QDomElement &element);
    
    // 解析变换属性
    static void parseTransformAttribute(DrawingShape *shape, const QString &transformStr);
    
    
    // 解析defs元素中的渐变定义
    static void parseDefsElements(const QDomElement &root);
    
    // 解析渐变停止点
    static void parseGradientStops(QGradient *gradient, const QDomElement &element);
    
    // 解析滤镜效果
    static void parseFilterElements(const QDomElement &root);
    static void applyFilterToShape(DrawingShape *shape, const QString &filterId);
    static void applyFilterToShape(DrawingGroup *group, const QString &filterId);
    
    // 解析Pattern
    static void parsePatternElements(const QDomElement &root);
    static QBrush parsePatternBrush(const QString &patternId);
    
    // 解析Marker
    static void renderMarkerToCache(const QString &id, const QDomElement &markerElement);
    static QPainterPath createMarkerPath(const QString &markerId, const QPointF &startPoint, const QPointF &endPoint);
    // 应用Marker到路径
    static void applyMarkerToPath(DrawingPath *path, const QString &markerId, const QString &position = "end");
    
    // 导出场景到SVG文档
    static QDomDocument exportSceneToSvgDocument(DrawingScene *scene);
    
    // 导出形状到SVG元素
    static QDomElement exportShapeToSvgElement(QDomDocument &doc, DrawingShape *shape);
    
    // 导出路径到SVG路径元素
    static QDomElement exportPathToSvgElement(QDomDocument &doc, DrawingPath *path);
    
    // 导出矩形到SVG矩形元素
    static QDomElement exportRectangleToSvgElement(QDomDocument &doc, DrawingRectangle *rect);
    
    // 导出椭圆到SVG椭圆元素
    static QDomElement exportEllipseToSvgElement(QDomDocument &doc, DrawingEllipse *ellipse);
    
    // 导出文本到SVG文本元素
    static QDomElement exportTextToSvgElement(QDomDocument &doc, DrawingText *text);
    
    // 导出线条到SVG线条元素
    static QDomElement exportLineToSvgElement(QDomDocument &doc, DrawingLine *line);
    
    // 导出折线到SVG折线元素
    static QDomElement exportPolylineToSvgElement(QDomDocument &doc, DrawingPolyline *polyline);
    
    // 导出多边形到SVG多边形元素
    static QDomElement exportPolygonToSvgElement(QDomDocument &doc, DrawingPolygon *polygon);
    
    // 辅助函数
    static QString pathDataToString(const QPainterPath &path);
    static void parseGroupElement(const QDomElement &groupElement);
    
    // 椭圆弧转换函数
    static void convertEllipticalArcToBezier(QPainterPath &path, const QPointF &start, const QPointF &end, 
                                           qreal rx, qreal ry, qreal xAxisRotation, 
                                           bool largeArcFlag, bool sweepFlag);
    
    // 导出辅助函数
    static QDomElement exportLayerToSvgElement(QDomDocument &doc, DrawingLayer *layer);
    static void exportGradientsToSvg(QDomDocument &doc, QDomElement &defsElement, const QList<QGraphicsItem*> &items);
    static void exportFiltersToSvg(QDomDocument &doc, QDomElement &defsElement, const QList<QGraphicsItem*> &items);
    static QString transformToString(const QTransform &transform);
};

#endif // SVGHANDLER_H