#ifndef SVGSTREAMHANDLER_H
#define SVGSTREAMHANDLER_H

#include <QPainterPath>
#include <QGraphicsItem>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QGradient>
#include <QLinearGradient>
#include <QRadialGradient>

#include <QString>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QHash>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include "svgelementcollector.h"
#include "svghandler.h"
#include "svgtransformmanager.h"
#include "svgcolorparser.h"
#include "svglengthparser.h"

// 前向声明
struct MarkerData;
class FastPathParser;
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

// 前向声明
struct SvgStreamElement;
struct SvgMetadata;
class SvgStreamParser;

/**
 * 流式SVG处理类 - 负责导入SVG文件，使用QXmlStreamReader进行流式解析
 */
class SvgStreamHandler
{
public:
    // 从SVG文件导入
    static bool importFromSvg(DrawingScene *scene, const QString &fileName);
    
private:
    // 解析SVG元数据
    static SvgMetadata parseSvgMetadataFromElement(const SvgStreamElement &svgElement);
    
    // 计算SVG到Scene的变换矩阵 - 使用SvgTransformManager
    static QTransform calculateSvgToSceneTransform(const SvgMetadata &metadata);
    
    // 应用SVG设置到Scene
    static void applySvgSettingsToScene(DrawingScene *scene, const SvgMetadata &metadata);
    
    // 真正的流式解析函数 - 使用栈结构
    static bool parseSvgStream(DrawingScene *scene, QXmlStreamReader &reader);
    
    // 从属性直接创建元素的辅助函数
    static DrawingRectangle* parseRectElementFromAttributes(const QXmlStreamAttributes &attributes);
    static DrawingEllipse* parseCircleElementFromAttributes(const QXmlStreamAttributes &attributes);
    static DrawingEllipse* parseEllipseElementFromAttributes(const QXmlStreamAttributes &attributes);
    static DrawingPath* parsePathElementFromAttributes(const QXmlStreamAttributes &attributes);
    static DrawingText* parseTextElementFromAttributes(const QXmlStreamReader &reader);
    
    // 从流式解析的元素解析SVG文档
    static bool parseSvgDocumentFromElement(DrawingScene *scene, const SvgStreamElement &rootElement);
    
    // 流式解析辅助函数
    static SvgElementCollector::CollectedElements collectElementsFromStream(const SvgStreamElement &rootElement);
    static void collectElementsFromStreamRecursive(const SvgStreamElement &element, 
                                                   SvgElementCollector::CollectedElements &collected, 
                                                   bool isInDefs,
                                                   bool isInGroup = false);
    
    // 解析SVG元素（复用DOM版本的函数）
    static DrawingShape* parseSvgElement(const QDomElement &element);
    static DrawingPath* parsePathElement(const QDomElement &element);
    static DrawingRectangle* parseRectElement(const QDomElement &element);
    static DrawingEllipse* parseEllipseElement(const QDomElement &element);
    static DrawingEllipse* parseCircleElement(const QDomElement &element);
    static DrawingPath* parseLineElement(const QDomElement &element);
    static DrawingPath* parsePolygonElement(const QDomElement &element);
    static DrawingText* parseTextElement(const QDomElement &element);
    static DrawingGroup* parseGroupElement(DrawingScene *scene, const QDomElement &groupElement);
    static DrawingLayer* parseLayerElement(const QDomElement &element);
    static DrawingShape* parseUseElement(const QDomElement &element);
    
    // 解析变换字符串为QTransform（复用DOM版本的函数）
    static QTransform parseTransform(const QString &transformStr);
    static QTransform parseAdjustedTransform(const QString &transformStr, const QPointF &shapePos);
    
    // 解析样式到图形（复用DOM版本的函数）
    static void applyStyleToShape(DrawingShape *shape, const QString &style);
    static void parseStyleAttributes(DrawingShape *shape, const QDomElement &element);
    static void parseStyleAttributes(DrawingGroup *group, const QDomElement &element);
    
    // 流式解析版本的样式解析
    static void parseStyleAttributes(DrawingShape *shape, const QXmlStreamAttributes &attributes);
    static void parseStyleAttributesFromStyleString(DrawingShape *shape, const QString &style);
    
    // 解析变换属性（复用DOM版本的函数）
    static void parseTransformAttribute(DrawingShape *shape, const QString &transformStr);
    
    // 解析颜色字符串（复用DOM版本的函数）
    static QColor parseColor(const QString &colorStr);
    
    // 解析长度值（复用DOM版本的函数）
    static qreal parseLength(const QString &lengthStr);
    
    // 解析渐变（复用DOM版本的函数）
    static QLinearGradient parseLinearGradient(const QDomElement &element);
    static QRadialGradient parseRadialGradient(const QDomElement &element);
    static void parseGradientStops(QGradient *gradient, const QDomElement &element);
    
    // 解析滤镜（复用DOM版本的函数）
    static QGraphicsBlurEffect* parseGaussianBlurFilter(const QDomElement &element);
    static QGraphicsDropShadowEffect* parseDropShadowFilter(const QDomElement &element);
    static void applyFilterToShape(DrawingShape *shape, const QString &filterId);
    static void applyFilterToShape(DrawingGroup *group, const QString &filterId);
    
    // 解析Pattern（复用DOM版本的函数）
    static QBrush parsePatternBrush(const QDomElement &patternElement);
    
    // 解析Marker（复用DOM版本的函数）
    static MarkerData parseMarkerData(const QDomElement &markerElement);
    static void applyMarkers(DrawingPath *path, const QString &markerStart, const QString &markerMid, const QString &markerEnd);
    
    // 辅助函数：从字符串解析点数据（复用DOM版本的函数）
    static void parseSvgPointsData(const QString &pointsStr, QPainterPath &path, bool closePath = true);
    
    // 辅助函数：解析路径数据（复用DOM版本的函数）
    static void parseSvgPathData(const QString &data, QPainterPath &path);
    
    // 椭圆弧转换函数（复用DOM版本的函数）
    static void convertEllipticalArcToBezier(QPainterPath &path, const QPointF &start, const QPointF &end, 
                                           qreal rx, qreal ry, qreal xAxisRotation, 
                                           bool largeArcFlag, bool sweepFlag);
};

#endif // SVGSTREAMHANDLER_H