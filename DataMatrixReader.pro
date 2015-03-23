#-------------------------------------------------
#
# Project created by QtCreator 2011-02-12T13:12:48
#
#-------------------------------------------------

QT       += core gui

TARGET = DataMatrixReader
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    datamatrixreader.cpp \
    stepdata.cpp \
    run.cpp \
    runmanager.cpp \
    infoabout.cpp \
    houghresultdata.cpp \
    blockdata.cpp \
    showimage.cpp \
    iplimageconverter.cpp

HEADERS  += mainwindow.h \
    datamatrixreader.h \
    stepdata.h \
    run.h \
    runmanager.h \
    infoabout.h \
    houghresultdata.h \
    blockdata.h \
    showimage.h \
    iplimageconverter.h

FORMS    += mainwindow.ui \
    infoabout.ui \
    showimage.ui

INCLUDEPATH += D:/Fabian/Studium/BV/OpenCV2.2/include/opencv D:/Fabian/Studium/BV/OpenCV2.2/include

LIBS += D:/Fabian/Studium/BV/OpenCV2.2/lib/*.lib

RESOURCES += \
    Images.qrc
