#
# Copyright (c) 2015-2022 Red Hat.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# NB: this GNUmakefile is only for use from within the PCP build tree.
# It manages packaging of the testsuite into the $PCP_VAR_DIR/testsuite
# directory; GNUmakefile.install file manages execution of the testsuite.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

IAM = pcp-testsuite
TESTDIR = $(PCP_VAR_DIR)/testsuite
TESTS	= $(shell sed -n -e '/^[0-9][0-9]*:retired/d' -e '/^[0-9][0-9]*:reserved/d' -e '/^[0-9]/s/[        ].*//' -e '/^[0-9]/p' <group)

SUBDIRS = src pmdas cisco gluster pconf sadist collectl nfsclient named \
	  archives badarchives views qt linux unbound cifs gpfs lustre ganglia \
	  java mmv postfix perl json slurm tmparch sheet smart admin hacluster \
	  sockets denki gfs2 farm

ifeq "$(PMDA_PERFEVENT)" "true"
SUBDIRS += perfevent
endif
ifeq "$(PMDA_STATSD)" "true"
SUBDIRS += statsd
endif
ifeq "$(PMDA_BPFTRACE)" "true"
SUBDIRS += bpftrace
endif
ifeq "$(HAVE_PYTHON)" "true"
SUBDIRS += secure mic haproxy lio openmetrics
endif

ifeq "$(ENABLE_SELINUX)" "true"
PACKAGES_SELINUX_DIR = $(PCP_SELINUX_DIR)/packages
TARGETED_SELINUX_DIR = $(PCP_SELINUX_DIR)/packages/targeted
SELINUX_FILES = $(IAM).pp.bz2
else
SELINUX_FILES =
endif

# No implicit rules here, thank you
#
.SUFFIXES:

default_pcp default: qa_outfiles localconfig $(SELINUX_FILES) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

setup: $(SUBDIRS) localconfig qa_hosts
	$(SUBDIRS_MAKERULE)

# override ${MANDIRT} from buildefs, we want to keep e.g. *.[1-9].xz
MANDIRT =

LDIRT += 051.work 134.full.* \
	 $(IAM).cil $(IAM).mod $(IAM).pp $(IAM).pp.bz2 \
	 *.bak *.bad *.core *.full *.notrun *.raw *.o core a.out core.* \
	 *.log *.log.prev eek* urk* so_locations tmp.* gmon.out oss.qa.tar.gz \
	 *.full.ok *.new rc_cron_check.clean \
	 make.out qa_hosts qa_outfiles stap_*.ko localconfig check.time

# these ones are links to the real files created when the associated
# test is run
LDIRT += $(shell [ -f .gitignore ] && grep '\.out$$' .gitignore)

# temporary files from selinux policy building
LDIRDIRT += tmp

SCRIPTS = mk.localconfig mk.pcpversion mk.qa_hosts mk.variant getpmcdhosts \
	  new check recheck remake findmetric group-stats \
	  check-gitignore check-group chk.setup show-me disk-work 008.helper \
	  xlate_2_new_pmns common setup.051 mk.logfarm check.callback.sample \
	  daily-cleanup

COMMON_SCRIPTS = common.install.cisco common.pcpweb
COMMON = common.check common.config common.filter \
	 common.product common.rc common.setup \
	 common.secure \
	 common.qt common.containers \
	 common.discovery common.compress \
	 common.python common.openmetrics \
	 common.bcc common.bpf common.bpftrace common.redis

OTHERS = group triaged qa_hosts.primary README sanity.coverage \
	 $(shell echo valgrind-suppress* helgrind-suppress*)

DOTOUTFILES = $(shell [ -f qa_outfiles ] && cat qa_outfiles || ls -1 | grep '^[0-9]' | grep -v '^[0-9][0-9][0-9]$$' | grep -v '^[0-9][0-9][0-9][0-9]$$' | grep -E -v '^[0-9][0-9][0-9]*\.(full|out\.bad|notrun|helper|work)' | tee qa_outfiles)

qa_outfiles:
	@echo $(DOTOUTFILES) > qa_outfiles

qa_hosts:	qa_hosts.primary mk.qa_hosts
	./mk.qa_hosts

localconfig:
	@echo PCP_PLATFORM=$(PCP_PLATFORM) > localconfig
	./mk.pcpversion $(PCP_VERSION) >> localconfig

install_pcp install: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 -f $(TESTS) $(SCRIPTS) $(COMMON_SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 -f $(COMMON) $(OTHERS) $(TESTDIR)
	$(INSTALL) -m 644 -f GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 -f qa_outfiles $(DOTOUTFILES) $(TESTDIR)
ifeq "$(ENABLE_SELINUX)" "true"
	$(INSTALL) -m 644 $(IAM).pp.bz2 $(TARGETED_SELINUX_DIR)/$(IAM).pp.bz2
endif
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

ifeq "$(ENABLE_SELINUX)" "true"
$(IAM).pp: $(IAM).te $(IAM).fc $(IAM).if
	@test -d tmp || mkdir tmp # workaround an el7 selinux makefile issue
	# lots of duplicate definition from system container.if - filter out
	$(MAKE) -f /usr/share/selinux/devel/Makefile $(IAM).pp 2>&1 | grep -v container.if

$(IAM).pp.bz2: $(IAM).pp
	$(BZIP2) -9 < $^ > $@
endif

# and do nothing if NNN.out is older than NNN
#
%.out: %

debug-me:
	@echo TESTS=$(TESTS)
	@echo SRCFILES=$(SRCFILES)
	@echo CONFFILES=$(CONFFILES)
	@echo SUBDIRS=$(SUBDIRS)
	@echo SELINUX_PCPQA=$(SELINUX_PCPQA)
