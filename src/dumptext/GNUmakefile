TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmdumptext
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc

UIFILES = $(shell echo *.ui)
SOURCES = kmdumptext.cpp

LSRCFILES = $(PROJECT).in $(SOURCES)
LDIRT = $(COMMAND)

default:
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
