TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = src archives views interact
BASEDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))
TESTDIR = $(BASEDIR)/testsuite

SCRIPTS = check new remake
COMMONS = $(shell echo common.*)
AUTOTEST = $(shell echo [0-9][0-9][0-9])
OUTFILES = $(shell echo *.out)
FOLIOS = 20071023-08-06-36.folio
VIEWS = 20071023-08-06-36.view
LSRCFILES = $(COMMONS) $(SCRIPTS) \
	    $(AUTOTEST) $(OUTFILES) \
	    $(FOLIOS) $(VIEWS) group GNUmakefile.install

LDIRT = check.log check.time pmlogger.log *.out.bad *.full

default setup: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(BASEDIR)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(FOLIOS) $(VIEWS) group $(TESTDIR)
	$(INSTALL) -m 755 $(AUTOTEST) $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 $(COMMONS) $(OUTFILES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
