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

IAM	= mailq
DOMAIN	= MAILQ

CMDTARGET = $(IAM)$(EXECSUFFIX)
CFILES	= mailq.c
DFILES	= README
LSRCFILES = Install Remove root help pmns $(DFILES) GNUmakefile.install
LLDLIBS	= -lpcp_pmda -lpcp $(LIB_FOR_REGEX)

PMDADIR = $(PCP_PMDAS_DIR)/$(IAM)
LDIRT	= domain.h *.log *.dir *.pag so_locations

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(PMDADIR)/pmda$(CMDTARGET)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root help pmns domain.h $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile

mailq.o:	domain.h

domain.h:	../../pmns/stdpmid
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(TOPDIR)/src/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) < $< >> $@ '$$1=="#define" && $$2=="$(DOMAIN)" { print "#define $(DOMAIN) " $$3 }'

default_pcp:	default

install_pcp:	install
