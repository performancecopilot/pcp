#!gmake
#
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#

ifdef PCP_CONF
include $(PCP_CONF)
else
include $(PCP_DIR)/etc/pcp.conf
endif
PATH	= $(shell . $(PCP_DIR)/etc/pcp.env; echo $$PATH)
include $(PCP_INC_DIR)/builddefs

SUBDIRS = src-oss pmdas
TESTDIR = $(PCP_VAR_DIR)/testsuite

TESTS	= $(shell sed -n -e '/^[0-9]/s/[ 	].*//p' <group)

default_pcp default: localconfig new remake check qa_hosts qa_outfiles $(OTHERS) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

setup: $(SUBDIRS) pconf cisco localconfig qa_hosts
	$(SUBDIRS_MAKERULE)

LDIRT += 051.work 134.full.* \
         *.bak *.bad *.core *.full *.raw *.o core a.out core.* \
	 *.log eek* urk* so_locations tmp.* gmon.out oss.qa.tar.gz \
	 *.full.ok *.new rc_cron_check.clean \
	  make.out qa_hosts qa_outfiles localconfig localconfig.h check.time
	# these ones are links to the real files created when the associated
	# test is run
LDIRT += $(shell [ -f .gitignore ] && grep '\.out$$' .gitignore)
	# from QA 441
LDIRT += big1.*

# 051 depends on this rule being here
051.work/die.001: 051.setup
	chmod u+x 051.setup
	./051.setup

qa_hosts:	qa_hosts.master mk.qa_hosts
	PATH=$(PATH); ./mk.qa_hosts

localconfig:
	PATH=$(PATH); ./mk.localconfig

pconf:	pconf.tar
	tar xf pconf.tar
	touch pconf

cisco:	cisco.tar
	tar xf cisco.tar
	touch cisco

SCRIPTS = mk.localconfig new check recheck remake mk.qa_hosts findmetric \
	  getpmcdhosts group-stats mk.variant changeversion \
	  check-gitignore check-group chk.setup sanity.coverage show-me \
	  xlate_2_new_pmns

COMMON = common common.check common.config common.filter common.install.cisco \
	 common.pcpweb common.product common.rc common.setup

OTHERS = group qa_hosts.master README \
	 pconf.tar cisco.tar valgrind-suppress

DOTOUTFILES = $(shell [ -f qa_outfiles ] && cat qa_outfiles || git ls-tree --name-only HEAD | grep '^[0-9]' | grep -v '^[0-9][0-9][0-9]$$' | grep -v '^[0-9][0-9][0-9][0-9]$$' | tee qa_outfiles)
LSRCFILES = $(SCRIPTS) $(COMMON) $(TESTS) $(DOTOUTFILES) $(OTHERS) qa_outfiles

qa_outfiles:
	@echo $(DOTOUTFILES) > qa_outfiles

install_pcp install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(TESTS) $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile $(COMMON) $(OTHERS) $(TESTDIR)
	$(INSTALL) -m 644 qa_outfiles $(DOTOUTFILES) $(TESTDIR)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
