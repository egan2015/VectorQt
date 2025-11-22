QT += core widgets
CONFIG += c++17
TARGET = transform-demo

SOURCES += \
    transform-system.cpp \
    transform-demo.cpp \
    main-transform-demo.cpp

HEADERS += \
    transform-system.h \
    transform-demo.h

# 默认规则
unix {
    target.path = /usr/local/bin
    INSTALLS += target
}
