#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPen>
#include <QDebug>
#include "../tools/drawing-tool-brush.h"
#include "../core/drawing-throttle.h"
#include "../core/brush-engine.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"

DrawingToolBrush::DrawingToolBrush(QObject *parent)
    : ToolBase(parent)
    , m_currentPath(nullptr)
    , m_throttle(nullptr)
    , m_brushWidth(2.0)
    , m_smoothness(0.5)
    , m_drawing(false)
{
    // 创建节流器（暂时保留，但暂不使用）
    m_throttle = new DrawingThrottle(this);
    m_throttle->setThrottleInterval(16);  // 60fps
    m_throttle->setDistanceThreshold(1.5);  // 1.5像素阈值
    m_throttle->setMaxPendingEvents(8);     // 最多缓存8个事件
}

void DrawingToolBrush::activate(DrawingScene *scene, DrawingView *view)
{
    ToolBase::activate(scene, view);
    m_currentPath = nullptr;
    m_points.clear();
    m_drawing = false;
    
    // 清除节流器的待处理事件
    if (m_throttle) {
        m_throttle->clearPendingEvents();
    }
}

void DrawingToolBrush::deactivate()
{
    // 处理所有待处理的事件
    if (m_throttle) {
        m_throttle->flushPendingEvents();
    }
    
    // 不要删除图形，只是清除引用
    // 图形应该保留在场景中供其他工具编辑
    m_currentPath = nullptr;
    
    ToolBase::deactivate();
}

bool DrawingToolBrush::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_scene) {
        m_drawing = true;
        m_points.clear();
        m_points.append(scenePos);
        m_lastPoint = scenePos;
        
        // 立即创建路径对象
        m_currentPath = new DrawingPath();
        QPainterPath path;
        path.moveTo(scenePos);
        m_currentPath->setPath(path);
        
        // 设置基本画笔样式
        QPen pen(Qt::black, m_brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        m_currentPath->setStrokePen(pen);
        m_currentPath->setFillBrush(Qt::NoBrush); // 确保绘制纯线条
        
        // 添加到活动图层
        LayerManager *layerManager = LayerManager::instance();
        DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
        
        if (activeLayer) {
            activeLayer->addShape(m_currentPath);
            qDebug() << "Added DrawingPath to active layer:" << activeLayer->name();
        } else {
            // 如果没有活动图层，直接添加到场景（向后兼容）
            m_scene->addItem(m_currentPath);
            qDebug() << "No active layer, added DrawingPath directly to scene";
        }
        
        m_currentPath->setVisible(true);
        m_currentPath->setFlag(QGraphicsItem::ItemIsSelectable, true);
        
        qDebug() << "Created initial DrawingPath at" << scenePos;
        
        return true;
    }
    
    return false;
}

bool DrawingToolBrush::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (m_drawing && m_currentPath && m_scene) {
        // 计算与上一个点的距离
        qreal distance = QLineF(m_lastPoint, scenePos).length();
        
        // 只有当移动距离足够大时才添加点（避免过多的点）
        if (distance > 2.0) {
            m_points.append(scenePos);
            m_lastPoint = scenePos;
            
            // 直接更新路径
            QPainterPath path;
            path.moveTo(m_points.first());
            for (int i = 1; i < m_points.size(); ++i) {
                path.lineTo(m_points[i]);
            }
            
            m_currentPath->setPath(path);
            
            qDebug() << "Updated path with" << m_points.size() << "points";
        }
        
        return true;
    }
    
    return false;
}

bool DrawingToolBrush::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        
        // 设置最终路径和控制点
        if (m_currentPath) {
            m_currentPath->setControlPoints(m_points);
            m_scene->setModified(true);
            
            qDebug() << "Finished drawing with" << m_points.size() << "points";
        }
        
        return true;
    }
    
    return false;
}

QVector<QPointF> DrawingToolBrush::smoothPath(const QVector<QPointF> &points)
{
    if (points.size() < 3) {
        return points;
    }
    
    QVector<QPointF> smoothedPoints;
    smoothedPoints.append(points[0]);
    
    // 使用简单的加权平均进行平滑
    for (int i = 1; i < points.size() - 1; ++i) {
        QPointF prev = points[i - 1];
        QPointF curr = points[i];
        QPointF next = points[i + 1];
        
        // 加权平均：当前点权重更高
        QPointF smoothed = prev * (1 - m_smoothness) / 2 + 
                          curr * m_smoothness + 
                          next * (1 - m_smoothness) / 2;
        
        smoothedPoints.append(smoothed);
    }
    
    smoothedPoints.append(points.last());
    
    return smoothedPoints;
}





