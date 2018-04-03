TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

IAM = pcp-programmers-guide
REV = Revision_History.xml
AUT = Author_Group.xml
XML = $(IAM).xml
PDF = $(IAM).pdf
ENT = $(IAM).ent
PUB = Book_Info.xml
CFG = publican.cfg

CP = cp -rdp
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
	# workaround publican/fop interaction issues (RHBZ 1290423)
	$(CP) $(CWD)/fop.xconf tmp/en-US/xml/
	$(LN_S) $(CWD)/tmp/en-US/xml/images $(CWD)/tmp/en-US/xml/images/images
	cd tmp/en-US/xml && fop -c fop.xconf -fo $(IAM).fo -pdf ../pdf/$(IAM).pdf
	$(LN_S) $(CWD)/tmp/en-US/pdf/$(IAM).pdf pdf/$(IAM).pdf
endif

ifneq "$(findstring $(BOOK_TOOLCHAIN),publican)" ""
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
