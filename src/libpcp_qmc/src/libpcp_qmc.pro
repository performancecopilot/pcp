TARGET		= pcp_qmc
TEMPLATE	= lib
VERSION		= 1.0.0
CONFIG		+= qt staticlib warn_on
INCLUDEPATH	+= ../../include
release:DESTDIR = build/debug
debug:DESTDIR   = build/release
QMAKE_CXXFLAGS	+= $$(PCP_CFLAGS)

HEADERS	= qmc_context.h qmc_desc.h qmc_group.h \
	  qmc_indom.h qmc_metric.h qmc_source.h \
	  qmc_time.h

SOURCES = qmc_context.cpp qmc_desc.cpp qmc_group.cpp \
	  qmc_indom.cpp qmc_metric.cpp qmc_source.cpp \
	  qmc_time.cpp
