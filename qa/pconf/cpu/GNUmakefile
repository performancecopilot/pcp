#!gmake

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/pconf/cpu
CONFIGS = aggregate_util excess_fpe load_average \
	  single_util system_calls system_mode
LSRCFILES = $(CONFIGS)

default default_pcp setup: 

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 $(CONFIGS) $(TESTDIR)

include $(BUILDRULES)
