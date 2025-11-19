cmake_minimum_required(VERSION 3.16)
project(VectorQt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Qt6路径
set(CMAKE_PREFIX_PATH $ENV{HOME}/Qt/6.9.2/macos ${CMAKE_PREFIX_PATH})

# 查找Qt6组件
find_package(Qt6 COMPONENTS Widgets SvgWidgets Xml REQUIRED)

# Boost配置将在add_executable之后处理

# 收集源文件
set(SOURCES
    ruler.cpp
    main.cpp
    mainwindow.cpp
    drawingscene.cpp
    drawingview.cpp
    toolbase.cpp
    propertypanel.cpp
    drawing-document.cpp
    drawing-tool.cpp
    control-frame.cpp
    drawing-canvas.cpp
    vectorflow.cpp
    # 新增的改进绘图系统
    drawing-shape.cpp
    drawing-transform.cpp
    drawing-edit-handles.cpp
    selection-layer.cpp
    drawing-tool-bezier.cpp
    drawing-tool-bezier-edit.cpp
    drawing-tool-node-edit.cpp
    # 新增的绘图工具
    drawing-tool-brush.cpp
    drawing-tool-fill.cpp
    drawing-tool-gradient-fill.cpp
    drawing-tool-pen.cpp
    drawing-tool-eraser.cpp
    drawing-tool-line.cpp
    drawing-tool-polyline.cpp
    drawing-tool-polygon.cpp
    drawing-tool-path-edit.cpp
    # 选择工具
    drawing-tool-outline-preview.cpp
    # 变换手柄
    transform-handle.cpp
    # 滚动工具栏
    scrollable-toolbar.cpp
    # 图层系统
    drawing-layer.cpp
    # SVG处理
    svghandler.cpp
    # 组合对象
    drawing-group.cpp
    # 颜色调色板
    colorpalette.cpp
    # 路径编辑器
    patheditor.cpp
    # 光标管理器
    cursor-manager.cpp
)

# 收集头文件
set(HEADERS
    ruler.h
    mainwindow.h
    drawingscene.h
    drawingview.h
    toolbase.h
    propertypanel.h
    drawing-document.h
    drawing-tool.h
    control-frame.h
    drawing-canvas.h
    vectorflow.h
    graphics-adapter.h
    # 新增的改进绘图系统
    drawing-shape.h
    drawing-transform.h
    selection-layer.h
    drawing-tool-bezier.h
    drawing-tool-bezier-edit.h
    drawing-tool-node-edit.h
    # 新增的绘图工具
    drawing-tool-brush.h
    drawing-tool-fill.h
    drawing-tool-gradient-fill.h
    drawing-tool-pen.h
    drawing-tool-eraser.h
    drawing-tool-line.h
    drawing-tool-polyline.h
    drawing-tool-polygon.h
    drawing-tool-path-edit.h
    # 选择工具
    drawing-tool-outline-preview.h
    # 变换手柄
    transform-handle.h
    # 滚动工具栏
    scrollable-toolbar.h
    # 图层系统
    drawing-layer.h
    # SVG处理
    svghandler.h
    # 组合对象
    drawing-group.h
    # 颜色调色板
    colorpalette.h
    # 路径编辑器
    patheditor.h
    # 光标管理器
    cursor-manager.h
)

# 收集资源文件
set(RESOURCES
    icons.qrc
)

# 设置Qt的MOC（必须在add_executable之前）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 生成可执行文件
add_executable(VectorQt ${SOURCES} ${HEADERS} ${RESOURCES})

# 链接Qt6库
target_link_libraries(VectorQt
    Qt6::Widgets
    Qt6::SvgWidgets
    Qt6::Xml
)

# 设置包含目录
target_include_directories(VectorQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})



# 编译器特定设置
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(VectorQt PRIVATE -Wall -Wextra -Wno-deprecated-builtins)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(VectorQt PRIVATE /W4)
endif()

# 调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(VectorQt PRIVATE DEBUG)
endif()

