#
# Copyright (c) 2007 Nathan Scott.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

UIFILES = kmquery.ui
UIHFILES = $(UIFILES:.ui=.ui.h)
CPPFILES = main.cpp
HPPFILES = $(CPPFILES:.cpp=.h)
LSRCFILES = kmquery.pro kmconfirm.sh kmmessage.sh \
	$(UIFILES) $(UIHFILES) $(CPPFILES) $(HPPFILES)
LDIRT = kmquery Makefile

default:
	$(QMAKE)
	$(MAKE) -f Makefile

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 kmquery $(PKG_BIN_DIR)
	$(INSTALL) -m 755 kmconfirm.sh $(PKG_BIN_DIR)/kmconfirm
	$(INSTALL) -m 755 kmmessage.sh $(PKG_BIN_DIR)/kmmessage
