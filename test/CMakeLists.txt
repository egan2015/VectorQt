cmake_minimum_required(VERSION 3.16)
project(TestTransformComponents)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6
set(CMAKE_PREFIX_PATH $ENV{HOME}/Qt/6.9.2/macos ${CMAKE_PREFIX_PATH})
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

# 设置Qt的MOC
set(CMAKE_AUTOMOC ON)

# 创建可执行文件
add_executable(test-transform-components 
    test-transform-components.cpp
)

# 链接Qt6库
target_link_libraries(test-transform-components Qt6::Core Qt6::Widgets)

# 设置包含目录
target_include_directories(test-transform-components PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 编译器设置
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test-transform-components PRIVATE -Wall -Wextra)
endif()