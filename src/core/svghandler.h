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

// Marker存储 - 声明为extern以便其他文件访问
extern QHash<QString, QDomElement> s_markers;

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
class SvgHandler
{
public:
    // 从SVG文件导入
    static bool importFromSvg(DrawingScene *scene, const QString &fileName);
    
    // 导出到SVG文件
    static bool exportToSvg(DrawingScene *scene, const QString &fileName);
    
    // 从QPainterPath创建DrawingPath对象
    static DrawingPath* createPathFromPainterPath(const QPainterPath &path, const QString &elementId = QString());

private:
    // 解析SVG文档
    static bool parseSvgDocument(DrawingScene *scene, const QDomDocument &doc);
    
    // 解析SVG元素
    static DrawingShape* parseSvgElement(const QDomElement &element);
    
    // 收集所有有id的元素（用于use元素）
    static void collectDefinedElements(const QDomElement &parent);
    
    // 解析use元素
    static DrawingShape* parseUseElement(const QDomElement &element);
    
    // 调整use元素的变换，考虑位置偏移
    static QString adjustTransformForUseElement(const QString &transformStr, qreal x, qreal y);
    
    // 解析变换字符串为QTransform
    static QTransform parseTransform(const QString &transformStr);
    
    // 应用样式到图形
    static void applyStyleToShape(DrawingShape *shape, const QString &style);
    
    // 解析路径元素
    static DrawingPath* parsePathElement(const QDomElement &element);
    
    // 解析 Inkscape sodipodi:arc 元素
    static DrawingEllipse* parseSodipodiArcElement(const QDomElement &element);
    
    // 解析矩形元素
    static DrawingRectangle* parseRectElement(const QDomElement &element);
    
    // 解析椭圆元素
    static DrawingEllipse* parseEllipseElement(const QDomElement &element);
    
    // 解析圆元素
    static DrawingEllipse* parseCircleElement(const QDomElement &element);
    
    // 解析线元素
    static DrawingPath* parseLineElement(const QDomElement &element);
    
    // 解析多边形元素
    static DrawingPath* parsePolygonElement(const QDomElement &element);
    
    // 解析文本元素
    static DrawingText* parseTextElement(const QDomElement &element);
    
    // 解析组元素（现在支持图层）
    static DrawingGroup* parseGroupElement(DrawingScene *scene, const QDomElement &groupElement);
    static DrawingLayer* parseLayerElement(const QDomElement &element);
    
    // 解析多线元素
    static DrawingPath* parsePolylineElement(const QDomElement &element);
    
    // 解析样式属性
    static void parseStyleAttributes(DrawingShape *shape, const QDomElement &element);
    static void parseStyleAttributes(DrawingGroup *group, const QDomElement &element);
    
    // 解析变换属性
    static void parseTransformAttribute(DrawingShape *shape, const QString &transformStr);
    
    
    // 解析颜色字符串
    static QColor parseColor(const QString &colorStr);
    
    // 解析defs元素中的渐变定义
    static void parseDefsElements(const QDomElement &root);
    
    // 解析线性渐变
    static QLinearGradient parseLinearGradient(const QDomElement &element);
    
    // 解析径向渐变
    static QRadialGradient parseRadialGradient(const QDomElement &element);
    
    // 解析渐变停止点
    static void parseGradientStops(QGradient *gradient, const QDomElement &element);
    
    // 解析滤镜效果
    static void parseFilterElements(const QDomElement &root);
    static QGraphicsBlurEffect* parseGaussianBlurFilter(const QDomElement &element);
    static QGraphicsDropShadowEffect* parseDropShadowFilter(const QDomElement &element);
    static void applyFilterToShape(DrawingShape *shape, const QString &filterId);
    static void applyFilterToShape(DrawingGroup *group, const QString &filterId);
    
    // 解析Pattern
    static void parsePatternElements(const QDomElement &root);
    static QBrush parsePatternBrush(const QDomElement &patternElement);
    static QBrush parsePatternBrush(const QString &patternId);
    
    // 解析Marker
    static void renderMarkerToCache(const QString &id, const QDomElement &markerElement);
    static QPainterPath createMarkerPath(const QString &markerId, const QPointF &startPoint, const QPointF &endPoint);
    // 应用Marker到路径
    static void applyMarkers(DrawingPath *path, const QString &markerStart, const QString &markerMid, const QString &markerEnd);
    static void applyMarkerToPath(DrawingPath *path, const QString &markerId, const QString &position = "end");
    
    // 从字符串解析长度值
    static qreal parseLength(const QString &lengthStr);
    
    // 从字符串解析点数据
    static void parseSvgPointsData(const QString &pointsStr, QPainterPath &path, bool closePath = true);
    
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
    static void parseSvgPathData(const QString &data, QPainterPath &path);
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