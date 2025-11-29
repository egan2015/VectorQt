#include <QDebug>
#include "drawing-layer.h"
#include "drawing-shape.h"
#include "../ui/drawingscene.h"

DrawingLayer::DrawingLayer(const QString &name, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_visible(true)
    , m_opacity(1.0)
    , m_locked(false)
    , m_scene(nullptr)
{
}

DrawingLayer::~DrawingLayer()
{
    // 清理所有图形，将它们从场景中移除
    for (DrawingShape *shape : m_shapes) {
        if (shape && m_scene) {
            // 禁用编辑把手，防止访问已删除的图层
            shape->setEditHandlesEnabled(false);
            
            // 从场景中移除图形
            m_scene->removeItem(shape);
            
            // 如果图形被选中，取消选中
            if (shape->isSelected()) {
                shape->setSelected(false);
            }
        }
    }
    m_shapes.clear();
}

void DrawingLayer::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        
        // 更新所有子图形的可见性
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                shape->setVisible(visible);
            }
        }
        
        emit visibilityChanged(visible);
    }
}

void DrawingLayer::setOpacity(qreal opacity)
{
    if (m_opacity != opacity) {
        m_opacity = qBound(0.0, opacity, 1.0);
        
        // 更新所有子图形的透明度
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                shape->setOpacity(m_opacity);
            }
        }
        
        emit opacityChanged(m_opacity);
    }
}

void DrawingLayer::addShape(DrawingShape *shape)
{
    if (shape && !m_shapes.contains(shape)) {
        m_shapes.append(shape);
        
        // 将图形添加到场景中
        if (m_scene) {
            m_scene->addItem(shape);
        }
        
        // 应用图层属性到图形
        shape->setVisible(m_visible);
        shape->setOpacity(m_opacity);
        
        // 发出对象添加信号
        emit shapeAdded(shape);
    }
}

void DrawingLayer::removeShape(DrawingShape *shape)
{
    if (m_shapes.removeOne(shape)) {
        // 从场景中移除图形
        if (m_scene) {
            m_scene->removeItem(shape);
        }
        
        // 发出对象移除信号
        emit shapeRemoved(shape);
    }
}

void DrawingLayer::setScene(DrawingScene *scene)
{
    if (m_scene == scene) {
        return;
    }
    
    // 如果之前有场景，先从中移除所有图形
    if (m_scene) {
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                m_scene->removeItem(shape);
            }
        }
    }
    
    m_scene = scene;
    
    // 将所有图形添加到新场景
    if (m_scene) {
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                m_scene->addItem(shape);
            }
        }
    }
}

void DrawingLayer::setLayerTransform(const QTransform &transform)
{
    if (m_layerTransform != transform) {
        m_layerTransform = transform;
        
        // 应用变换到所有图形
        for (DrawingShape *shape : m_shapes) {
            if (shape) {
                // TODO: 应用变换到图形
            }
        }
    }
}

void DrawingLayer::parseFromSvg(const QDomElement &element)
{
    // 解析图层属性
    m_name = element.attribute("id", m_name);
    
    // 解析透明度
    QString opacityStr = element.attribute("opacity");
    if (!opacityStr.isEmpty()) {
        m_opacity = opacityStr.toDouble();
    }
    
    // 解析可见性
    QString visibilityStr = element.attribute("visibility");
    if (!visibilityStr.isEmpty()) {
        m_visible = (visibilityStr != "hidden");
    }
    
    // 解析变换
    QString transformStr = element.attribute("transform");
    if (!transformStr.isEmpty()) {
        // TODO: 解析SVG变换字符串
    }
    
    // qDebug() << "解析图层:" << m_name << "透明度:" << m_opacity << "可见:" << m_visible;
}

QDomElement DrawingLayer::exportToSvg(QDomDocument &doc) const
{
    QDomElement gElement = doc.createElement("g");
    
    // 设置图层属性
    if (!m_name.isEmpty()) {
        gElement.setAttribute("id", m_name);
    }
    
    if (m_opacity < 1.0) {
        gElement.setAttribute("opacity", QString::number(m_opacity));
    }
    
    if (!m_visible) {
        gElement.setAttribute("visibility", "hidden");
    }
    
    // 导出所有图形
    for (DrawingShape *shape : m_shapes) {
        if (shape) {
            // TODO: 将图形导出为SVG元素并添加到gElement中
        }
    }
    
    return gElement;
}