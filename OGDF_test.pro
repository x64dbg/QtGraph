#-------------------------------------------------
#
# Project created by QtCreator 2015-07-09T00:47:32
#
#-------------------------------------------------

QT       += core gui

QMAKE_CFLAGS_RELEASE += /MT

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = OGDF_test
TEMPLATE = app

INCLUDEPATH += "C:/OGDF/include"
LIBS += -lcoin -logdf -L"c:/OGDF/Win32/Release"


SOURCES += main.cpp\
        MainWindow.cpp

HEADERS  += MainWindow.h

FORMS    += MainWindow.ui
