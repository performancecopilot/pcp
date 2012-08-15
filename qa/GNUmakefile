TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = src archives views interact
TESTDIR = $(PCP_VAR_DIR)/../pcp-gui/testsuite

COMMONS = common.cmd common.filter common.sh common.test
SCRIPTS = check new remake
AUTOTEST = $(shell echo [0-9][0-9][0-9])
OUTFILES = $(shell echo *.out)
LSRCFILES = $(COMMONS) $(SCRIPTS) $(AUTOTEST) $(OUTFILES)

LDIRT = check.log check.time pmlogger.log *.out.bad *.full

default: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(COMMONS) $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 755 $(AUTOTEST) $(OUTFILES) $(TESTDIR)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
