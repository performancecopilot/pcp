TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COMMAND = kmdumptext
PROJECT = $(COMMAND).pro
QRCFILE = $(COMMAND).qrc

UIFILES = $(shell echo *.ui)
SOURCES = kmdumptext.cpp

LSRCFILES = $(PROJECT) $(SOURCES)
LDIRDIRT = build $(COMMAND).xcodeproj
LDIRT = $(COMMAND)

LCFLAGS = -I$(TOPDIR)/libqmc

ifeq ($(PKG_PLATFORM),darwin)
LLDFLAGS = -L$(TOPDIR)/libqmc
else
LLDFLAGS = -L$(TOPDIR)/libqmc/build/Default
endif

default:
	$(QTMAKE)
	$(LNMAKE)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(INSTALL) -m 755 $(BINARY) $(PKG_BIN_DIR)/$(COMMAND)
