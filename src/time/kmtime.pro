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
RESOURCES	= kmtime.qrc
LIBS		= -lpcp -lqwt -lqassistantclient
INCLUDEPATH	+= ../include /usr/include/qwt
CONFIG		+= qt warn_on release
QT		+= assistant network
