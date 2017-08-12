#-------------------------------------------------
#
# Project created by QtCreator 2013-09-25T09:11:42
#
#-------------------------------------------------

QT       += core widgets gui opengl
CONFIG   += c++11 force_debug_info


TARGET = MyOpenGL
TEMPLATE = app


SOURCES += main.cpp\
        window.cpp \
    ink_layer_glwidget.cpp \
    ink_data.cpp \
    ink_stroke.cpp

HEADERS  += window.h \
    ink_layer_glwidget.h \
    ink_data.h \
    ink_stroke.h

FORMS    += window.ui

LIBS += -lopengl32
