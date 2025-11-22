#include "../ui/cursor-manager.h"
#include <QWidget>
#include <QApplication>
#include <QHash>
#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <QtMath>

CursorManager& CursorManager::instance()
{
    static CursorManager instance;
    return instance;
}

CursorManager::CursorManager()
    : m_initialized(false)
{
    createCursors();
}

QCursor CursorManager::getCursor(CursorType type)
{
    if (!m_initialized) {
        createCursors();
    }
    
    return m_cursors.value(type, Qt::ArrowCursor);
}

void CursorManager::setCursorForView(QWidget *view, CursorType type)
{
    if (view) {
        view->setCursor(getCursor(type));
    }
}

void CursorManager::createCursors()
{
    if (m_initialized) {
        return;
    }
    
    int cursorSize = 32;
    int hotSpotX = cursorSize / 4; // 十字线中心位置（左上角）
    int hotSpotY = cursorSize / 4;
    
    // 创建各种工具的光标，设置正确的热点
    m_cursors[SelectCursor] = QCursor(Qt::ArrowCursor);
    m_cursors[RectangleCursor] = QCursor(createCrosshairWithShape(RectangleCursor), hotSpotX, hotSpotY);
    m_cursors[EllipseCursor] = QCursor(createCrosshairWithShape(EllipseCursor), hotSpotX, hotSpotY);
    m_cursors[LineCursor] = QCursor(createCrosshairWithShape(LineCursor), hotSpotX, hotSpotY);
    m_cursors[BezierCursor] = QCursor(createCrosshairWithShape(BezierCursor), hotSpotX, hotSpotY);
    m_cursors[PolygonCursor] = QCursor(createCrosshairWithShape(PolygonCursor), hotSpotX, hotSpotY);
    m_cursors[PolylineCursor] = QCursor(createCrosshairWithShape(PolylineCursor), hotSpotX, hotSpotY);
    m_cursors[BrushCursor] = QCursor(Qt::CrossCursor);
    m_cursors[FillCursor] = QCursor(Qt::PointingHandCursor);
    m_cursors[NodeEditCursor] = QCursor(Qt::CrossCursor);
    m_cursors[PathEditCursor] = QCursor(createCrosshairWithShape(PathEditCursor), hotSpotX, hotSpotY);
    m_cursors[DefaultCursor] = QCursor(Qt::ArrowCursor);
    
    m_initialized = true;
}

QPixmap CursorManager::createCrosshairCursor(int size)
{
    // 创建透明背景的图像
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    // 先关闭抗锯齿绘制十字线
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    // 设置细十字线样式
    QPen pen(Qt::black, 1);
    pen.setCosmetic(true); // 确保线宽不受变换影响
    painter.setPen(pen);
    
    int center = size / 2;
    int crossSize = size / 3; // 十字线更小
    
    // 绘制细十字线（在左上角区域）
    painter.drawLine(center - crossSize/2, center, center + crossSize/2, center);        // 水平线
    painter.drawLine(center, center - crossSize/2, center, center + crossSize/2);        // 垂直线
    
    // 在十字中心绘制一个小点
    painter.setPen(QPen(Qt::black, 1));
    painter.drawPoint(center, center);
    
    return pixmap;
}

