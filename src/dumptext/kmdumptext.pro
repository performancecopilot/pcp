TEMPLATE	= app
LANGUAGE	= C++
SOURCES		= kmdumptext.cpp
INCLUDEPATH	+= ../include ../libqmc
LIBS		= -lpcp -lqmc
LIBS		+= -L../libqmc -L../libqmc/build/Default
CONFIG		+= qt warn_on debug #release
