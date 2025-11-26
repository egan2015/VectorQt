#ifndef PATH_OPERATIONS_MANAGER_H
#define PATH_OPERATIONS_MANAGER_H

#include <QObject>
#include <QString>

class DrawingScene;
class QMenu;
class DrawingShape;
class MainWindow;

class PathOperationsManager : public QObject
{
    Q_OBJECT

public:
    explicit PathOperationsManager(MainWindow *parent = nullptr);
    ~PathOperationsManager();

    void setScene(DrawingScene *scene);
    DrawingScene *scene() const;

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
        Reverse
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
    
    // 文本转路径
    void convertTextToPath();
    
    // 通用路径操作方法（从MainWindow移动过来）
    void performPathBooleanOperation(int op, const QString &opName);
    void executeBooleanOperation(int op);
    void executePathOperation(const QString &operation);
    
    // 上下文菜单支持
    void addPathOperationsToMenu(QMenu *menu, const QPointF &pos);
    
    // 路径编辑方法
    void generateShape();
    void createShapeAtPosition(const QString &shapeType, const QPointF &pos);
    void convertSelectedTextToPath();

private:
    // 执行布尔运算
    void performBooleanOperation(BooleanOperation op, const QString &opName);
    
    // 执行路径操作
    void performPathOperation(PathOperation op, const QString &opName);
    
    // 检查选择是否适合操作
    bool validateSelectionForBoolean();
    bool validateSelectionForPathOperation();

private:
    MainWindow *m_mainWindow;
    DrawingScene *m_scene;
};

#endif // PATH_OPERATIONS_MANAGER_H