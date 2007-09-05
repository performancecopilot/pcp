TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmquery

SOURCES = main.cpp kmquery.cpp
HEADERS = $(CPPFILES:.cpp=.h)
SCRIPTS = kmconfirm.sh kmmessage.sh
LSRCFILES = kmquery.pro kmquery.qrc $(SCRIPTS) $(SOURCES) $(HEADERS)
ICONLINKS = dialog-archive.png dialog-error.png dialog-host.png \
	    dialog-information.png dialog-question.png dialog-warning.png
LDIRT = Makefile $(COMMAND) $(ICONLINKS)

default: iconlinks
	$(QTMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(COMMAND) $(PKG_BIN_DIR)
	$(INSTALL) -m 755 kmconfirm.sh $(PKG_BIN_DIR)/kmconfirm
	$(INSTALL) -m 755 kmmessage.sh $(PKG_BIN_DIR)/kmmessage
