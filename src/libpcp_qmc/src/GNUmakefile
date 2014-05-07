TOPDIR = ../../..
LIBRARY = libpcp_qmc
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)
LSRCFILES = $(SOURCES) $(HEADERS) $(PROJECT)

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
