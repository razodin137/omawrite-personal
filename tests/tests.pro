QT += core gui concurrent quick testlib network
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_writecam

INCLUDEPATH += ../src
SOURCES += \
    tst_writecam.cpp \
    ../src/alternativetone.cpp \
    ../src/backend.cpp \
    ../src/cli.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/systemone.cpp \
    ../src/writecamstate.cpp
HEADERS += \
    ../src/alternativetone.h \
    ../src/backend.h \
    ../src/cli.h \
    ../src/markdownhighlighter.h \
    ../src/systemone.h \
    ../src/writecamstate.h

RESOURCES += ../src/resources.qrc

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
