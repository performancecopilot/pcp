TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

IAM = pcp-users-and-administrators-guide
REV = Revision_History.xml
AUT = Author_Group.xml
XML = $(IAM).xml
PDF = $(IAM).pdf
ENT = $(IAM).ent
PUB = Book_Info.xml
CFG = publican.cfg

CP = cp -rdp
LSRCFILES = $(XML) $(PUB) $(CFG) $(PDF)
LDIRDIRT = pdf html en-US tmp
LDIRT = built.* publican.cfg
CWD = $(shell pwd)

default: build-me

include $(BUILDRULES)

ifeq "$(BOOK_TOOLCHAIN)" "publican"
built.$(BOOK_TOOLCHAIN):	$(XML) $(CFG)
	@rm -fr pdf html en-US tmp
	@mkdir -p pdf html en-US tmp
	$(CP) $(CWD)/$(PUB) en-US/
	$(CP) $(CWD)/$(ENT) en-US/
	$(CP) $(CWD)/$(REV) en-US/
	$(CP) $(CWD)/$(AUT) en-US/
	$(CP) $(CWD)/$(XML) en-US/
	$(CP) $(CWD)/$(TOPDIR)/images en-US/
	$(PUBLICAN) build --langs=en-US --formats=pdf
	$(PUBLICAN) build --langs=en-US --formats=html,html-single
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

publican.cfg : publican.cfg.in
	$(SED) -e 's;@brand@;'$(BOOK_BRAND)';' $< > $@

install: default
	$(INSTALL) -m 755 -d $(PCP_BOOKS_DIR)
	$(INSTALL) -m 644 $(PDF) $(PCP_BOOKS_DIR)/$(PDF)

default_pcp : default

install_pcp : install
