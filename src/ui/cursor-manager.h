#ifndef CURSOR_MANAGER_H
#define CURSOR_MANAGER_H

#include <QCursor>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QHash>

/**
 * 自定义光标管理器 - 为不同工具创建专门的鼠标指针
 */
class CursorManager
{
public:
    // 获取单例实例
    static CursorManager& instance();
    
    // 不同类型的光标
    enum CursorType {
        SelectCursor,        // 选择工具光标
        RectangleCursor,     // 矩形工具光标
        EllipseCursor,       // 椭圆工具光标
        LineCursor,          // 直线工具光标
        BezierCursor,        // 贝塞尔工具光标
        PolygonCursor,       // 多边形工具光标
        PolylineCursor,      // 多线工具光标
        BrushCursor,         // 画笔工具光标
        FillCursor,          // 填充工具光标
        NodeEditCursor,      // 节点编辑光标
        PathEditCursor,      // 路径编辑光标
        TextCursor,          // 文本工具光标
        DefaultCursor        // 默认光标
    };
    
    // 获取指定类型的光标
    QCursor getCursor(CursorType type);
    
    // 为视图设置光标
    void setCursorForView(QWidget *view, CursorType type);

private:
    CursorManager(); // 私有构造函数，实现单例模式
    
    // 创建各种光标
    void createCursors();
    
    // 创建十字线光标的基础方法
    QPixmap createCrosshairCursor(int size = 32);
    
    // 创建带形状的十字光标
    QPixmap createCrosshairWithShape(CursorType type, int size = 32);
    
    // 存储创建的光标
    QHash<CursorType, QCursor> m_cursors;
    bool m_initialized;
};

#endif // CURSOR_MANAGER_H