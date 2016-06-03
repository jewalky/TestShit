#-------------------------------------------------
#
# Project created by QtCreator 2016-05-29T02:30:38
#
#-------------------------------------------------

QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TestShit
TEMPLATE = app

LIBS += -lglu32 -lopengl32

SOURCES += main.cpp\
        mainwindow.cpp \
    data/doommap.cpp \
    data/wadfile.cpp \
    openmapdialog.cpp \
    view2d.cpp \
    glarray.cpp \
    view3d.cpp \
    data/texman.cpp

HEADERS  += mainwindow.h \
    data/doommap.h \
    data/wadfile.h \
    openmapdialog.h \
    view2d.h \
    glarray.h \
    view3d.h \
    data/texman.h

FORMS    += mainwindow.ui \
    openmapdialog.ui

include(../QxPoly2Tri/QxPoly2Tri.pri)

RESOURCES += \
    resources.qrc
