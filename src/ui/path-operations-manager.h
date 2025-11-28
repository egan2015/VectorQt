#ifndef PATH_OPERATIONS_MANAGER_H
#define PATH_OPERATIONS_MANAGER_H

#include <QObject>
#include <QString>
#include <QPainterPath>
#include <QMap>

class DrawingScene;
class QMenu;
class DrawingShape;
class DrawingPath;
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
    // 执行布尔运算（旧版本，保留兼容性）
    void performBooleanOperation(BooleanOperation op, const QString &opName);
    
    // 执行布尔运算（新版本，使用宏命令）
    void performBooleanOperationMacro(BooleanOperation op, const QString &opName);
    
    // 执行路径操作
    void performPathOperation(PathOperation op, const QString &opName);
    
    // 执行路径操作（宏命令版本）
    void performPathOperationMacro(PathOperation op, const QString &opName);
    
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
    
    // 宏命令版本的状态保存
    QMap<DrawingShape*, QPointF> m_savedPositions;
    QMap<DrawingShape*, QPair<QPen, QBrush>> m_savedStyles;
    QList<DrawingPath*> m_createdPaths;
    
    // 执行布尔运算的具体步骤
    void executeBooleanOperationSteps(BooleanOperation op);
    
    // 执行路径操作的具体步骤
    void executePathOperationSteps(PathOperation op);
    
    // 宏命令版本的状态保存
    QMap<DrawingShape*, QPainterPath> m_originalPaths;
};

#endif // PATH_OPERATIONS_MANAGER_H