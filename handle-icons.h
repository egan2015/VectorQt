#ifndef HANDLE_ICONS_H
#define HANDLE_ICONS_H

#include <QPixmap>
#include "handle-types.h"

/**
 * @brief 手柄图标资源
 * @details 使用XPM格式定义各种手柄的图标样式
 */
class HandleIcons
{
public:
    // 获取对应的QPixmap
    static QPixmap getScaleHandleIcon();
    static QPixmap getRotateHandleIcon();
    static QPixmap getCenterHandleIcon();
    static QPixmap getEdgeScaleHandleIcon();
    static QPixmap getCornerScaleHandleIcon();
    static QPixmap getCornerScaleHandleIcon(TransformHandle::HandleType type);
};

#endif // HANDLE_ICONS_H