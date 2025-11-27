#ifndef PATH_OPERATIONS_MANAGER_H
#define PATH_OPERATIONS_MANAGER_H

#include <QObject>
#include <QString>
#include <QPainterPath>

class DrawingScene;
class QMenu;
class DrawingShape;
class MainWindow;
class CommandManager;

class PathOperationsManager : public QObject
{
    Q_OBJECT

public:
    explicit PathOperationsManager(MainWindow *parent = nullptr);
    ~PathOperationsManager();

    void setScene(DrawingScene *scene);
    DrawingScene *scene() const;
    
    void setCommandManager(CommandManager *commandManager);
    CommandManager *commandManager() const;

    // 路径布尔运算操作类型
    enum BooleanOperation {
        Union,
        Subtract,
        Intersect,
        Xor
    };

    // 路径操作类型
    enum PathOperation {
        Simplify,
        Smooth,
        Reverse,
        ConvertToCurve,
        OffsetPath,
        ClipPath
    };

signals:
    void pathOperationCompleted(const QString &operation);
    void statusMessageChanged(const QString &message);

public slots:
    // 布尔运算
    void pathUnion();
    void pathSubtract();
    void pathIntersect();
    void pathXor();
    
    // 路径操作
    void pathSimplify();
    void pathSmooth();
    void pathReverse();
    void pathConvertToCurve();
    void pathOffsetPath();
    void pathClipPath();
    
    // 文本转路径
    void convertTextToPath();
    void convertSelectedTextToPath();
    
    // 上下文菜单支持
    void addPathOperationsToMenu(QMenu *menu, const QPointF &pos);
    
    // 创建图形方法
    void createShapeAtPosition(const QString &shapeType, const QPointF &pos);

private:
    // 执行布尔运算
    void performBooleanOperation(BooleanOperation op, const QString &opName);
    
    // 执行路径操作
    void performPathOperation(PathOperation op, const QString &opName);
    
    // 检查选择是否适合操作
    bool validateSelectionForBoolean();
    bool validateSelectionForPathOperation();
    
    // 路径处理辅助方法
    QPainterPath simplifyPath(const QPainterPath &path);
    QPainterPath smoothPath(const QPainterPath &path);
    
    // 静态辅助方法
    static QPainterPath simplifyPathStatic(const QPainterPath &path);
    static QPainterPath smoothPathStatic(const QPainterPath &path);
    static QPainterPath convertToCurveStatic(const QPainterPath &path);
    static QPainterPath offsetPathStatic(const QPainterPath &path, qreal offset);
    static QPainterPath clipPathStatic(const QPainterPath &path);

private:
    MainWindow *m_mainWindow;
    DrawingScene *m_scene;
    CommandManager *m_commandManager;
};

#endif // PATH_OPERATIONS_MANAGER_H