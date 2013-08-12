TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

IAM = programmer-guide
XML = $(IAM).xml
PUB = Book_Info.xml
CFG = publican.cfg
PDF = pdf/$(IAM).pdf

LSRCFILES = $(XML) $(PUB) $(CFG)
LDIRDIRT = pdf html en-US
CWD = $(shell pwd)

default: build-me

include $(BUILDRULES)

ifeq "$(BOOK_TOOLCHAIN)" "publican"
built.$(BOOK_TOOLCHAIN):	$(XML)
	@rm -fr pdf html en-US
	@mkdir -p pdf html en-US/xml
	$(LN_S) $(CWD)/$(PUB) en-US/
	$(LN_S) $(CWD)/$(XML) en-US/xml/
	$(PUBLICAN) build --langs=en-US --formats=pdf
	@# $(PUBLICAN) build --langs=en-US --formats=pdf,html
	$(LN_S) $(CWD)/en-US/pdf/*.pdf pdf/
endif

ifeq "$(BOOK_TOOLCHAIN)" "dblatex"
built.$(BOOK_TOOLCHAIN):	$(XML)
	@rm -fr pdf
	$(DBLATEX) --type=pdf --output-dir=pdf $(XML)
endif

ifeq "$(BOOK_TOOLCHAIN)" "xmlto"
built.$(BOOK_TOOLCHAIN):	$(XML)
	@rm -fr pdf html
	$(XMLTO) --with-fop -o pdf pdf $(XML)
	@# $(XMLTO) --with-fop -o html html $(XML)
endif
ifneq "$(findstring $(BOOK_TOOLCHAIN),publican dblatext xmlto)" ""
build-me: built.$(BOOK_TOOLCHAIN)
	@touch built.$(BOOK_TOOLCHAIN)

install: default
	$(INSTALL) -m 755 -d $(PKG_BOOKS_DIR)
	$(INSTALL) -m 644 $(PDFFILES) $(PKG_BOOKS_DIR)/$(IAM).pdf
else
build-me:
install:
endif
