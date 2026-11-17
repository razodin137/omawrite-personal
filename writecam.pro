QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus network multimedia

CONFIG += c++17 release
TARGET = writecam
TEMPLATE = app

HEADERS += \
    src/aiservice.h \
    src/alternativetone.h \
    src/backend.h \
    src/cli.h \
    src/markdownhighlighter.h \
    src/systemone.h \
    src/systemtheme.h \
    src/toneplayer.h \
    src/writecamstate.h

SOURCES += \
    src/main.cpp \
    src/aiservice.cpp \
    src/alternativetone.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/markdownhighlighter.cpp \
    src/systemone.cpp \
    src/systemtheme.cpp \
    src/toneplayer.cpp \
    src/writecamstate.cpp

RESOURCES += src/resources.qrc
