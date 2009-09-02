TOPDIR = ../..
LIBRARY = libqwt
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo qwt*.h)
SOURCES = $(shell echo qwt*.cpp)
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(SOURCES) $(HEADERS)

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
