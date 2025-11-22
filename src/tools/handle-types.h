#ifndef HANDLE_TYPES_H
#define HANDLE_TYPES_H

/**
 * @brief 变换手柄类型枚举
 */
class TransformHandle
{
public:
    enum HandleType {
        None = 0,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Left,
        Right,
        Top,
        Bottom,
        Center,
        Rotate  // 旋转手柄
    };
};

/**
 * @brief 手柄显示模式
 */
class HandleMode
{
public:
    enum Mode {
        Scale,      // 缩放模式：显示8个缩放手柄
        RotateMode  // 旋转模式：显示4个旋转手柄+中心手柄
    };
};

#endif // HANDLE_TYPES_H