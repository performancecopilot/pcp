#!gmake
#
# Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
#

SHELL = sh

ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif
include $(PCP_INC_DIR)/builddefs

# remove -Lpath and -Ipath options from builddefs CFLAGS value
#
PCP_LIBS        =
TMP             := $(CFLAGS:-I%=)
ifdef PCP_DIR
# put -Ipath and -Lpath back but use paths for run-time environment
#
CFLAGS          = $(TMP) -I$(PCP_INC_DIR)/..
LDFLAGS         = -L$(PCP_LIB_DIR)
else
CFLAGS          = $(TMP)
endif

CFILES = bigun.c
LIBTARGET = bigun.$(DSOSUFFIX)
TESTDIR = $(PCP_VAR_DIR)/testsuite
TARGETS = $(LIBTARGET)
MYFILES = domain.h help pmns root
MYSCRIPTS = Install Remove
LSRCFILES = $(MYFILES) $(MYSCRIPTS)
LDIRT = help.pag help.dir

LLDLIBS = -lpcp_pmda -lpcp

default default_pcp setup: $(TARGETS)

$(LIBTARGET):	bigun.o

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)/pmdas/bigun
	$(INSTALL) -m 755 $(MYSCRIPTS) $(TARGETS) $(TESTDIR)/pmdas/bigun
	$(INSTALL) -m 644 GNUmakefile $(CFILES) $(MYFILES) $(TESTDIR)/pmdas/bigun

include $(PCP_INC_DIR)/buildrules
