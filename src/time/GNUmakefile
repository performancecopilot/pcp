TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmtime

UIFILES = aboutdialog.ui console.ui kmtimearch.ui kmtimelive.ui \
	  seealsodialog.ui showboundsdialog.ui
SOURCES = aboutdialog.cpp console.cpp kmtimearch.cpp kmtimelive.cpp \
	  seealsodialog.cpp showboundsdialog.cpp timelord.cpp main.cpp
HEADERS = aboutdialog.cpp console.cpp kmtimearch.cpp kmtimelive.cpp \
	  seealsodialog.cpp showboundsdialog.cpp timelord.cpp timezone.h

LSRCFILES = kmtime.pro kmtime.qrc $(UIFILES) $(HEADERS) $(SOURCES)

ICONLINKS = play_off.png play_on.png fastfwd_off.png fastfwd_on.png \
	fastback_off.png fastback_on.png back_off.png back_on.png \
	stepfwd_off.png stepfwd_on.png stepback_off.png stepback_on.png \
	stop_off.png stop_on.png aboutpcp.png aboutqt.png aboutkmtime.png \
	whatsthis.png edit-clear.png internet-web-browser.png kmtime.png

LDIRT = Makefile $(COMMAND) $(ICONLINKS)

default: iconlinks
	$(QTMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(COMMAND) $(PKG_BIN_DIR)
