#ifndef EFFECT_MANAGER_H
#define EFFECT_MANAGER_H

#include <QObject>
#include <QGraphicsEffect>
#include <QColor>
#include <QPointF>
#include <QUndoCommand>
#include <QMap>

class DrawingScene;
class DrawingShape;
class MainWindow;
class CommandManager;

class EffectManager : public QObject
{
    Q_OBJECT

public:
    explicit EffectManager(MainWindow *parent = nullptr);
    ~EffectManager();

    void setScene(DrawingScene *scene);
    DrawingScene *scene() const;
    
    void setCommandManager(CommandManager *commandManager);
    CommandManager *commandManager() const;

signals:
    void effectApplied(const QString &effectName);
    void effectCleared();
    void statusMessageChanged(const QString &message);

public slots:
    void applyBlurEffect(qreal radius);
    void applyBlurEffect(); // 无参数版本，使用默认值
    void applyDropShadowEffect(const QColor &color, 
                              qreal blurRadius, 
                              const QPointF &offset);
    void applyDropShadowEffect(); // 无参数版本，使用默认值
    void clearFilterEffect();



private:
    MainWindow *m_mainWindow;
    DrawingScene *m_scene;
    CommandManager *m_commandManager;
};

#endif // EFFECT_MANAGER_H