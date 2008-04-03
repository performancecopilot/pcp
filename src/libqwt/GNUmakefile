TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LIBRARY = libqwt
PROJECT = $(LIBRARY).pro
HEADERS = $(shell echo *.h)
SOURCES = $(shell echo *.cpp)

LSRCFILES = $(PROJECT) $(SOURCES) $(HEADERS)
LDIRDIRT = build qwt.xcodeproj
LDIRT = *.a

default: $(PROJECT)
	$(QTMAKE)

include $(BUILDRULES)

install: default
