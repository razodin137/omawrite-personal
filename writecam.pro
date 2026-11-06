QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus network

CONFIG += c++17 release
TARGET = writecam
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/markdownhighlighter.h \
    src/systemone.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/markdownhighlighter.cpp \
    src/systemone.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
