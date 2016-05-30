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
    viewarea.cpp

HEADERS  += mainwindow.h \
    viewarea.h

FORMS    += mainwindow.ui
