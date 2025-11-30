cmake_minimum_required(VERSION 3.16)
project(PerformanceTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Qt6路径
set(CMAKE_PREFIX_PATH $ENV{HOME}/Qt/6.9.2/macos ${CMAKE_PREFIX_PATH})

# 查找Qt6组件
find_package(Qt6 COMPONENTS Core Widgets Xml REQUIRED)

# 设置Qt的MOC
set(CMAKE_AUTOMOC ON)

# 包含头文件目录
include_directories(..)

# 添加可执行文件
add_executable(performance_test 
    performance_test.cpp
    ../src/core/fastpathparser.cpp
)

# 渐变验证测试
add_executable(gradient_verify 
    gradient_verify.cpp
    ../src/core/svgelementcollector.cpp
)

# 滤镜验证测试
add_executable(filter_verify 
    filter_verify.cpp
    ../src/core/svghandler.cpp
    ../src/core/svgelementcollector.cpp
)

# 简单滤镜验证测试
add_executable(simple_filter_verify 
    simple_filter_verify.cpp
    ../src/core/svgelementcollector.cpp
)

# Polyline验证测试
add_executable(polyline_verify 
    polyline_verify.cpp
    ../src/core/svgelementcollector.cpp
)

# 完整Polyline测试
add_executable(complete_polyline_test 
    complete_polyline_test.cpp
    ../src/core/svghandler.cpp
    ../src/core/svgelementcollector.cpp
)

# 链接Qt库
target_link_libraries(performance_test Qt6::Core Qt6::Widgets Qt6::Xml)
target_link_libraries(gradient_verify Qt6::Core Qt6::Widgets Qt6::Xml)
target_link_libraries(filter_verify Qt6::Core Qt6::Widgets Qt6::Xml)
target_link_libraries(simple_filter_verify Qt6::Core Qt6::Widgets Qt6::Xml)
target_link_libraries(polyline_verify Qt6::Core Qt6::Widgets Qt6::Xml)
target_link_libraries(complete_polyline_test Qt6::Core Qt6::Widgets Qt6::Xml)

# 链接Qt库
target_link_libraries(performance_test Qt6::Core Qt6::Widgets Qt6::Xml)