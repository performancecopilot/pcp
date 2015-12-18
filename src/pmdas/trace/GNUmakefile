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
DEMOFILES = README.demos Makefile.proto GNUmakefile.stub stub.c
APPS	= app1$(EXECSUFFIX) app2$(EXECSUFFIX) app3$(EXECSUFFIX)

LCFLAGS = -I.
LLDFLAGS= -L$(TOPDIR)/src/libpcp/src -L$(TOPDIR)/src/libpcp_trace/src
LLDLIBS = $(PCP_TRACELIB)

LDIRT	= *.log *.dir *.pag *.o $(APPS) tmp.c \
	  pmtrace.c Makefile.demos Makefile.stub

LSRCFILES= $(SCRIPTS) $(OTHERS) $(DEMOS) $(DEMOFILES) $(DFILES)

SUBDIRS	= src

default:	$(SUBDIRS) Makefile.demos Makefile.stub build-me
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

ifneq "$(TARGET_OS)" "mingw"
build-me:	demos

install:	$(SUBDIRS) Makefile.demos Makefile.stub
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 $(OTHERS) $(DFILES) $(PMDADIR)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 Makefile.demos $(DEMODIR)/Makefile
	$(INSTALL) -m 644 Makefile.stub $(DEMODIR)/Makefile.stub
	$(INSTALL) -m 644 README.demos $(DEMODIR)/README
	$(INSTALL) -m 644 stub.c pmtrace.c $(DEMOS) $(DEMODIR)

else
build-me:
install:	$(SUBDIRS)
endif

demos:	$(APPS) pmtrace.c

pmtrace.c:	$(TOPDIR)/src/pmtrace/pmtrace.c
	rm -f $@ && cp $< $@

app1.o:	app1.c $(TOPDIR)/src/include/pcp/pmapi.h
	@rm -f tmp.c
	sed -e 's;<pcp/\(.*\)>;"\1";' app1.c >tmp.c
	$(CCF) -c -o $@ tmp.c
	@rm -f tmp.c

app2.o:	app2.c $(TOPDIR)/src/include/pcp/pmapi.h
	@rm -f tmp.c
	sed -e 's;<pcp/\(.*\)>;"\1";' app2.c >tmp.c
	$(CCF) -c -o $@ tmp.c
	@rm -f tmp.c

app3.o:	app3.c $(TOPDIR)/src/include/pcp/pmapi.h
	@rm -f tmp.c
	sed -e 's;<pcp/\(.*\)>;"\1";' app3.c >tmp.c
	$(CCF) -c -o $@ tmp.c
	@rm -f tmp.c

app1$(EXECSUFFIX):	app1.o
	$(CCF) -o $@ $(LDFLAGS) app1.o $(LDLIBS)

app2$(EXECSUFFIX):	app2.o
	$(CCF) -o $@ $(LDFLAGS) app2.o $(LDLIBS)

app3$(EXECSUFFIX):	app3.o
	$(CCF) -o $@ $(LDFLAGS) app3.o $(LDLIBS) $(LIB_FOR_PTHREADS)

default_pcp:	default

install_pcp:	install

.NOTPARALLEL:
Makefile.demos:	Makefile.proto
	rm -f $@
	sed \
		-e 's@PCP_INC_DIR@$(PCP_INC_DIR)@' \
		-e 's/PTHREAD_LIB/$(LIB_FOR_PTHREADS)/' \
		-e 's/DLOPEN_LIB/$(LIB_FOR_DLOPEN)/' \
		-e 's/MATH_LIB/$(LIB_FOR_MATH)/' \
		<$^ >$@

Makefile.stub:	GNUmakefile.stub
	rm -f $@
	sed \
		-e 's@\$$(PCP_ETC_DIR)@$(PCP_ETC_DIR)@' \
		<$^ >$@
