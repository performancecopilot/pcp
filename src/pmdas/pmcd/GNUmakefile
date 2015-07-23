#!gmake
#
# Copyright (c) 2000-2001,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#
# pmcd PMDA
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

DFILES		= help
LTARGETS	= help.dir
LDIRT		= *.log *.dir *.pag domain.h so_locations

TARGETS = help.dir
LSRCFILES = help root_pmcd

SUBDIRS = src
PMDADIR = $(PCP_PMDAS_DIR)/pmcd
CONF_LINE = "pmcd	2	dso	pmcd_init	$(PCP_PMDAS_DIR)/pmcd/pmda_pmcd.$(DSOSUFFIX)"

default_pcp default :: $(TARGETS) 

default_pcp default :: $(SUBDIRS) 
	$(SUBDIRS_MAKERULE)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 help.dir help.pag $(PMDADIR) 
	$(INSTALL) -m 644 root_pmcd $(PCP_VAR_DIR)/pmns/root_pmcd

help.dir: help root_pmcd 
	$(NEWHELP) -v 2 -n root_pmcd -o help < help

include $(BUILDRULES)
