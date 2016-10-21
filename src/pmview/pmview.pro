TEMPLATE	= app
LANGUAGE	= C++
HEADERS		= main.h pmview.h colorlist.h \
		  barmod.h barobj.h baseobj.h \
		  defaultobj.h gridobj.h labelobj.h stackobj.h \
		  launch.h viewobj.h pipeobj.h link.h xing.h \
		  scenefileobj.h scenegroup.h \
		  colorscalemod.h colormod.h colorscale.h \
		  metriclist.h modlist.h modulate.h \
		  scalemod.h stackmod.h togglemod.h \
		  text.h yscalemod.h pcpcolor.h
SOURCES		= main.cpp colorlist.cpp barmod.cpp barobj.cpp baseobj.cpp \
		  defaultobj.cpp gridobj.cpp labelobj.cpp stackobj.cpp \
		  launch.cpp viewobj.cpp pipeobj.cpp link.cpp xing.cpp \
		  scenefileobj.cpp scenegroup.cpp \
		  colorscalemod.cpp colormod.cpp colorscale.cpp \
		  scalemod.cpp stackmod.cpp togglemod.cpp yscalemod.cpp \
		  metricList.cpp modlist.cpp modulate.cpp pcpcolor.cpp \
		  text.cpp error.cpp gram.cpp lex.cpp pmview.cpp
FORMS		= pmview.ui
ICON		= pmview.icns
RC_FILE		= pmview.rc
RESOURCES	= pmview.qrc
INCLUDEPATH	+= /usr/include/Coin2
INCLUDEPATH	+= ../include ../libpcp_qmc/src ../libpcp_qed/src
CONFIG		+= qt warn_on
release:DESTDIR	= build/debug
debug:DESTDIR	= build/release
LIBS		+= -L../libpcp/src
LIBS		+= -L../libpcp_qmc/src -L../libpcp_qmc/src/$$DESTDIR
LIBS		+= -L../libpcp_qed/src -L../libpcp_qed/src/$$DESTDIR
LIBS		+= -lpcp_qed -lpcp_qmc -lpcp -lCoin -lSoQt
win32:LIBS	+= -lwsock32 -liphlpapi
QT		+= network
QMAKE_INFO_PLIST = pmview.info
QMAKE_CXXFLAGS	+= $$(PCP_CFLAGS)
