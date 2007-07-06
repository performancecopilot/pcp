#
# Copyright (c) 2007 Nathan Scott.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

UIFILES = aboutdialog.ui seealsodialog.ui \
	chartdialog.ui hostdialog.ui infodialog.ui \
	kmchart.ui recorddialog.ui settingsdialog.ui tabdialog.ui
UIHFILES = $(UIFILES:.ui=.ui.h)
CPPFILES = chart.cpp main.cpp namespace.cpp qcolorpicker.cpp \
	source.cpp tab.cpp timeaxis.cpp timecontrol.cpp view.cpp
HPPFILES = $(CPPFILES:.cpp=.h)
SUBDIRS = views pmafm
LSRCFILES = kmchart.pro $(UIFILES) $(UIHFILES) $(CPPFILES) $(HPPFILES)
LDIRT = kmchart Makefile

default:
	$(QMAKE)
	@perl -i -pe 's/ -Wall / -Wall -Wno-deprecated /g' Makefile
	$(MAKE) -f Makefile

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 kmchart $(PKG_BIN_DIR)
