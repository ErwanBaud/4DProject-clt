#-------------------------------------------------
#
# Project created by QtCreator 2014-11-26T21:42:33
#
#-------------------------------------------------

QT       += core widgets network

QT       -= gui

QMAKE_CXXFLAGS += -fpermissive

TARGET = cltCore
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    ClientCore.cpp \
    Simu.cpp \
    SslServer.cpp

HEADERS += \
    ClientCore.h \
    Simu.h \
    SslServer.h
