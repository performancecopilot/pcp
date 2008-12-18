TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LIBRARY = libqwt
PROJECT = $(LIBRARY).pro
HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)

LSRCFILES = $(PROJECT).in $(SOURCES) $(HEADERS)
LDIRT = $(PROJECT)

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
