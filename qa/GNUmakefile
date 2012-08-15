TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = src archives views interact
BASEDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))
TESTDIR = $(BASEDIR)/testsuite

COMMONS = common.cmd common.filter common.sh common.test
SCRIPTS = check new remake
AUTOTEST = $(shell echo [0-9][0-9][0-9])
OUTFILES = $(shell echo *.out)
FOLIOS = 20071023.08\:06\:36.folio
VIEWS = 20071023.08\:06\:36.view
LSRCFILES = $(COMMONS) $(SCRIPTS) \
	    $(AUTOTEST) $(OUTFILES) \
	    $(FOLIOS) $(VIEWS) GNUmakefile.install

LDIRT = check.log check.time pmlogger.log *.out.bad *.full

default: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(BASEDIR)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(FOLIOS) $(VIEWS) $(TESTDIR)
	$(INSTALL) -m 755 $(COMMONS) $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 755 $(AUTOTEST) $(OUTFILES) $(TESTDIR)
	$(INSTALL) -m 755 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
