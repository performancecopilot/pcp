#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/systemd
SYSTEMD	= $(PCP_ETC_DIR)/systemd/system
CONFIGS = $(shell echo *.conf)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CONFIGS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
ifeq ($(ENABLE_SYSTEMD),true)
	for s in `echo *.conf | sed -e 's/.conf//g'`; do \
	    $(INSTALL) -m 755 -d $(SYSTEMD)/$$s.service.d; \
	    $(INSTALL) -m 644 $$s.conf $(SYSTEMD)/$$s.service.d/$$s.conf; \
	done
endif

include $(BUILDRULES)
