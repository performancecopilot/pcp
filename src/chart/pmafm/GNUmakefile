TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
include $(PCP_DIR)/etc/pcp.conf
PMAFM  = $(PCP_VAR_DIR)/config/pmafm
CONFIG = kmchart
LSRCFILES = $(CONFIG)

default:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMAFM)
	$(INSTALL) -m 0444 $(CONFIG) $(PMAFM)
