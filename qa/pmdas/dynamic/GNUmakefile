#
# Copyright (c) 2012-2015 Red Hat.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas/dynamic

CFILES = dynamic.c
CMDTARGET = pmdadynamic
TARGETS = $(LIBTARGET) $(CMDTARGET)
MYFILES = domain.h help pmns root
MYSCRIPTS = Install Remove
LSRCFILES = $(MYSCRIPTS) $(MYFILES) GNUmakefile.install
LDIRT = help.pag help.dir

LLDFLAGS = $(PCP_LIBS)
LLDLIBS = $(PCP_PMDALIB)

default default_pcp setup: $(TARGETS)

$(OBJECTS): domain.h

install install_pcp: default
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TARGETS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
