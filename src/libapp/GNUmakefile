TOPDIR = ../..
LIBRARY = libapp
PROJECT = $(LIBRARY).pro
include $(TOPDIR)/src/include/builddefs

HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)
QRCFILE = $(shell echo *.qrc)
UIFILES = $(shell echo *.ui)
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(SOURCES) $(HEADERS) $(UIFILES) $(QRCFILE)

SUBDIRS = views images

default: $(PROJECT)
	$(SUBDIRS_MAKERULE)
	$(QTMAKE)

include $(BUILDRULES)

install: default
	$(SUBDIRS_MAKERULE)
