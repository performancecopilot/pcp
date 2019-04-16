#!gmake
#
# Copyright (c) 2015-2019 Red Hat.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# NB: this GNUmakefile is only for use from within the PCP build tree.  It governs
# only the packaging of the testsuite into the $PCP_VAR_DIR/testsuite directory.
# The nearby GNUmakefile.install file actually governs execution of the testsuite.

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

TESTDIR = $(PCP_VAR_DIR)/testsuite
TESTS	= $(shell sed -n -e '/^[0-9]/s/:retired//' -e '/^[0-9][0-9]*:reserved/d' -e '/^[0-9]/s/[        ].*//' -e '/^[0-9]/p' <group)

SUBDIRS = src pmdas cisco gluster pconf sadist collectl nfsclient named \
	  archives badarchives views qt linux unbound cifs gpfs lustre ganglia \
	  java mmv postfix perl json slurm tmparch sheet smart admin
ifeq "$(PMDA_PERFEVENT)" "true"
SUBDIRS += perfevent
endif
ifeq "$(HAVE_PYTHON)" "true"
SUBDIRS += secure mic haproxy lio prometheus
endif

# No implicit rules here, thank you
#
.SUFFIXES:

# and do nothing if NNN.out is older than NNN
#
%.out: %

default_pcp default: qa_outfiles localconfig $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

setup: $(SUBDIRS) localconfig qa_hosts
	$(SUBDIRS_MAKERULE)

# override ${MANDIRT} from buildefs, we want to keep e.g. *.[1-9].xz
MANDIRT =

LDIRT += 051.work 134.full.* \
	 *.bak *.bad *.core *.full *.notrun *.raw *.o core a.out core.* \
	 *.log eek* urk* so_locations tmp.* gmon.out oss.qa.tar.gz \
	 *.full.ok *.new rc_cron_check.clean \
	  make.out qa_hosts qa_outfiles stap_*.ko localconfig check.time
	# these ones are links to the real files created when the associated
	# test is run
LDIRT += $(shell [ -f .gitignore ] && grep '\.out$$' .gitignore)

SCRIPTS = mk.localconfig mk.pcpversion mk.qa_hosts mk.variant getpmcdhosts \
	  new check recheck remake findmetric group-stats changeversion \
	  check-gitignore check-group chk.setup show-me disk-work \
	  xlate_2_new_pmns common setup.051 mk.logfarm check.callback.sample

COMMON_SCRIPTS = common.install.cisco common.pcpweb
COMMON = common.check common.config common.filter \
	 common.product common.rc common.setup \
	 common.secure common.gfs2 common.rpm \
	 common.qt common.avahi common.containers \
	 common.discovery common.compress common.webapi \
	 common.python common.prometheus common.bcc

OTHERS = group qa_hosts.master README sanity.coverage \
	 $(shell echo valgrind-suppress* helgrind-suppress*)

DOTOUTFILES = $(shell [ -f qa_outfiles ] && cat qa_outfiles || ls -1 | grep '^[0-9]' | grep -v '^[0-9][0-9][0-9]$$' | grep -v '^[0-9][0-9][0-9][0-9]$$' | egrep -v '^[0-9][0-9][0-9]*\.(full|out\.bad|notrun|work)' | tee qa_outfiles)

qa_outfiles:
	@echo $(DOTOUTFILES) > qa_outfiles

qa_hosts:	qa_hosts.master mk.qa_hosts
	./mk.qa_hosts

localconfig:
	@echo PCP_PLATFORM=$(PCP_PLATFORM) > localconfig
	./mk.pcpversion $(PCP_VERSION) >> localconfig

install_pcp install: $(SUBDIRS)
ifeq (, $(filter mingw darwin freebsd netbsd openbsd solaris, $(TARGET_OS)))
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(TESTS) $(SCRIPTS) $(COMMON_SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 $(COMMON) $(OTHERS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 qa_outfiles $(DOTOUTFILES) $(TESTDIR)
	$(SUBDIRS_MAKERULE)
endif

include $(BUILDRULES)

debug-me:
	@echo TESTS=$(TESTS)
	@echo SRCFILES=$(SRCFILES)
	@echo CONFFILES=$(CONFFILES)
	@echo SUBDIRS=$(SUBDIRS)
