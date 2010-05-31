#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#*************************************************************************
#
# Makefile - sample PMDA
#
#*************************************************************************

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

DOMAIN  = SAMPLE

DFILES	= README help
LSRCFILES = root pmns $(DFILES) Install Remove Sample.pmchart
LDIRT = domain.h *.dir *.pag sample.log

SUBDIRS = src
PMDADIR = $(PCP_PMDAS_DIR)/sample

default_pcp default :: domain.h $(SUBDIRS)

default_pcp default :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install :: default
	$(INSTALL) -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns root domain.h $(PMDADIR)
	$(INSTALL) -m 644 Sample.pmchart $(PCP_VAR_DIR)/config/pmchart/Sample

include $(BUILDRULES)

.NOTPARALLEL:
.ORDER:	domain.h $(OBJECTS)

domain.h: ../../pmns/stdpmid
	rm -f domain.h
	@echo "/*" >domain.h
	@echo " * built from $<" >>domain.h
	@echo " */" >>domain.h
	@$(PCP_AWK_PROG) <$< '\
$$1=="#define" && $$2 == "$(DOMAIN)" { print "#define $(DOMAIN) " $$3 >>"$@"; found++ }\
END	{ if (found == 0) { print "Botch: no #define for domain $(DOMAIN) in $<";  exit(1) }\
          if (found > 1) { print "Botch: multiple #defines for domain $(DOMAIN) in $<"; print "... see $@ for details"; exit(1) }\
	  exit(0)\
	}'
