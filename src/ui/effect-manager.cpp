#include "effect-manager.h"
#include "mainwindow.h"
#include "drawingscene.h"
#include "command-manager.h"
#include "../core/drawing-shape.h"
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>

EffectManager::EffectManager(MainWindow *parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_scene(nullptr)
    , m_commandManager(nullptr)
{
}

EffectManager::~EffectManager()
{
}

void EffectManager::setScene(DrawingScene *scene)
{
    m_scene = scene;
}

DrawingScene *EffectManager::scene() const
{
    return m_scene;
}

void EffectManager::setCommandManager(CommandManager *commandManager)
{
    m_commandManager = commandManager;
}

CommandManager *EffectManager::commandManager() const
{
    return m_commandManager;
}

void EffectManager::applyBlurEffect(qreal radius)
{
    qDebug() << "EffectManager::applyBlurEffect called with radius:" << radius;
    
    if (!m_scene) {
        qDebug() << "No scene available";
        return;
    }

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    qDebug() << "Selected items count:" << selected.count();
    
    if (selected.isEmpty()) {
        qDebug() << "No selected items";
        emit statusMessageChanged("请先选择要应用效果的对象");
        return;
    }

    if (m_commandManager) {
        qDebug() << "CommandManager is available";
        // 转换为DrawingShape列表
        QList<DrawingShape*> shapes;
        for (QGraphicsItem *item : selected) {
            if (DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item)) {
                shapes.append(shape);
                qDebug() << "Found DrawingShape:" << shape;
            }
        }
        
        qDebug() << "DrawingShapes count:" << shapes.count();
        
        if (!shapes.isEmpty()) {
            qDebug() << "Creating EffectCommand";
            // 使用统一的EffectCommand
            EffectCommand *command = new EffectCommand(m_commandManager, shapes, 
                EffectCommand::Blur, radius, "应用高斯模糊");
            qDebug() << "Pushing EffectCommand to CommandManager";
            m_commandManager->pushCommand(command);
            qDebug() << "EffectCommand pushed successfully";
        } else {
            qDebug() << "No DrawingShapes found in selected items";
        }
    } else {
        qDebug() << "CommandManager is not available";
        emit statusMessageChanged("命令管理器未初始化");
    }
}

void EffectManager::applyBlurEffect()
{
    qDebug() << "EffectManager::applyBlurEffect() called";
    applyBlurEffect(5.0); // 使用默认半径
}

void EffectManager::applyDropShadowEffect(const QColor &color, qreal blurRadius, const QPointF &offset)
{
    if (!m_scene) return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        emit statusMessageChanged("请先选择要应用效果的对象");
        return;
    }

    if (m_commandManager) {
        // 转换为DrawingShape列表
        QList<DrawingShape*> shapes;
        for (QGraphicsItem *item : selected) {
            if (DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item)) {
                shapes.append(shape);
            }
        }
        
        if (!shapes.isEmpty()) {
            // 阴影参数打包
            QVariantMap shadowParams;
            shadowParams["color"] = color;
            shadowParams["blurRadius"] = blurRadius;
            shadowParams["offset"] = offset;
            
            // 使用统一的EffectCommand
            m_commandManager->pushCommand(new EffectCommand(m_commandManager, shapes, 
                EffectCommand::DropShadow, shadowParams, "应用阴影效果"));
        }
    } else {
        emit statusMessageChanged("命令管理器未初始化");
    }
}

void EffectManager::applyDropShadowEffect()
{
    applyDropShadowEffect(Qt::black, 3.0, QPointF(3.0, 3.0)); // 使用默认值
}

void EffectManager::clearFilterEffect()
{
    if (!m_scene) return;

    QList<QGraphicsItem *> selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        emit statusMessageChanged("请先选择要清除效果的对象");
        return;
    }

    if (m_commandManager) {
        // 转换为DrawingShape列表
        QList<DrawingShape*> shapes;
        for (QGraphicsItem *item : selected) {
            if (DrawingShape *shape = qgraphicsitem_cast<DrawingShape*>(item)) {
                shapes.append(shape);
            }
        }
        
        if (!shapes.isEmpty()) {
            // 使用统一的EffectCommand
            m_commandManager->pushCommand(new EffectCommand(m_commandManager, shapes, 
                EffectCommand::ClearEffect, QVariant(), "清除滤镜效果"));
        }
    } else {
        emit statusMessageChanged("命令管理器未初始化");
    }
}

