# Add more folders to ship with the application, here
folder_01.source = qml/qtcam
folder_01.target = qml
DEPLOYMENTFOLDERS = folder_01

QT += widgets
TARGET = Qtcam

CONFIG += release

# Additional import path used to resolve QML modules in Creator's code model
QML_IMPORT_PATH =

# The .cpp file which was generated for your project. Feel free to hack it.
SOURCES += main.cpp \
    logger.cpp \
    about.cpp \
    cameraproperty.cpp \
    videostreaming.cpp \
    uvccamera.cpp \
    v4l2-api.cpp \
    seecam_10cug_m.cpp \
    seecam_10cug_bayer.cpp \
    seecam_11cug.cpp \
    seecam_cu80.cpp \
    seecam_cu50.cpp \
    videoencoder.cpp

# Installation path
# target.path =

# Please do not modify the following two lines. Required for deployment.
include(qtquick2applicationviewer/qtquick2applicationviewer.pri)
qtcAddDeployment()


HEADERS += \
    logger.h \
    about.h \
    cameraproperty.h \
    videostreaming.h \
    uvccamera.h \
    v4l2-api.h \
    seecam_10cug_m.h \
    seecam_10cug_bayer.h \
    seecam_11cug.h \
    seecam_cu80.h \
    seecam_cu50.h \
    videoencoder.h

INCLUDEPATH +=  $$PWD/v4l2headers/include \
                $$PWD/include

LIBS += -lv4l2 -lv4lconvert \                
        -ludev \
        -L$$PWD/lib -lavutil -lavcodec -lavformat -lswscale -lavresample


QMAKE_CFLAGS_THREAD = -D__STDC_CONSTANT_MACROS      #For Ubuntu 12.04 compilation
QMAKE_CXXFLAGS_THREAD = -D__STDC_CONSTANT_MACROS    #For Ubuntu 12.04 compilation
