TARGET		= qmc
TEMPLATE	= lib
VERSION		= 1.0.0
CONFIG		= qt warn_on debug #release
CONFIG		+= dll
CONFIG		+= staticlib

HEADERS	= qmc_context.h qmc_desc.h qmc_group.h \
	  qmc_indom.h qmc_metric.h qmc_source.h

SOURCES = qmc_context.cpp qmc_desc.cpp qmc_group.cpp \
	  qmc_indom.cpp qmc_metric.cpp qmc_source.cpp
