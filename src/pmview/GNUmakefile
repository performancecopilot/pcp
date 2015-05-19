TOPDIR = ../..
COMMAND = pmview
PROJECT = $(COMMAND).pro
include $(TOPDIR)/src/include/builddefs

WRAPPER = $(COMMAND).sh
QRCFILE = $(COMMAND).qrc
ICNFILE = $(COMMAND).icns
ICOFILE = $(COMMAND).ico
XMLFILE = $(COMMAND).info
DESKTOP = $(COMMAND).desktop
UIFILES = $(shell echo *.ui)
CLASSES = main.h pmview.h colorlist.h \
	  barmod.h barobj.h baseobj.h \
	  defaultobj.h gridobj.h labelobj.h stackobj.h \
	  text.h viewobj.h pipeobj.h link.h xing.h \
	  scenefileobj.h scenegroup.h \
	  colorscalemod.h colormod.h colorscale.h \
	  metriclist.h modlist.h modulate.h \
	  scalemod.h stackmod.h togglemod.h \
	  yscalemod.h pcpcolor.h launch.h
SOURCES = $(CLASSES:.h=.cpp) error.cpp
HEADERS = $(CLASSES) modobj.h
GENERATED = gram.cpp lex.cpp 
LFILES = lex.l
YFILES = gram.y
LDIRT = $(COMMAND) $(WRAPPER) $(XMLFILE) $(GENERATED) gram.h y.tab.? images

SUBDIRS = front-ends

default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_QT)" "true"
build-me:: images wrappers $(GENERATED)
	+$(QTMAKE)
	$(LNMAKE)

build-me:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

lex.cpp:	lex.l
	$(LEX) -t lex.l > $@

gram.h y.tab.c:	gram.y
	$(YACC) -d gram.y && cp y.tab.h gram.h

gram.cpp:	y.tab.c
	cp y.tab.c $@

lex.o:	gram.h

ifeq ($(WINDOW),mac)
MACBUILD = $(COMMAND).app/Contents
PKG_MAC_DIR = /Applications/$(COMMAND).app/Contents
wrappers: $(WRAPPER)
else
wrappers:
endif

$(WRAPPER): $(WRAPPER).IN
	$(SED) -e '/\# .*/b' -e 's;PKG_MAC_DIR;$(PKG_MAC_DIR);g' < $< > $@

install: default
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PCP_BIN_DIR)
ifeq ($(WINDOW),win)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
endif
ifeq ($(WINDOW),x11)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 -d $(PKG_DESKTOP_DIR)
	$(INSTALL) -m 644 $(DESKTOP) $(PKG_DESKTOP_DIR)/$(DESKTOP)
endif
ifeq ($(WINDOW),mac)
	$(INSTALL) -m 755 $(WRAPPER) $(PKG_BIN_DIR)/$(COMMAND)
	$(INSTALL) -m 755 -d /Applications
	$(INSTALL) -m 755 -d /Applications/$(COMMAND).app
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)
	$(INSTALL) -m 644 $(MACBUILD)/Info.plist $(PKG_MAC_DIR)/Info.plist
	$(INSTALL) -m 644 $(MACBUILD)/PkgInfo $(PKG_MAC_DIR)/PkgInfo
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/MacOS
	$(INSTALL) -m 755 $(BINARY) $(PKG_MAC_DIR)/MacOS/$(COMMAND)
	$(INSTALL) -m 755 -d $(PKG_MAC_DIR)/Resources
	$(INSTALL) -m 644 $(ICNFILE) $(PKG_MAC_DIR)/Resources/$(ICNFILE)
endif

else
build-me:
install:
endif

default_pcp:	default

install_pcp:	install

images: $(ICNFILE)
	$(LN_S) $(TOPDIR)/images images

$(ICNFILE):
	$(LN_S) $(TOPDIR)/images/$(ICNFILE) $(ICNFILE)
