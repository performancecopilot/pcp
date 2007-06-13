#
# Copyright (c) 2007 Nathan Scott.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

UIFILES = aboutdialog.ui aboutpcpdialog.ui console.ui \
	kmtimearch.ui kmtimelive.ui showboundsdialog.ui
UIHFILES = $(UIFILES:.ui=.ui.h)
CPPFILES = main.cpp
HPPFILES = $(CPPFILES:.cpp=.h)
HEADERS = timelord.h timezone.h constants.h

LSRCFILES = kmtime.pro $(UIFILES) $(UIHFILES) $(CPPFILES) $(HPPFILES)
LDIRT = kmtime Makefile

default:
	qmake
	make -f Makefile

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 kmtime $(PKG_BIN_DIR)

fix:	# ugh!  -- TODO: remove need for this, somehow..
	perl -i -pe 's,<action name="optionsTimezoneAction"/>,<action name="optionsTimezoneAction"/>\n\t<item text="&amp;Timezone" name="Timezone" accel="">\n\t</item>,g' kmtimearch.ui
	perl -i -pe 's,<action name="optionsTimezoneAction"/>,<action name="optionsTimezoneAction"/>\n\t<item text="&amp;Timezone" name="Timezone" accel="">\n\t</item>,g' kmtimelive.ui
