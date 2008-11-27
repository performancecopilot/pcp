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

IAM	= weblog
TARGETS	= $(IAM)$(EXECSUFFIX) check_match$(EXECSUFFIX)
CFILES	= weblog.c pmda.c sproc.c
HFILES	= weblog.h
SCRIPTS	= Install Remove server.sh weblogconv.sh
CHARTS	= Web.Alarms.pmchart Web.Requests.pmchart Web.Volume.pmchart \
	  Web.Allservers.pmchart Web.Perserver.Bytes.pmchart \
	  Web.Perserver.Requests.pmchart
DFILES	= README
LSRCFILES = pmns help $(DFILES) root $(SCRIPTS) check_match.c $(CHARTS) \
	GNUmakefile.install

LDIRT	= domain.h $(TARGETS) check_match.o

PMDADIR	= $(PCP_PMDAS_DIR)/weblog
PMCHARTDIR = $(PCP_VAR_DIR)/config/pmchart
CONFDIR	= $(PCP_VAR_DIR)/config/web

LDLIBS	= -lpcp_pmda -lpcp $(PTHREAD_LIB)

default:	build-me

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS)

install:	build-me
	# $(INSTALL) -d $(CONFDIR)
	# $(INSTALL) -m 644 weblog.conf $(CONFDIR)/weblog.conf
	$(INSTALL) -d $(PMDADIR)
	$(INSTALL) -m 755 $(IAM) $(PMDADIR)/pmda$(IAM)
	$(INSTALL) -m 755 check_match $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(DFILES) root help pmns domain.h $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 Web.Alarms.pmchart $(PMCHARTDIR)/Web.Alarms
	$(INSTALL) -m 644 Web.Requests.pmchart $(PMCHARTDIR)/Web.Requests
	$(INSTALL) -m 644 Web.Volume.pmchart $(PMCHARTDIR)/Web.Volume
	$(INSTALL) -m 755 Web.Allservers.pmchart $(PMCHARTDIR)/Web.Allservers
	$(INSTALL) -m 755 Web.Perserver.Bytes.pmchart $(PMCHARTDIR)/Web.Perserver.Bytes
	$(INSTALL) -m 755 Web.Perserver.Requests.pmchart $(PMCHARTDIR)/Web.Perserver.Requests
else
build-me:
install:
endif

weblog$(EXECSUFFIX):	$(OBJECTS)

weblog.o:	domain.h

check_match$(EXECSUFFIX):	check_match.o
	$(CCF) -o $@ $(LDFLAGS) check_match.o $(LDLIBS)

DOMAIN	= WEBSERVER
domain.h:	../../pmns/stdpmid
	echo "/*" >domain.h
	echo " * built from stdpmid by Makefile on `date`" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) < $< >> $@ '$$1=="#define" && $$2=="$(DOMAIN)" { print "#define $(DOMAIN) " $$3 }'

default_pcp:	default

install_pcp:	install
