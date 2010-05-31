#!gmake
#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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


DFILES	= README
CFILES	= txmon.c txrecord.c
HFILES	= txmon.h
TARGETS	= txmon$(EXECSUFFIX) txrecord$(EXECSUFFIX)

LCFLAGS	= -I.
LLDLIBS	= -lpcp_pmda -lpcp

SCRIPTS	= Install Remove genload
OTHERS	= pmns help root
LSRCFILES= $(SCRIPTS) $(OTHERS) GNUmakefile.install $(DFILES)

IAM	= txmon
DOMAIN	= TXMON
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

LDIRT	= pcp *.o *.log *.dir *.pag domain.h $(TARGETS)

default:	build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	pcp $(TARGETS)

install:	build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
	$(INSTALL) -m 755 txrecord $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root $(HFILES) $(PMDADIR)
	$(INSTALL) -m 644 help pmns domain.h $(CFILES) $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
else
build-me:
install:
endif

domain.h:	../../pmns/stdpmid
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

$(OBJECTS):	domain.h

txmon$(EXECSUFFIX):	txmon.o
	$(CCF) -o $@ $(LDFLAGS) txmon.o $(LDLIBS)

txrecord$(EXECSUFFIX):	txrecord.o
	$(CCF) -o $@ $(LDFLAGS) txrecord.o $(LDLIBS)

pcp:
	ln -s $(TOPDIR)/src/include pcp

default_pcp:	default

install_pcp:	install
