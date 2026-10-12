QT += core gui quick testlib network
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_writecam

INCLUDEPATH += ../src
SOURCES += \
    tst_writecam.cpp \
    ../src/backend.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/systemone.cpp
HEADERS += \
    ../src/backend.h \
    ../src/markdownhighlighter.h \
    ../src/systemone.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
