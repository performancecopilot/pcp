TEMPLATE	= app
LANGUAGE	= C++
HEADERS		= pmgadgets.h tokens.h
SOURCES		= pmgadgets.cpp main.cpp parse.cpp
FLEXSOURCES	= lex.l
ICON		= pmgadgets.icns
RESOURCES	= pmgadgets.qrc
CONFIG		+= qt warn_on
INCLUDEPATH	+= ../include ../libpcp_qed/src
LIBS		+= -L../libpcp_qed/src/build/release -L../libpcp_qmc/src/$$DESTDIR
LIBS		+= -lpcp -lpcp_qed
win32:LIBS	+= -lwsock32 -liphlpapi
QT		+= network widgets
QMAKE_INFO_PLIST = pmgadgets.info
QMAKE_EXTRA_COMPILERS += flex
QMAKE_CFLAGS	+= $$(CFLAGS)
QMAKE_CXXFLAGS	+= $$(CFLAGS) $$(CXXFLAGS)
QMAKE_LFLAGS	+= $$(LDFLAGS)
CONFIG(release, release|debug) {
DESTDIR = build/release
}
CONFIG(debug, release|debug) {
DESTDIR   = build/debug
}

flex.commands = flex ${QMAKE_FILE_IN}
flex.input = FLEXSOURCES
flex.output = lex.yy.c
flex.variable_out = SOURCES
flex.depends = tokens.h
flex.name = flex
