#include "transform-components.h"
#include <QPointF>
#include <QDebug>
#include <cmath>
#include <chrono>

void testTransformComponents()
{
    qDebug() << "=== 测试变换分量系统 ===";
    
    // 1. 基本变换测试
    qDebug() << "\n1. 基本变换测试:";
    
    Translate trans(QPointF(10, 20));
    Rotate rotate(45, QPointF(50, 50));
    Scale scale(QPointF(2.0, 1.5), QPointF(50, 50));
    Shear shear(QPointF(0.2, 0.1), QPointF(50, 50));
    
    qDebug() << "平移 (10, 20):" << trans.toTransform();
    qDebug() << "旋转 45° 中心(50,50):" << rotate.toTransform();
    qDebug() << "缩放 (2.0, 1.5) 中心(50,50):" << scale.toTransform();
    qDebug() << "斜切 (0.2, 0.1) 中心(50,50):" << shear.toTransform();
    
    // 2. 变换组合测试
    qDebug() << "\n2. 变换组合测试:";
    
    QTransform transform = QTransform();
    transform = transform * trans;
    transform = transform * rotate;
    transform = transform * scale;
    
    qDebug() << "组合变换结果:" << transform;
    
    // 3. 链式操作测试
    qDebug() << "\n3. 链式操作测试:";
    
    // 使用 *= 运算符的链式操作
    QTransform chainTransform;
    chainTransform *= Translate{QPointF(10, 20)};
    chainTransform *= Rotate{45, QPointF(50, 50)};
    chainTransform *= Scale{QPointF(1.5, 1.5), QPointF(50, 50)};
    
    qDebug() << "链式变换结果:" << chainTransform;
    
    // 4. 逆操作测试
    qDebug() << "\n4. 逆操作测试:";
    
    QPointF testPoint(60, 60);
    QPointF transformed = (trans * rotate * scale).map(testPoint);
    qDebug() << "原始点:" << testPoint;
    qDebug() << "变换后:" << transformed;
    
    // 应用逆变换
    QPointF restored = (scale.inverse() * rotate.inverse() * trans.inverse()).map(transformed);
    qDebug() << "逆变换恢复:" << restored;
    qDebug() << "恢复精度:" << (testPoint - restored).manhattanLength();
    
    // 5. 操作组合测试
    qDebug() << "\n5. 操作组合测试:";
    
    Scale scale1(QPointF(2.0, 2.0), QPointF(0, 0));
    Scale scale2(QPointF(1.5, 1.5), QPointF(10, 10));
    Scale combinedScale = scale1 * scale2;
    qDebug() << "缩放1:" << scale1.factors;
    qDebug() << "缩放2:" << scale2.factors;
    qDebug() << "组合缩放:" << combinedScale.factors;
    
    // 6. 便利函数测试
    qDebug() << "\n6. 便利函数测试:";
    
    QTransform quickTransform = createTransform(
        QPointF(10, 20),  // 平移
        30,               // 旋转角度
        QPointF(1.2, 0.8), // 缩放
        QPointF(50, 50)    // 中心
    );
    qDebug() << "快速创建的变换:" << quickTransform;
    
    qDebug() << "\n=== 测试完成 ===";
}

// 性能测试
void performanceTest()
{
    qDebug() << "\n=== 性能测试 ===";
    
    const int iterations = 100000;
    QPointF testPoint(100, 100);
    
    // 测试传统方式
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        QTransform t;
        t.translate(10, 20);
        t.rotate(45);
        t.scale(1.5, 1.5);
        t.map(testPoint);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto traditionalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 测试新系统（使用 *= 运算符）
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        QTransform transform;
        transform *= Translate{QPointF(10, 20)};
        transform *= Rotate{45, QPointF(0, 0)};
        transform *= Scale{QPointF(1.5, 1.5), QPointF(0, 0)};
        transform.map(testPoint);
    }
    end = std::chrono::high_resolution_clock::now();
    auto newSystemTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    qDebug() << "传统方式耗时:" << traditionalTime.count() << "微秒";
    qDebug() << "新系统耗时:" << newSystemTime.count() << "微秒";
    qDebug() << "性能比率:" << (double)newSystemTime.count() / traditionalTime.count();
}

int main()
{
    testTransformComponents();
    performanceTest();
    return 0;
}