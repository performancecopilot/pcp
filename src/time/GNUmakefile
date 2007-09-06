TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmtime
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc

UIFILES = $(shell echo *.ui)
HEADERS = aboutdialog.cpp console.cpp kmtimearch.cpp kmtimelive.cpp \
	  seealsodialog.cpp showboundsdialog.cpp timelord.cpp timezone.h
SOURCES = aboutdialog.cpp console.cpp kmtimearch.cpp kmtimelive.cpp \
	  seealsodialog.cpp showboundsdialog.cpp timelord.cpp main.cpp

LSRCFILES = $(PROJECT) $(QRCFILE) $(UIFILES) $(HEADERS) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND) $(ICONLINKS)

LCFLAGS = -I$(TOPDIR)/libqwt

ifeq ($(PKG_PLATFORM),darwin)
BINARY =  build/Default/$(COMMAND).app/Contents/MacOS/$(COMMAND)
LLDFLAGS = -L$(TOPDIR)/libqwt
else
BINARY =  $(COMMAND)
LLDFLAGS = -L$(TOPDIR)/libqwt/build/Default
endif

ICONLINKS = play_off.png play_on.png fastfwd_off.png fastfwd_on.png \
	fastback_off.png fastback_on.png back_off.png back_on.png \
	stepfwd_off.png stepfwd_on.png stepback_off.png stepback_on.png \
	stop_off.png stop_on.png aboutpcp.png aboutqt.png aboutkmtime.png \
	whatsthis.png edit-clear.png internet-web-browser.png kmtime.png

default: iconlinks
	$(QTMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
