#include "handle-icons.h"

// XPM图标定义 - 专业设计风格
static const char* scale_handle_xpm[] = {
    "16 16 8 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #34495E",
    "#	c #3498DB",
    "$	c #2980B9",
    "%	c #ECF0F1",
    "&	c #BDC3C7",
    "                ",
    "       ++       ",
    "      +..+      ",
    "     +....+     ",
    "    +......+    ",
    "   +........+   ",
    "  +..........+  ",
    " +............+ ",
    "+#############+ ",
    " +............+ ",
    "  +..........+  ",
    "   +........+   ",
    "    +......+    ",
    "     +....+     ",
    "      +..+      ",
    "       ++       "
};

static const char* rotate_handle_xpm[] = {
    "16 16 8 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #E74C3C",
    "#	c #C0392B",
    "$	c #ECF0F1",
    "%	c #BDC3C7",
    "&	c #95A5A6",
    "        %%      ",
    "      %..%      ",
    "    %......%    ",
    "   %........%   ",
    "  %..........%  ",
    " %............% ",
    "%@@@@@@@@@@@@@@%",
    "%@@@@@@@@@@@@@@%",
    "%@@@@@@@@@@@@@@%",
    " %............% ",
    "  %..........%  ",
    "   %........%   ",
    "    %......%    ",
    "      %..%      ",
    "        %%      ",
    "                "
};

static const char* center_handle_xpm[] = {
    "16 16 6 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #27AE60",
    "#	c #229954",
    "$	c #ECF0F1",
    "                ",
    "                ",
    "       ++       ",
    "       ++       ",
    "       ++       ",
    "       ++       ",
    "++++++++++++++++",
    "++++++++++++++++",
    "++++++++++++++++",
    "++++++++++++++++",
    "       ++       ",
    "       ++       ",
    "       ++       ",
    "       ++       ",
    "                ",
    "                "
};

static const char* edge_scale_handle_xpm[] = {
    "16 16 7 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #8E44AD",
    "#	c #7D3C98",
    "$	c #ECF0F1",
    "%	c #BDC3C7",
    "                ",
    "       ++       ",
    "      +..+      ",
    "     +....+     ",
    "    +......+    ",
    "   +........+   ",
    "  +..........+  ",
    " +@@@@@@@@@@@@+ ",
    "+@@@@@@@@@@@@@@+",
    "                ",
    "                ",
    "                ",
    "                ",
    "                ",
    "                ",
    "                "
};

static const char* corner_scale_handle_xpm[] = {
    "16 16 7 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #E67E22",
    "#	c #D35400",
    "$	c #ECF0F1",
    "%	c #BDC3C7",
    "                ",
    "       ++       ",
    "      +..+      ",
    "     +....+     ",
    "    +......+    ",
    "   +........+   ",
    "  +..........+  ",
    " +@@@@@@@@@@@@+ ",
    "+@@@@@@@@@@@@@@+",
    "+@@@@@@@@@@@@@@+",
    " +@@@@@@@@@@@@+ ",
    "  +..........+  ",
    "   +........+   ",
    "    +......+    ",
    "     +....+     ",
    "      +..+      "
};

// 左上角箭头（向左上）
static const char* corner_scale_tl_xpm[] = {
    "16 16 7 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #E67E22",
    "#	c #D35400",
    "$	c #ECF0F1",
    "%	c #BDC3C7",
    "++++++.........",
    "+++++......... ",
    "+++++.........  ",
    "++++.........   ",
    "+++........    ",
    "+++........    ",
    "++..........   ",
    "+@@@@@@@@@@@@+ ",
    "+@@@@@@@@@@@@@@+",
    " +@@@@@@@@@@@@+ ",
    "  +..........+  ",
    "   +........+   ",
    "    +......+    ",
    "     +....+     ",
    "      +..+      ",
    "       ++       "
};

// 右上角箭头（向右上）
static const char* corner_scale_tr_xpm[] = {
    "16 16 7 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #E67E22",
    "#	c #D35400",
    "$	c #ECF0F1",
    "%	c #BDC3C7",
    ".........++++++",
    " ........+++++ ",
    "  ........+++++ ",
    "   ........++++ ",
    "    ........+++ ",
    "    ........+++ ",
    "   ..........++ ",
    " +@@@@@@@@@@@@+ ",
    "+@@@@@@@@@@@@@@+",
    "+@@@@@@@@@@@@@@+",
    " +@@@@@@@@@@@@+ ",
    "  +..........+  ",
    "   +........+   ",
    "    +......+    ",
    "     +....+     ",
    "      +..+      "
};

// 左下角箭头（向左下）
static const char* corner_scale_bl_xpm[] = {
    "16 16 7 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #E67E22",
    "#	c #D35400",
    "$	c #ECF0F1",
    "%	c #BDC3C7",
    "       ++       ",
    "      +..+      ",
    "     +....+     ",
    "    +......+    ",
    "   +........+   ",
    "  +..........+  ",
    " +@@@@@@@@@@@@+ ",
    "+@@@@@@@@@@@@@@+",
    "+@@@@@@@@@@@@@@+",
    " ++@@@@@@@@@@@@+",
    "  ++..........+ ",
    "   ++........+  ",
    "    ++.......   ",
    "     +++......  ",
    "      ++++.... ",
    "       +++++... "
};

// 右下角箭头（向右下）
static const char* corner_scale_br_xpm[] = {
    "16 16 7 1",
    " 	c None",
    ".	c #000000",
    "+	c #2C3E50",
    "@	c #E67E22",
    "#	c #D35400",
    "$	c #ECF0F1",
    "%	c #BDC3C7",
    "       ++       ",
    "      +..+      ",
    "     +....+     ",
    "    +......+    ",
    "   +........+   ",
    "  +..........+  ",
    " +@@@@@@@@@@@@+ ",
    "+@@@@@@@@@@@@@@+",
    "+@@@@@@@@@@@@@@+",
    " +@@@@@@@@@@@@++",
    "  +..........++ ",
    "   +........++  ",
    "    .......++   ",
    "  ......+++     ",
    " ....+++++      ",
    " ...+++++       "
};

// 实现静态方法
QPixmap HandleIcons::getScaleHandleIcon()
{
    return QPixmap(scale_handle_xpm);
}

QPixmap HandleIcons::getRotateHandleIcon()
{
    return QPixmap(rotate_handle_xpm);
}

QPixmap HandleIcons::getCenterHandleIcon()
{
    return QPixmap(center_handle_xpm);
}

QPixmap HandleIcons::getEdgeScaleHandleIcon()
{
    return QPixmap(edge_scale_handle_xpm);
}

QPixmap HandleIcons::getCornerScaleHandleIcon()
{
    return QPixmap(corner_scale_handle_xpm);
}

QPixmap HandleIcons::getCornerScaleHandleIcon(TransformHandle::HandleType type)
{
    switch (type) {
    case TransformHandle::TopLeft:
        return QPixmap(corner_scale_tl_xpm);
    case TransformHandle::TopRight:
        return QPixmap(corner_scale_tr_xpm);
    case TransformHandle::BottomLeft:
        return QPixmap(corner_scale_bl_xpm);
    case TransformHandle::BottomRight:
        return QPixmap(corner_scale_br_xpm);
    default:
        return QPixmap(corner_scale_handle_xpm);
    }
}