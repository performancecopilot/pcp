#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/lio
MYFILES = iscsi_target_configfs_dump.tar.gz iscsi_conf.json
PYMODULES = iscsi_target.python lio_test.python #lio_test.py iscsi_target.py
LDIRT = lio_test.py iscsi_target.py lio_test.pyc iscsi_target.pyc lio_test.pyo iscsi_target.pyo

default default_pcp setup: lio_test.py iscsi_target.py

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 644 $(PYMODULES) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)

%.py : %.python
	$(LN_S) $< $@
