TOPDIR = ../../..
LIBRARY = libpcp_qwt
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)
LSRCFILES = $(SOURCES) $(HEADERS) $(PROJECT)

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
