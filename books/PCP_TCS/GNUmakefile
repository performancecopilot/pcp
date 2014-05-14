TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

IAM = pcp-tutorials-and-case-studies
XML = $(IAM).xml
PDF = #$(IAM).pdf
PUB = Book_Info.xml
CFG = publican.cfg

LSRCFILES = $(XML) $(PUB) $(CFG) $(PDF)
LDIRDIRT = pdf html en-US
LDIRT = built.*
CWD = $(shell pwd)

default: build-me

include $(BUILDRULES)

ifeq "$(BOOK_TOOLCHAIN)" "publican"
built.$(BOOK_TOOLCHAIN):	$(XML)
	@rm -fr pdf html en-US
	@mkdir -p pdf html en-US/xml
	$(LN_S) $(CWD)/$(PUB) en-US/
	$(LN_S) $(CWD)/$(XML) en-US/xml/
	$(LN_S) $(CWD)/$(TOPDIR)/images en-US/xml/figures
	$(PUBLICAN) build --langs=en-US --formats=pdf
	@# $(PUBLICAN) build --langs=en-US --formats=pdf,html
	$(LN_S) $(CWD)/en-US/pdf/*.pdf pdf/$(IAM).pdf
endif

ifeq "$(BOOK_TOOLCHAIN)" "dblatex"
built.$(BOOK_TOOLCHAIN):	$(XML)
	@rm -fr pdf figures
	$(LN_S) $(CWD)/$(TOPDIR)/images figures
	$(DBLATEX) --type=pdf --output-dir=pdf $(XML)
endif

ifeq "$(BOOK_TOOLCHAIN)" "xmlto"
built.$(BOOK_TOOLCHAIN):	$(XML)
	@rm -fr pdf html figures
	@mkdir -p pdf html
	$(LN_S) $(CWD)/$(TOPDIR)/images pdf/figures
	$(LN_S) $(CWD)/$(TOPDIR)/images html/figures
	$(XMLTO) --with-fop -o pdf pdf $(XML)
	$(XMLTO) --with-fop -o html html $(XML)
endif

ifneq "$(findstring $(BOOK_TOOLCHAIN),publican dblatext xmlto)" ""
build-me: built.$(BOOK_TOOLCHAIN)
	@touch built.$(BOOK_TOOLCHAIN)
else
build-me:
endif

install: default
#	$(INSTALL) -m 755 -d $(PCP_BOOKS_DIR)
#	$(INSTALL) -m 644 $(PDF) $(PCP_BOOKS_DIR)/$(PDF)

default_pcp : default

install_pcp : install
