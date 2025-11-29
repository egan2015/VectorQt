#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPen>
#include <QDebug>
#include "drawing-tool-brush.h"
#include "../core/drawing-throttle.h"
#include "../core/brush-engine.h"

#include "../core/smart-render-manager.h"
#include "../core/performance-monitor.h"
#include "../ui/drawingscene.h"
#include "../ui/drawingview.h"
#include "../core/drawing-shape.h"
#include "../core/drawing-layer.h"
#include "../core/layer-manager.h"
#include "../ui/command-manager.h"

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
    
    // 画笔工具不需要对象池，直接创建对象更简单高效
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
    
    // 如果正在绘制，完成绘制
    if (m_drawing && m_currentPath) {
        m_drawing = false;
        
        // 检查路径是否太小
        QRectF boundingRect = m_currentPath->boundingRect();
        if (boundingRect.width() <= 5 && boundingRect.height() <= 5) {
            // 太小了，删除
            m_scene->removeItem(m_currentPath);
            // 删除路径对象
            delete m_currentPath;
            m_currentPath = nullptr;
            m_points.clear();
            qDebug() << "Brush stroke too small on deactivate, deleted";
        } else {
            // 路径大小合适，添加到图层
            // 先从场景中移除（因为已经添加了）
            m_scene->removeItem(m_currentPath);
            
            // 添加到活动图层
            LayerManager *layerManager = LayerManager::instance();
            DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
            
            if (activeLayer) {
                activeLayer->addShape(m_currentPath);
                qDebug() << "Added brush stroke to active layer on deactivate:" << activeLayer->name();
            } else {
                // 如果没有活动图层，直接添加到场景（向后兼容）
                m_scene->addItem(m_currentPath);
                qDebug() << "No active layer, added brush stroke directly to scene on deactivate";
            }
            
            m_scene->setModified(true);
            m_currentPath = nullptr; // 不再由工具管理
        }
    } else {
        // 没有在绘制，只是清除引用
        m_currentPath = nullptr;
    }
    
    ToolBase::deactivate();
}

bool DrawingToolBrush::mousePressEvent(QMouseEvent *event, const QPointF &scenePos)
{
    PERF_MONITOR_SCOPE("BrushMousePress");
    
    if (event->button() == Qt::LeftButton && m_scene) {
        m_drawing = true;
        m_points.clear();
        m_points.append(scenePos);
        m_lastPoint = scenePos;
        
        // 直接创建路径对象
        m_currentPath = new DrawingPath();
        
        // 移除对象池性能监控
        QPainterPath path;
        path.moveTo(scenePos);
        m_currentPath->setPath(path);
        
        // 设置基本画笔样式
        QPen pen(Qt::black, m_brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        m_currentPath->setStrokePen(pen);
        m_currentPath->setFillBrush(Qt::NoBrush); // 确保绘制纯线条
        
        // 只添加到场景进行预览，不添加到图层
        m_scene->addItem(m_currentPath);
        qDebug() << "Created brush preview, not yet added to layer";
        
        m_currentPath->setVisible(true);
        m_currentPath->setFlag(QGraphicsItem::ItemIsSelectable, true);
        
        qDebug() << "Created initial DrawingPath at" << scenePos;
        
        return true;
    }
    
    return false;
}

bool DrawingToolBrush::mouseMoveEvent(QMouseEvent *event, const QPointF &scenePos)
{
    PERF_MONITOR_SCOPE("BrushMouseMove");
    
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
            
            // qDebug() << "Updated path with" << m_points.size() << "points";
        }
        
        return true;
    }
    
    return false;
}

