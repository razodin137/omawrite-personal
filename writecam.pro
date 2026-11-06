QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus network

CONFIG += c++17 release
TARGET = writecam
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/markdownhighlighter.h \
    src/systemone.h \
    src/systemtheme.h \
    src/writecamstate.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/markdownhighlighter.cpp \
    src/systemone.cpp \
    src/systemtheme.cpp \
    src/writecamstate.cpp

RESOURCES += src/resources.qrc
