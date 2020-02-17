TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/statsd/src/cases

ifeq "$(PMDA_STATSD)" "true"
default setup default_pcp:

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 01.py $(TESTDIR)/01.py
	$(INSTALL) -m 644 02.py $(TESTDIR)/02.py
	$(INSTALL) -m 644 03.py $(TESTDIR)/03.py
	$(INSTALL) -m 644 04.py $(TESTDIR)/04.py
	$(INSTALL) -m 644 05.py $(TESTDIR)/05.py
	$(INSTALL) -m 644 06.py $(TESTDIR)/06.py
	$(INSTALL) -m 644 07.py $(TESTDIR)/07.py
	$(INSTALL) -m 644 08.py $(TESTDIR)/08.py
	$(INSTALL) -m 644 09.py $(TESTDIR)/09.py
	$(INSTALL) -m 644 10.py $(TESTDIR)/10.py
	$(INSTALL) -m 644 11.py $(TESTDIR)/11.py
	$(INSTALL) -m 644 12.py $(TESTDIR)/12.py
	$(INSTALL) -m 644 13.py $(TESTDIR)/13.py
	$(INSTALL) -m 644 14.py $(TESTDIR)/14.py
	$(INSTALL) -m 644 15.py $(TESTDIR)/15.py
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
else
default setup default_pcp:
install install_pcp:
endif

include $(BUILDRULES)
