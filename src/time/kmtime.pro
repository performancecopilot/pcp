TEMPLATE	= app
LANGUAGE	= C++
HEADERS		= aboutdialog.h console.h kmtimearch.h kmtimelive.h \
		  seealsodialog.h showboundsdialog.h \
		  timelord.h timezone.h
SOURCES		= aboutdialog.cpp console.cpp kmtimearch.cpp kmtimelive.cpp \
		  seealsodialog.cpp showboundsdialog.cpp \
		  timelord.cpp main.cpp
FORMS		= aboutdialog.ui console.ui kmtimelive.ui kmtimearch.ui \
		  seealsodialog.ui showboundsdialog.ui
ICON		= kmtime.icns
RESOURCES	= kmtime.qrc
INCLUDEPATH	+= ../include ../libqwt
LIBS		= -lpcp -lqwt
LIBS		+= -L../libqwt -L../libqwt/build/Default
CONFIG		+= qt warn_on release
QT		+= assistant network
