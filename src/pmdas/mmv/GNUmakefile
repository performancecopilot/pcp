#
# Copyright (c) 2000-2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM		= mmv
DOMAIN		= MMV
CMDTARGET	= pmda$(IAM)$(EXECSUFFIX)
LIBTARGET	= pmda_$(IAM).$(DSOSUFFIX)
TARGETS		= $(CMDTARGET) $(LIBTARGET) mmvdump$(EXECSUFFIX)
LDIRT		= domain.h *.log *.dir *.pag so_locations mmvdump mmvdump.o pcp

CFILES		= mmv.c
LSRCFILES	= Install Remove mmvdump.c root_mmv

LCFLAGS		= -I.
LLDFLAGS	= -L$(TOPDIR)/src/libpcp_mmv/src
LLDLIBS		= -lpcp_pmda -lpcp

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE = "mmv	70	dso	mmv_init	$(PCP_PMDAS_DIR)/mmv/pmda_mmv.$(DSOSUFFIX)"

default_pcp default: $(TARGETS)
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

include $(BUILDRULES)

install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h $(PMDADIR)/domain.h
	$(INSTALL) -m 755 $(TARGETS) Install Remove $(PMDADIR)
	$(INSTALL) -m 644 root_mmv $(PCP_VAR_DIR)/pmns/root_mmv

$(CMDTARGET):	$(OBJECTS)

$(IAM).o : domain.h pcp

domain.h:	../../pmns/stdpmid
	rm -f $@
	echo "/*" >$@
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>$@
	echo " */" >>$@
	$(PCP_AWK_PROG) <$< >>$@ '$$1=="#define" && $$2=="$(DOMAIN)" { print "#define $(DOMAIN) " $$3 }'

pcp :
	$(LN_S) -f $(TOPDIR)/src/include pcp

mmvdump$(EXECSUFFIX):    mmvdump.o
	$(CCF) -o $@ mmvdump.o $(LDFLAGS) -lpcp -lpcp_mmv
