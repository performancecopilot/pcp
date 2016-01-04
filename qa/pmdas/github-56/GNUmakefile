#
# Copyright (c) 2015 Emanuele Altieri.
# Copyright (c) 2012-2015 Red Hat.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas/github-56

IAM	  = trivial
DOMAIN    = TRIVIAL

CFILES	  = trivial.c
CMDTARGET = pmda$(IAM)$(EXECSUFFIX)
LLDLIBS   = $(PCP_PMDALIB)
LSRCFILES = GNUmakefile.install
LDIRT	  = domain.h *.o $(IAM).log $(TARGETS)

LLDFLAGS  = $(PCP_LIBS)

default default_pcp setup: $(CMDTARGET)

$(OBJECTS): domain.h

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) domain.h $(TESTDIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(TESTDIR)/$(CMDTARGET)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

install install_pcp: default

include $(BUILDRULES)

domain.h: ../../../src/pmns/stdpmid
	$(DOMAIN_MAKERULE)
