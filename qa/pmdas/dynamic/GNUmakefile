TOPDIR = ../../..
SHELL = sh

ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif

include $(PCP_DIR)/etc/pcp.conf
include $(PCP_INC_DIR)/builddefs

TESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/src/dynamic

CFILES = dynamic.c
CMDTARGET = pmdadynamic
TARGETS = $(LIBTARGET) $(CMDTARGET)
MYFILES = domain.h help pmns root
MYSCRIPTS = Install Remove
LSRCFILES = $(MYSCRIPTS) $(PMDAFILES) GNUmakefile.install
LDIRT = help.pag help.dir

LLDLIBS = -lpcp_pmda -lpcp

default default_pcp setup: $(TARGETS)

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(CFILES) $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TARGETS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
