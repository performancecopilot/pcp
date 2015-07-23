TEMPLATE	= app
LANGUAGE	= C++
HEADERS		= console.h pmtime.h pmtimearch.h pmtimelive.h \
		  aboutdialog.h seealsodialog.h showboundsdialog.h \
		  timelord.h timezone.h
SOURCES		= console.cpp pmtime.cpp pmtimearch.cpp pmtimelive.cpp \
		  aboutdialog.cpp seealsodialog.cpp showboundsdialog.cpp \
		  timelord.cpp main.cpp
FORMS		= aboutdialog.ui console.ui pmtimelive.ui pmtimearch.ui \
		  seealsodialog.ui showboundsdialog.ui
ICON		= pmtime.icns
RC_FILE		= pmtime.rc
RESOURCES	= pmtime.qrc
CONFIG		+= qt warn_on
INCLUDEPATH	+= ../include ../libpcp_qwt/src ../libpcp_qmc/src
release:DESTDIR	= build/debug
debug:DESTDIR	= build/release
LIBS		+= -L../libpcp/src
LIBS		+= -L../libpcp_qwt/src -L../libpcp_qwt/src/$$DESTDIR
LIBS		+= -L../libpcp_qmc/src -L../libpcp_qmc/src/$$DESTDIR
LIBS		+= -lpcp_qwt -lpcp_qmc -lpcp
win32:LIBS	+= -lwsock32 -liphlpapi
QT		+= network
QMAKE_INFO_PLIST = pmtime.info
