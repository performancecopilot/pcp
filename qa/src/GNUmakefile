#!gmake
#
# Copyright (c) 2012,2014,2016,2018 Red Hat.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR	= $(PCP_VAR_DIR)/testsuite/src

LLDFLAGS = \
	-L$(TOPDIR)/src/libpcp_gui/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_mmv/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_web/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_trace/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_import/$(LIBPCP_ABIDIR)

NVIDIACFLAGS = -I$(TOPDIR)/src/pmdas/nvidia
NVIDIAQALIB = libnvidia-ml.$(DSOSUFFIX)

LDIRT += localconfig.h libpcp.h

include GNUlocaldefs

localconfig.h: ../localconfig
	@awk -F= '{ print "#define",$$1,$$2 }' < ../localconfig > localconfig.h

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CFILES) $(MYFILES) $(TESTDIR)
	$(INSTALL) -m 755 $(PSCRIPTS) $(MYSCRIPTS) $(TESTDIR)
	$(INSTALL) -m 755 $(NVIDIAQALIB) $(TESTDIR)/$(NVIDIAQALIB)
	$(INSTALL) -m 644 GNUlocaldefs $(TESTDIR)/GNUlocaldefs
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

libpcp.h:	$(TOPDIR)/src/include/pcp/libpcp.h
	rm -f libpcp.h
	$(LN_S) $(TOPDIR)/src/include/pcp/libpcp.h libpcp.h
