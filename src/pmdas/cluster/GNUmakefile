#
# Copyright (c) 2008-2009 Silicon Graphics, Inc.  All Rights Reserved.
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

DFILES	= config
CFILES	= pmda.c server.c util.c
# These C files are automatically generated.
AUTO_CFILES = metric_table.c subdomains.c
CMDTARGET = pmdacluster$(EXECSUFFIX)
DAEMON = pmclusterd
LLDLIBS = -lpcp -lpcp_pmda
LCFLAGS = -I.
LSRCFILES = Install Remove $(DFILES) root GNUmakefile.install \
	    rc_pmclusterd clusterns.pl
IAM	= cluster
DOMAIN	= CLUSTER
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
$(CMDTARGET)_OBJS = $(CFILES:.c=.o) $(AUTO_CFILES:.c=.o)
$(DAEMON)_OBJS = pmclusterd.o util.o $(AUTO_CFILES:.c=.o)

# sub-PMDA namespace root nodes and help files

SUBPMNS = ../linux/root_linux ../ib/root
SUBHELP = ../linux/help ../ib/help

LDIRT	= help pmns metric_table.c domain.h pcp *.o $(IAM).log \
	  pmda$(IAM) pmda_$(IAM).so $(DAEMON) pmns.tmp* subdomains.c

default: build-me

include $(BUILDRULES)

ifeq ($(HAVE_IBDEV),1)
build-me:	help pmns pcp domain.h subdomains.c \
			$(CMDTARGET) $(DAEMON)
install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 -d $(PMDADIR)/nodes
	$(INSTALL) -m 755 Install Remove $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 GNUmakefile.install $(PMDADIR)/Makefile
	$(INSTALL) -m 644 $(DFILES) pmns root help domain.h $(PMDADIR)
	$(INSTALL) -m 755 pmclusterd $(PCP_BINADM_DIR)/pmclusterd
	$(INSTALL) -m 755 rc_pmclusterd $(PCP_SHARE_DIR)/lib/rc.pmclusterd
else
build-me:
install:
endif

PMNSDIR    = $(TOPDIR)/src/pmns
STDPMID	   = $(PMNSDIR)/stdpmid
RUN_STDPMID = $(RUN_IN_BUILD_ENV) CPATH=$(PMNSDIR)
PMINFO     = $(TOPDIR)/src/pminfo/pminfo

# Build a namespace containing all of the sub-PMDAS.  At the same time
# generate a list of metric names and domain numbers and generate a C
# source file containing the corresponding subdomain mapping.

help pmns metric_table.c subdomains.c : \
		config cluster.h $(SUBPMNS) $(SUBHELP) $(STDPMID) clusterns.pl pcp
	rm -f pmns.tmp*
	echo "root {" >pmns.tmp
	echo "}" >>pmns.tmp
	thisDir=`pwd` && \
	for ns in $(SUBPMNS) ; do \
	    pmdaName=`dirname $$ns` ; \
	    pmdaName=`basename $$pmdaName` ; \
	    echo "including PMNS from $$ns" ; \
	    cd `dirname $$ns` && \
	    pmdaroot=`basename $$ns` && \
	    echo $(RUN_STDPMID) $(PMNSDIR)/pmnsmerge \
		$$thisDir/pmns.tmp $$pmdaroot $$thisDir/pmns.tmp.out && \
	    $(RUN_STDPMID) $(PMNSDIR)/pmnsmerge \
		$$thisDir/pmns.tmp $$pmdaroot $$thisDir/pmns.tmp.out && \
	    echo $(RUN_STDPMID) $(PMINFO) -m -n $$ns && \
	    $(RUN_STDPMID) $(PMINFO) -m -n $$ns | \
		awk -v pmda=$$pmdaName -v ns=$$ns \
		    >>$$thisDir/pmns.tmp.map ' \
		    NF==3 && $$2=="PMID:" && \
		    $$3 ~ /^[0-9]+\.[0-9]+\.[0-9]+$$/ \
		    { \
			n++; \
			d = substr($$3, 1, index($$3, ".") - 1); \
			print d, pmda; \
			exit(0); \
		    } ; \
		    END { \
			if (n==0) { \
			    print "no metrics in ", ns, "\n" >"/dev/stderr"; \
			    exit(1) \
			} \
		    }' && \
	    mv $$thisDir/pmns.tmp.out $$thisDir/pmns.tmp ; \
	    cd $$thisDir ; \
	    rm -f pmns.tmp.out* ; \
	done
	./clusterns.pl pmns.tmp pmns.tmp.map $(SUBHELP)
	rm -f pmns.tmp*

domain.h: $(STDPMID)
	rm -f domain.h
	echo "/*" >domain.h
	echo " * built from $(PCP_VAR_DIR)/pmns/stdpmid" >>domain.h
	echo " */" >>domain.h
	$(PCP_AWK_PROG) <$(STDPMID) >>domain.h \
	    '/#define[ 	][ 	]*$(DOMAIN)[ 	]/ { print "#define $(DOMAIN) " $$3 }'

default_pcp: default

install_pcp: install

$(CMDTARGET) : OBJECTS=$($(CMDTARGET)_OBJS)
$(CMDTARGET) : $($(CMDTARGET)_OBJS)

$(DAEMON): OBJECTS=$($(DAEMON)_OBJS)
$(DAEMON): $($(DAEMON)_OBJS)
	$(CCF) -o $(DAEMON) $(LDFLAGS) $(OBJECTS) $(LDLIBS)

pcp:
	$(LN_S) -f $(TOPDIR)/src/include pcp
