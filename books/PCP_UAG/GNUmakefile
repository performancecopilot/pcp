TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

IAM = user-administrator-guide
XMLFILES = $(IAM).xml
PDFFILES = $(IAM).pdf

SUBDIRS = figures xml
LSRCFILES = $(XMLFILES) publican.cfg Book_Info.xml
LDIRT = $(PDFFILES)

default: build-me

include $(BUILDRULES)

ifeq "$(BOOK_TOOLCHAIN)" "publican"
build-me: build.done

build.done: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(LN_S) . en-US
	$(PUBLICAN) build --langs=en-US --formats=pdf
	touch build.done

install: default
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PKG_BOOKS_DIR)
	$(INSTALL) -m 644 $(shell echo pdf/*.pdf) $(PKG_BOOKS_DIR)/$(IAM).pdf
endif

ifneq "$(findstring $(BOOK_TOOLCHAIN),dblatext xmlto)" ""
build-me: built
	$(SUBDIRS_MAKERULE)

built:	$(IAM).pdf
	touch built

install: default
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PKG_BOOKS_DIR)
	$(INSTALL) -m 644 pdf/$(IAM).pdf $(PKG_BOOKS_DIR)/$(IAM).pdf
endif

ifeq "$(findstring $(BOOK_TOOLCHAIN),publican dblatext xmlto)" ""
build-me:
	$(SUBDIRS_MAKERULE)
install:
	$(SUBDIRS_MAKERULE)
endif
