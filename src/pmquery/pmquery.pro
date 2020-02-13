TEMPLATE	= app
LANGUAGE	= C++
HEADERS		= pmquery.h
SOURCES		= pmquery.cpp main.cpp
ICON		= pmquery.icns
RESOURCES	= pmquery.qrc
CONFIG		+= qt warn_on
QT		+= widgets
CONFIG(release, release|debug) {
DESTDIR	= build/release
}
CONFIG(debug, release|debug) {
DESTDIR	= build/debug
}
QMAKE_CFLAGS	+= $$(CFLAGS)
QMAKE_CXXFLAGS	+= $$(CFLAGS) $$(CXXFLAGS)
QMAKE_LFLAGS	+= $$(LDFLAGS)
