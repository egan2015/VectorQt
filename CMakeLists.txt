cmake_minimum_required(VERSION 3.16)
project(VectorQt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Qt6路径
set(CMAKE_PREFIX_PATH $ENV{HOME}/Qt/6.9.2/macos ${CMAKE_PREFIX_PATH})

# 查找Qt6组件
find_package(Qt6 COMPONENTS Widgets SvgWidgets Xml LinguistTools REQUIRED)

# 设置Qt的MOC（必须在add_executable之前）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 收集源文件
set(SOURCES
    # UI 模块
    src/ui/main.cpp
    src/ui/mainwindow.cpp
    src/ui/drawingscene.cpp
    src/ui/drawingview.cpp
    src/ui/control-frame.cpp
    
    # 核心模块
    src/core/toolbase.cpp
    src/core/vectorflow.cpp
    src/core/drawing-document.cpp
    src/core/drawing-canvas.cpp
    src/core/drawing-shape.cpp
    src/core/drawing-group.cpp
    src/core/drawing-layer.cpp
    src/core/drawing-throttle.cpp
    src/core/brush-engine.cpp
    src/core/object-pool.cpp
    src/core/smart-render-manager.cpp
    src/core/performance-monitor.cpp
    src/ui/colorpalette.cpp
    src/ui/cursor-manager.cpp
    src/core/layer-manager.cpp
    src/core/patheditor.cpp
    src/core/object-tree-item.cpp
    src/core/object-tree-model.cpp
    src/ui/object-tree-view.cpp
    src/ui/ruler.cpp
    src/ui/scrollable-toolbar.cpp
    src/core/svghandler.cpp
    src/core/graphics-adapter.h
    src/tools/tool-state-manager.cpp
    src/tools/tool-manager.cpp
    src/ui/shortcut-manager.cpp
    
    # 绘图工具模块
    src/tools/drawing-tool.cpp
    src/tools/drawing-tool-bezier.cpp
    src/tools/drawing-tool-brush.cpp
    src/tools/drawing-tool-eraser.cpp
    src/tools/drawing-tool-fill.cpp
    src/tools/drawing-tool-gradient-fill.cpp
    src/tools/drawing-tool-pen.cpp
    src/tools/drawing-tool-line.cpp
    src/tools/drawing-tool-polyline.cpp
    src/tools/drawing-tool-polygon.cpp
    src/tools/drawing-tool-path-edit.cpp
    src/tools/drawing-tool-node-edit.cpp
    src/tools/drawing-tool-outline-preview.cpp
    
    # UI 模块（包含面板）
    src/ui/propertypanel.cpp
    src/ui/tabbed-property-panel.cpp
    src/ui/layer-panel.cpp
    src/ui/tools-panel.cpp
    src/ui/page-settings-panel.cpp
    src/ui/tabbed-property-panel.cpp
    src/ui/performance-panel-tab.cpp
    
    # 工具模块（包含变换系统）
    src/tools/transform-handle.cpp
    src/tools/handle-item.cpp
    src/tools/handle-icons.cpp
    src/tools/node-handle-manager.cpp
    src/tools/transform-components.h
)

# 收集头文件
set(HEADERS
    # UI 模块
    src/ui/mainwindow.h
    src/ui/drawingscene.h
    src/ui/drawingview.h
    src/ui/control-frame.h
    
    # 核心模块
    src/core/toolbase.h
    src/core/vectorflow.h
    src/core/drawing-document.h
    src/core/drawing-canvas.h
    src/core/drawing-shape.h
    src/core/drawing-group.h
    src/core/drawing-layer.h
    src/core/drawing-throttle.h
    src/core/brush-engine.h
    src/core/object-pool.h
    src/core/smart-render-manager.h
    src/core/performance-monitor.h
    src/ui/colorpalette.h
    src/ui/cursor-manager.h
    src/core/layer-manager.h
    src/core/patheditor.h
    src/core/object-tree-item.h
    src/core/object-tree-model.h
    src/ui/object-tree-view.h
    src/ui/ruler.h
    src/ui/scrollable-toolbar.h
    src/core/svghandler.h
    src/core/graphics-adapter.h
    src/tools/tool-state-manager.h
    src/tools/tool-manager.h
    src/ui/shortcut-manager.h
    
    # 绘图工具模块
    src/tools/drawing-tool.h
    src/tools/drawing-tool-bezier.h
    src/tools/drawing-tool-brush.h
    src/tools/drawing-tool-eraser.h
    src/tools/drawing-tool-fill.h
    src/tools/drawing-tool-gradient-fill.h
    src/tools/drawing-tool-pen.h
    src/tools/drawing-tool-line.h
    src/tools/drawing-tool-polyline.h
    src/tools/drawing-tool-polygon.h
    src/tools/drawing-tool-path-edit.h
    src/tools/drawing-tool-node-edit.h
    src/tools/drawing-tool-outline-preview.h
    
    # UI 模块（包含面板）
    src/ui/propertypanel.h
    src/ui/tabbed-property-panel.h
    src/ui/layer-panel.h
    src/ui/tools-panel.h
    src/ui/page-settings-panel.h
    src/ui/object-tree-view.h
    src/ui/performance-panel-tab.h
    
    # 工具模块（包含变换系统）
    src/tools/transform-handle.h
    src/tools/handle-item.h
    src/tools/handle-icons.h
    src/tools/handle-types.h
    src/tools/node-handle-manager.h
    src/tools/transform-components.h
)

# 收集资源文件
set(RESOURCES
    icons.qrc
)

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

# 国际化支持
set(TS_FILES
    translations/vectorqt_zh_CN.ts
    translations/vectorqt_en.ts
)

# 生成翻译文件
qt6_add_lupdate(VectorQt ${SOURCES} ${HEADERS} ${TS_FILES})

# 编译翻译文件
qt6_add_lrelease(VectorQt ${TS_FILES})