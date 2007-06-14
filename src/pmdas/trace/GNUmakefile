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
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#
# $Id: GNUmakefile,v 1.36 2006/01/31 07:52:32 makc Exp $
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= trace
DOMAIN	= TRACE

PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
DEMODIR	= $(PCP_DEMOS_DIR)/$(IAM)

SCRIPTS	= Install Remove
OTHERS	= pmns root 
DFILES	= help README 
DEMOS	= app1.c app2.c app3.c fapp1.f japp1.java
DEMOFILES = README.demos GNUmakefile.demos GNUmakefile.stub stub.c
APPS	= app1$(EXECSUFFIX) app2$(EXECSUFFIX) app3$(EXECSUFFIX)

LCFLAGS = -I.
LLDFLAGS= -L$(TOPDIR)/src/libpcp/src -L$(TOPDIR)/src/libpcp_trace/src
LLDLIBS = -lpcp -lpcp_trace

LDIRT	= *.log *.dir *.pag *.o $(APPS) pcp \
	  pmtrace.c tmpcp

LSRCFILES= $(SCRIPTS) $(OTHERS) $(DEMOS) $(DEMOFILES) $(DFILES) \
	GNUmakefile.install

SUBDIRS	= src

default:	$(SUBDIRS) demos
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install:	$(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(OTHERS) $(DFILES) $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 GNUmakefile.demos $(DEMODIR)/Makefile
	$(INSTALL) -m 644 README.demos $(DEMODIR)/README
	$(INSTALL) -m 644 GNUmakefile.stub $(DEMODIR)/Makefile.stub
	$(INSTALL) -m 644 stub.c pmtrace.c $(DEMOS) $(DEMODIR)
	$(INSTALL) -m 755 $(APPS) $(DEMODIR)

demos:	$(APPS) pmtrace.c

pmtrace.c:	$(TOPDIR)/src/pmtrace/pmtrace.c
	rm -f $@ && cp $< $@

app1.o app2.o app3.o:	tmpcp

app1$(EXECSUFFIX):	app1.o
	$(CCF) -o $@ $(LDFLAGS) app1.o $(LDLIBS)

app2$(EXECSUFFIX):	app2.o
	$(CCF) -o $@ $(LDFLAGS) app2.o $(LDLIBS)

app3$(EXECSUFFIX):	app3.o
	$(CCF) -o $@ $(LDFLAGS) app3.o $(LDLIBS) $(PTHREAD_LIB)

tmpcp:
	$(LN_S) -s $(TOPDIR)/src/include pcp
	touch tmpcp

default_pcp:	default

install_pcp:	install
