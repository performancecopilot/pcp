#
# Copyright (c) 2018 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

IAM       = bcc
MODULEDIR = $(PCP_PMDAS_DIR)/$(IAM)/modules
PYSCRIPTS = $(shell echo *.py | sed -e 's/\.py//g')
EBPFPROGS = $(shell echo *.bpf)

include $(BUILDRULES)

default_pcp default:	$(SCRIPTS) $(BCC_EBPF)

install_pcp install:	default
	$(INSTALL) -m 755 -d $(MODULEDIR)
	for py in $(PYSCRIPTS); do \
	    $(INSTALL) -m 644 $${py}.py $(MODULEDIR)/$${py}; \
	done
	$(INSTALL) -m 644 __init.py__ $(MODULEDIR)/__init.py__
	$(INSTALL) -m 644 $(EBPFPROGS) $(MODULEDIR)
