TOPDIR = ../..
COMMAND = kmdumptext
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

QRCFILE = $(COMMAND).qrc
UIFILES = $(shell echo *.ui)
SOURCES = kmdumptext.cpp
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(SOURCES)

default:
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
