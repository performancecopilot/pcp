#
# Copyright (c) 2007 Nathan Scott.  All Rights Reserved.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
include /etc/pcp.conf
PMAFM  = $(PCP_VAR_DIR)/config/pmafm
CONFIG = kmchart

default:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMAFM)
	$(INSTALL) -m 0444 $(CONFIG) $(PMAFM)
