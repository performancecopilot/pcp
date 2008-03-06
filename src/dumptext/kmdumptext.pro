TEMPLATE	= app
LANGUAGE	= C++
SOURCES		= kmdumptext.cpp
INCLUDEPATH	+= ../include ../libqmc
LIBS		= -lpcp -lqmc
LIBS		+= -L../libqmc -L../libqmc/build/Default
QT		-= gui
CONFIG		+= qt warn_on release #debug
