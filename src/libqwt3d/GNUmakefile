TOPDIR = ../..
LIBRARY = libqwt3d
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.c *.cpp)
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(SOURCES) $(HEADERS)

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
