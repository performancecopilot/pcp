#!gmake
#
# Original implementation by Troy Dawson (dawson@fnal.gov)
# 
# Copyright (c) 2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= lmsensors
DOMAIN	= LMSENSORS
TARGETS	= $(IAM)
CFILES	= lmsensors.c
HFILES	= lmsensors.h
SCRIPTS	= Install Remove
DFILES	= README
LSRCFILES = $(SCRIPTS) pmns help root $(DFILES)

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= domain.h $(IAM).log pmda$(IAM) pmda_$(IAM).so $(TARGETS) \
	help.pag help.dir
LLDLIBS	= -lpcp_pmda -lpcp

default: build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(TARGETS)

install : default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) pmns help root domain.h $(PMDADIR)
else
build-me:
install:
endif

$(IAM):	$(OBJECTS)

lmsensors.o: domain.h

default_pcp: default

install_pcp: install

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
