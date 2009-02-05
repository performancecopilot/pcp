TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

PMAFM  = $(PCP_VAR_DIR)/config/pmafm
LSRCFILES = pmchart

default:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMAFM)
	$(INSTALL) -m 0444 pmchart $(PMAFM)/pmchart
