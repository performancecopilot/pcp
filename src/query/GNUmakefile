TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmquery
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc

HEADERS = kmquery.h
SOURCES = $(HEADERS:.h=.cpp)
SCRIPTS = kmconfirm.sh kmmessage.sh

LSRCFILES = $(PROJECT) $(QRCFILE) $(SCRIPTS) $(HEADERS) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) $(ICONLINKS)

ICONLINKS = dialog-archive.png dialog-error.png dialog-host.png \
	    dialog-information.png dialog-question.png dialog-warning.png

default: iconlinks
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 kmconfirm.sh $(PKG_BIN_DIR)/kmconfirm
	$(INSTALL) -m 755 kmmessage.sh $(PKG_BIN_DIR)/kmmessage
