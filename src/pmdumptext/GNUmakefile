TOPDIR = ../..
COMMAND = pmdumptext
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

QRCFILE = $(COMMAND).qrc
UIFILES = $(shell echo *.ui)
SOURCES = pmdumptext.cpp
CONFFILES = $(PROJECT)
LSRCFILES = $(PROJECT).in $(SOURCES)
LDIRT = $(COMMAND)

default:
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
ifeq ($(WINDOW),mac)
	$(call INSTALL_QT_FRAMEWORKS,$(BINARY))
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
	rm $(BINARY)
else
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
