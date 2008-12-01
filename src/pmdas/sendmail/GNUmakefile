#!gmake
#
# Copyright (c) 2000-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= sendmail
DOMAIN	= SENDMAIL
CMDTARGET = $(IAM)$(EXECSUFFIX)
LIBTARGET = pmda_$(IAM).$(DSOSUFFIX)
CFILES	= sendmail.c
SCRIPTS	= Install Remove
DFILES	= README
LSRCFILES= $(SCRIPTS) pmns help root Sendmail.pmchart $(DFILES) \
	GNUmakefile.install

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMCHART	= $(PCP_VAR_DIR)/config/pmchart

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) $(LIBTARGET) $(CMDTARGET)
LLDLIBS	= -lpcp_pmda -lpcp

default_pcp default:	$(CMDTARGET) $(LIBTARGET)

include $(BUILDRULES)

install install_pcp:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(PMDADIR)/$(LIBTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/pmda$(IAM)$(EXECSUFFIX)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns help root domain.h $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 Sendmail.pmchart $(PMCHART)/Sendmail

$(IAM)$(EXECSUFFIX):	$(OBJECTS)

sendmail.o:	domain.h

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) < $< >> $@ '$$1=="#define" && $$2=="$(DOMAIN)" { print "#define $(DOMAIN) " $$3 }'
