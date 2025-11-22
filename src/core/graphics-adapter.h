#ifndef GRAPHICS_ADAPTER_H
#define GRAPHICS_ADAPTER_H
#include <QObject>
#include <QPoint>
#include <QGraphicsItem>

// 定义控制接口
class ControlInterface
{
public:
    virtual ~ControlInterface() = default;
    virtual void control(const QPointF &scenePos) = 0;
};
// 定义ControlObject类，间接为CRTP子类提供QObject的信号能力
class ControlObject : public QObject,
                      public ControlInterface
{
    Q_OBJECT
public:
    explicit ControlObject(QObject *parent = nullptr) : QObject(parent) {}
};

template <typename Derived, typename GraphicsType>
class GraphicsWrapper : public ControlInterface, public GraphicsType
{
public:
    // // 不带QObject父对象的构造函数（使用默认nullptr）
    // template <typename... Args>
    // explicit GraphicsWrapper(Args&&... args)
    //     :GraphicsType(std::forward<Args>(args)...)
    // {}
    using GraphicsType::GraphicsType;
    void control(const QPointF &scenePos) override
    {
        static_cast<Derived *>(this)->controlImpl(scenePos);
    }
};


#endif