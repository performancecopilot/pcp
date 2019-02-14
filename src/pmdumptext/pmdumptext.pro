TEMPLATE	= app
LANGUAGE	= C++
SOURCES		= pmdumptext.cpp
CONFIG		+= qt console warn_on
INCLUDEPATH	+= ../include ../include/pcp ../libpcp_qmc/src
CONFIG(release, release|debug) {
DESTDIR	= build/release
}
CONFIG(debug, release|debug) {
DESTDIR	= build/debug
}
LIBS		+= -L../libpcp/src
LIBS		+= -L../libpcp_qmc/src -L../libpcp_qmc/src/$$DESTDIR
LIBS		+= -lpcp_qmc -lpcp
win32:LIBS	+= -lwsock32 -liphlpapi
QT		-= gui
QMAKE_CFLAGS	+= $$(CFLAGS)
QMAKE_CXXFLAGS	+= $$(CFLAGS) $$(CXXFLAGS)
QMAKE_LFLAGS	+= $$(LDFLAGS)