bool DrawingToolBrush::mouseReleaseEvent(QMouseEvent *event, const QPointF &scenePos)
{
    PERF_MONITOR_SCOPE("BrushMouseRelease");
    
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        
        // 设置最终路径和控制点
        if (m_currentPath) {
            m_currentPath->setControlPoints(m_points);
            
            // 检查路径是否太小，如果是则删除
            QRectF boundingRect = m_currentPath->boundingRect();
            if (boundingRect.width() <= 5 && boundingRect.height() <= 5) {
                // 太小了，删除
                m_scene->removeItem(m_currentPath);
                // 删除路径对象
                delete m_currentPath;
                m_currentPath = nullptr;
                m_points.clear();
                qDebug() << "Brush stroke too small, deleted";
            } else {
                // 路径大小合适，现在才添加到图层
                // 先从场景中移除（因为已经添加了）
                m_scene->removeItem(m_currentPath);
                
                // 添加到活动图层
                LayerManager *layerManager = LayerManager::instance();
                DrawingLayer *activeLayer = layerManager ? layerManager->activeLayer() : nullptr;
                
                if (activeLayer) {
                    activeLayer->addShape(m_currentPath);
                    qDebug() << "Added brush stroke to active layer:" << activeLayer->name();
                } else {
                    // 如果没有活动图层，直接添加到场景（向后兼容）
                    m_scene->addItem(m_currentPath);
                    qDebug() << "No active layer, added brush stroke directly to scene";
                }
                
                // 创建撤销命令
                    class BrushAddCommand : public QUndoCommand
                    {
                    public:
                        BrushAddCommand(DrawingScene *scene, DrawingPath *path, DrawingLayer *layer, QUndoCommand *parent = nullptr)
                            : QUndoCommand("添加画笔", parent), m_scene(scene), m_path(path), m_layer(layer), m_pathOwnedByCommand(false) {}
                        
                        ~BrushAddCommand() {
                            // 如果命令拥有路径对象且路径不在场景中，则删除对象
                            if (m_pathOwnedByCommand && m_path && !m_path->scene()) {
                                delete m_path;
                                m_path = nullptr;
                            }
                        }
                        
                        void undo() override {
                            if (m_path && m_layer) {
                                m_layer->removeShape(static_cast<DrawingShape*>(m_path));
                                m_path->setVisible(false);
                                m_path->setSelected(false); // 取消选中状态
                                
                                // 命令现在拥有路径对象的责任
                                m_pathOwnedByCommand = true;
                                
                                // 发送清理所有手柄信号，通知所有工具清理手柄
                                if (m_scene) {
                                    emit m_scene->allToolsClearHandles();
                                }
                                
                                // 通知图层内容变化
                                LayerManager *layerManager = LayerManager::instance();
                                if (layerManager) {
                                    emit layerManager->layerContentChanged(m_layer);
                                }
                            }
                        }
                        
                        void redo() override {
                            if (m_path && m_layer) {
                                m_layer->addShape(static_cast<DrawingShape*>(m_path));
                                m_path->setVisible(true);
                                
                                // 自动选中新创建的图形
                                m_path->setSelected(true);
                                
                                // 清除其他选中项
                                if (m_scene) {
                                    for (QGraphicsItem *item : m_scene->selectedItems()) {
                                        if (item != m_path) {
                                            item->setSelected(false);
                                        }
                                    }
                                }
                                
                                // 路径现在由图层管理，命令不再拥有
                                m_pathOwnedByCommand = false;
                                
                                // 通知图层内容变化
                                LayerManager *layerManager = LayerManager::instance();
                                if (layerManager) {
                                    emit layerManager->layerContentChanged(m_layer);
                                }
                            }
                        }
                        
                    private:
                        DrawingScene *m_scene;
                        DrawingPath *m_path;
                        DrawingLayer *m_layer;
                        bool m_pathOwnedByCommand; // 标记命令是否拥有路径对象
                    };
                    
                    // 创建并推送撤销命令
                    BrushAddCommand *command = new BrushAddCommand(m_scene, m_currentPath, activeLayer);
                    if (CommandManager::hasInstance()) {
        CommandManager::instance()->pushCommand(command);
    } else {
        command->redo();
        delete command;
    }
                
                m_scene->setModified(true);
                m_currentPath = nullptr; // 不再由工具管理
                qDebug() << "Finished drawing with" << m_points.size() << "points";
            }
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





