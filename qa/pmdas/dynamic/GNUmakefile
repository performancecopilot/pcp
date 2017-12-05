#
# Copyright (c) 2012-2015 Red Hat.
#

TOPDIR = ../../..

ifeq "$(shell [ -f $(TOPDIR)/src/include/builddefs ] && echo 1)" "1"
include $(TOPDIR)/src/include/builddefs
else
# we are running QA from a pristine git tree, so none of
# $(TOPDIR)/src/include has been configured ... force the make for
# this PMDA to use the installed /usr/include/pcp files
#
ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif
include $(PCP_INC_DIR)/builddefs

# strip -I and -L options
#
TMP             := $(CFLAGS:-I%=)
CFLAGS          = $(TMP)
PCP_LIBS	=

ifneq "$(PCP_INC_DIR)" "/usr/include/pcp"
# for cc add -I<run-time-include-dir> (need /.. at the end so
# #include <pcp/foo.h> works) when $(PCP_INC_DIR) may not be on
# the default cpp include search path.
CFLAGS		+= -I$(PCP_INC_DIR)/..
endif
ifneq "$(PCP_LIB_DIR)" "/usr/lib"
# for ld add -L<run-time-lib-dir> and include -rpath when
# $(PCP_LIB_DIR) may not be on the default ld search path.
#
ifeq "$(PCP_PLATFORM)" "darwin"
PCP_LIBS	+= -L$(PCP_LIB_DIR) -Wl,-rpath $(PCP_LIB_DIR)
else
PCP_LIBS	+= -L$(PCP_LIB_DIR) -Wl,-rpath=$(PCP_LIB_DIR)
endif
endif
endif

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

dynamic.o:	$(TOPDIR)/src/include/pcp/libpcp.h
