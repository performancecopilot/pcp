TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LIBRARY = libqmc
PROJECT = $(LIBRARY).pro
HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)

LSRCFILES = $(PROJECT).in $(SOURCES) $(HEADERS)
LDIRDIRT = build debug release qmc.xcodeproj
LDIRT = *.a $(PROJECT)

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
