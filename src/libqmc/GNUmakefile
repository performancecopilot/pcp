TOPDIR = ../..
LIBRARY = libqmc
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(SOURCES) $(HEADERS)

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
