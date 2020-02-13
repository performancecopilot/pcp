TEMPLATE        = app
LANGUAGE        = C++
SOURCES         = qmc_format.cpp
CONFIG          += qt warn_on
INCLUDEPATH     += ../../../src/include
INCLUDEPATH     += ../../../src/libpcp_qmc/src
CONFIG(release, release|debug) {
DESTDIR	= build/release
}
CONFIG(debug, release|debug) {
DESTDIR	= build/debug
}
LIBS            += -L../../../src/libpcp/src
LIBS            += -L../../../src/libpcp_qmc/src
LIBS            += -L../../../src/libpcp_qmc/src/$$DESTDIR
LIBS            += -lpcp_qmc -lpcp
QT		-= gui
QMAKE_CFLAGS	+= $$(CFLAGS)
QMAKE_CXXFLAGS	+= $$(CFLAGS) $$(CXXFLAGS)
QMAKE_LFLAGS	+= $$(LDFLAGS)