QPixmap CursorManager::createCrosshairWithShape(CursorType type, int size)
{
    // 创建透明背景的图像
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    // 重新开启抗锯齿，看看效果
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    int crossSize = size / 4; // 十字线大小
    int shapeSize = size / 2; // 放大图形尺寸
    
    // 绘制细十字线（在左上角区域）
    QPen crossPen(Qt::black, 1);
    crossPen.setCosmetic(true);
    painter.setPen(crossPen);
    
    // 十字线在左上角区域
    int crossCenterX = crossSize;
    int crossCenterY = crossSize;
    painter.drawLine(crossCenterX - crossSize/2, crossCenterY, crossCenterX + crossSize/2, crossCenterY);
    painter.drawLine(crossCenterX, crossCenterY - crossSize/2, crossCenterX, crossCenterY + crossSize/2);
    
    // 十字中心点
    painter.drawPoint(crossCenterX, crossCenterY);
    
    // 形状在右下角区域，使用更多空间
    int shapeCenterX = size - shapeSize/2 - 2;
    int shapeCenterY = size - shapeSize/2 - 2;
    
    // 为图形部分开启抗锯齿
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // 设置形状画笔样式
    QPen pen(Qt::black, 1);
    pen.setCosmetic(true);
    painter.setPen(pen);
    
    // 根据工具类型设置不同的填充颜色，将在switch语句中设置
    painter.setBrush(Qt::NoBrush); // 默认无填充
    
    // 根据工具类型绘制不同的形状
    switch (type) {
    case RectangleCursor:
        // 绘制矩形，蓝色填充
        painter.setBrush(QBrush(QColor(100, 150, 255, 100))); // 半透明蓝色
        painter.drawRect(shapeCenterX - shapeSize/2, shapeCenterY - shapeSize/2, shapeSize, shapeSize);
        break;
        
    case EllipseCursor:
        // 绘制椭圆，绿色填充
        painter.setBrush(QBrush(QColor(100, 255, 150, 100))); // 半透明绿色
        painter.drawEllipse(QPoint(shapeCenterX, shapeCenterY), shapeSize/2, shapeSize/2);
        break;
        
    case LineCursor:
        // 绘制线条
        painter.drawLine(shapeCenterX - shapeSize/2, shapeCenterY, shapeCenterX + shapeSize/2, shapeCenterY);
        break;
        
    case BezierCursor:
        // 绘制曲线（贝塞尔曲线）
        {
            QPainterPath path;
            path.moveTo(shapeCenterX - shapeSize/2, shapeCenterY);
            path.cubicTo(shapeCenterX - shapeSize/4, shapeCenterY - shapeSize/4,
                         shapeCenterX + shapeSize/4, shapeCenterY + shapeSize/4,
                         shapeCenterX + shapeSize/2, shapeCenterY);
            painter.drawPath(path);
        }
        break;
        
    case PolygonCursor:
        // 绘制多边形（五边形），橙色填充
        {
            painter.setBrush(QBrush(QColor(255, 200, 100, 100))); // 半透明橙色
            QPolygonF pentagon;
            int radius = shapeSize/2;
            for (int i = 0; i < 5; ++i) {
                qreal angle = i * 2 * M_PI / 5 - M_PI/2; // 从顶部开始
                QPointF point(shapeCenterX + radius * qCos(angle),
                            shapeCenterY + radius * qSin(angle));
                pentagon << point;
            }
            painter.drawPolygon(pentagon);
        }
        break;
        
    case PolylineCursor:
        // 绘制折线，红色填充（虽然折线是开放的，但给端点添加小圆点）
        {
            painter.setBrush(QBrush(QColor(255, 100, 100, 100))); // 半透明红色
            QPolygonF polyline;
            polyline << QPointF(shapeCenterX - shapeSize/2, shapeCenterY - shapeSize/4)
                     << QPointF(shapeCenterX - shapeSize/4, shapeCenterY + shapeSize/4)
                     << QPointF(shapeCenterX + shapeSize/4, shapeCenterY - shapeSize/4)
                     << QPointF(shapeCenterX + shapeSize/2, shapeCenterY + shapeSize/4);
            painter.drawPolyline(polyline);
            
            // 在折线的端点绘制小圆点
            painter.setBrush(QBrush(QColor(255, 100, 100, 150)));
            painter.drawEllipse(polyline.first(), 2, 2);
            painter.drawEllipse(polyline.last(), 2, 2);
        }
        break;
        
    case BrushCursor:
        // 绘制画笔
        {
            // 画笔主体
            painter.drawLine(shapeCenterX, shapeCenterY + shapeSize/2, shapeCenterX, shapeCenterY - shapeSize/2);
            // 画笔尖端
            painter.drawEllipse(QPoint(shapeCenterX, shapeCenterY - shapeSize/2), 2, 2);
        }
        break;
        
    case PathEditCursor:
        // 绘制节点编辑器的方块
        painter.drawRect(shapeCenterX - 3, shapeCenterY - 3, 6, 6);
        break;
        
    case SelectCursor:
    case FillCursor:
    case NodeEditCursor:
    case DefaultCursor:
        // 这些光标使用系统默认光标，不需要绘制形状
        break;
        
    default:
        // 未知类型，绘制一个简单的十字
        break;
    }
    
    return pixmap;
}