TARGET		= qwt3d
TEMPLATE	= lib
VERSION		= 0.2.6
CONFIG		+= qt static lib warn_on opengl release #debug
QT		+= opengl

HEADERS += \
    qwt3d_color.h \
    qwt3d_global.h \
    qwt3d_types.h \
    qwt3d_axis.h \
    qwt3d_coordsys.h \
    qwt3d_drawable.h \
    qwt3d_helper.h \
    qwt3d_label.h \
    qwt3d_openglhelper.h \
    qwt3d_colorlegend.h \
    qwt3d_plot.h \
    qwt3d_enrichment.h \
    qwt3d_enrichment_std.h \
    qwt3d_autoscaler.h \
    qwt3d_autoptr.h \
    qwt3d_io.h \
    qwt3d_io_reader.h \
    qwt3d_scale.h \
    qwt3d_portability.h \
    qwt3d_mapping.h

HEADERS += \
    qwt3d_gridmapping.h \
    qwt3d_parametricsurface.h \
    qwt3d_function.h

HEADERS += \
    qwt3d_surfaceplot.h \
    qwt3d_volumeplot.h \
    qwt3d_graphplot.h \
    qwt3d_multiplot.h

HEADERS += \
    qwt3d_io_gl2ps.h \
    gl2ps.h
         
SOURCES += \
    qwt3d_axis.cpp \
    qwt3d_color.cpp \
    qwt3d_coordsys.cpp \
    qwt3d_drawable.cpp \
    qwt3d_mousekeyboard.cpp \
    qwt3d_movements.cpp \
    qwt3d_lighting.cpp \
    qwt3d_colorlegend.cpp \
    qwt3d_plot.cpp \
    qwt3d_label.cpp \
    qwt3d_types.cpp \
    qwt3d_enrichment_std.cpp \
    qwt3d_autoscaler.cpp \
    qwt3d_io_reader.cpp \
    qwt3d_io.cpp \
    qwt3d_scale.cpp

SOURCES += \
    qwt3d_gridmapping.cpp \
    qwt3d_parametricsurface.cpp \
    qwt3d_function.cpp

SOURCES += \
    qwt3d_surfaceplot.cpp \
    qwt3d_gridplot.cpp \
    qwt3d_meshplot.cpp

SOURCES += \
    qwt3d_io_gl2ps.cpp \
    gl2ps.c
