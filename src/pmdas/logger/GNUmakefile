#!gmake
#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2011 Red Hat Inc.
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

CMDTARGET = pmdalogger$(EXECSUFFIX)
LIBTARGET = pmda_logger.$(DSOSUFFIX)
TARGETS = $(CMDTARGET) $(LIBTARGET)

DFILES 	= README
CFILES	= logger.c percontext.c
HFILES = percontext.h
LLDLIBS = -lpcp -lpcp_pmda
LCFLAGS = -I.
LSRCFILES = Install Remove pmns help $(DFILES) root GNUmakefile.install

IDBTAG	= PCP_DEMO

IAM	= logger
DOMAIN	= LOGGER
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h *.o $(IAM).log pmda$(IAM) pmda_$(IAM).so

default_pcp default:	domain.h $(TARGETS)

logger.o percontext.o: percontext.h

include $(BUILDRULES)

install install_pcp:	default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root help pmns domain.h $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(LIBTARGET) $(PMDADIR)

.NOTPARALLEL:
.ORDER:	domain.h $(OBJECTS)

# If we're in the pcp source tree, generate domain.h from
# pmns/stdpmid.  Otherwise, use the system one.  The best place to put
# a value for this pmda is in stdpmid.local (which is either in
# ../../pmns or in /var/lib/pcp/pmns).
# 
# If you receive an error like:
#   Botch: no #define for domain LOGGER in /var/lib/pcp/pmns/stdpmid
# add the following line to stdpmid.local:
#   LOGGER 255
# Then run /var/lib/pcp/pmns/Make.stdpmid
ifeq ($(wildcard ../../pmns/stdpmid), ../../pmns/stdpmid)
STDPMID = ../../pmns/stdpmid
else
STDPMID = /var/lib/pcp/pmns/stdpmid
endif

domain.h: $(STDPMID)
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
