#
# Copyright (c) 2015-2020 Red Hat.
# Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
#
# NB: this GNUmakefile is only for use from within the PCP build tree.  It governs
# only the packaging of the testsuite into the $PCP_VAR_DIR/testsuite directory.
# The nearby GNUmakefile.install file actually governs execution of the testsuite.

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs
ifeq "$(ENABLE_SELINUX)" "true"
-include ./GNUselinuxdefs
endif

TESTDIR = $(PCP_VAR_DIR)/testsuite
TESTS	= $(shell sed -n -e '/^[0-9][0-9]*:retired/d' -e '/^[0-9][0-9]*:reserved/d' -e '/^[0-9]/s/[        ].*//' -e '/^[0-9]/p' <group)

SUBDIRS = src pmdas cisco gluster pconf sadist collectl nfsclient named \
	  archives badarchives views qt linux unbound cifs gpfs lustre ganglia \
	  java mmv postfix perl json slurm tmparch sheet smart admin hacluster \
	  sockets denki

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

# No implicit rules here, thank you
#
.SUFFIXES:

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
	 make.out qa_hosts qa_outfiles stap_*.ko localconfig check.time \
	 pcpqa.pp pcpqa.fc pcpqa.if pcpqa.te

# these ones are links to the real files created when the associated
# test is run
LDIRT += $(shell [ -f .gitignore ] && grep '\.out$$' .gitignore)

SCRIPTS = mk.localconfig mk.pcpversion mk.qa_hosts mk.variant getpmcdhosts \
	  new check recheck remake findmetric group-stats \
	  check-gitignore check-group chk.setup show-me disk-work 008.helper \
	  xlate_2_new_pmns common setup.051 mk.logfarm check.callback.sample \
	  daily-cleanup

COMMON_SCRIPTS = common.install.cisco common.pcpweb
COMMON = common.check common.config common.filter \
	 common.product common.rc common.setup \
	 common.secure common.gfs2 \
	 common.qt common.containers \
	 common.discovery common.compress \
	 common.python common.openmetrics \
	 common.bcc common.bpf common.bpftrace common.redis

OTHERS = group triaged qa_hosts.primary README sanity.coverage \
	 $(shell echo valgrind-suppress* helgrind-suppress*)

ifeq "$(ENABLE_SELINUX)" "true"
SELINUX_PCPQA = pcpqa.te
OTHERS += GNUselinuxdefs $(SELINUX_PCPQA).in $(SELINUX_PCPQA)
else
SELINUX_PCPQA =
endif

DOTOUTFILES = $(shell [ -f qa_outfiles ] && cat qa_outfiles || ls -1 | grep '^[0-9]' | grep -v '^[0-9][0-9][0-9]$$' | grep -v '^[0-9][0-9][0-9][0-9]$$' | egrep -v '^[0-9][0-9][0-9]*\.(full|out\.bad|notrun|helper|work)' | tee qa_outfiles)

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
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

# and do nothing if NNN.out is older than NNN
#
%.out: %

debug-me:
	@echo TESTS=$(TESTS)
	@echo SRCFILES=$(SRCFILES)
	@echo CONFFILES=$(CONFFILES)
	@echo SUBDIRS=$(SUBDIRS)
	@echo SELINUX_PCPQA=$(SELINUX_PCPQA)

default_pcp default setup:	$(SELINUX_PCPQA)

pcpqa.te:	GNUselinuxdefs pcpqa.te.in
	$(SED) <pcpqa.te.in >pcpqa.te \
	    -e 's+@PCP_INITRC_TMP_T@+'$(PCP_INITRC_TMP_T)'+' \
	    -e 's+@PCP_INITRC_TMP_RULE_1@+'$(PCP_INITRC_TMP_RULE_1)'+' \
	    -e 's+@PCP_INITRC_TMP_RULE_2@+'$(PCP_INITRC_TMP_RULE_2)'+' \
	    -e 's+@PCP_SUDO_EXEC_T@+'$(PCP_SUDO_EXEC_T)'+' \
	    -e 's+@PCP_SUDO_EXEC_RULE@+'$(PCP_SUDO_EXEC_RULE)'+' \
	    -e 's+@PCP_SECURITY_T@+'$(PCP_SECURITY_T)'+' \
	    -e 's+@PCP_SECURITY_RULE@+'$(PCP_SECURITY_RULE)'+' \
	    -e 's+@PCP_CHKPWD_EXEC_T@+'$(PCP_CHKPWD_EXEC_T)'+' \
	    -e 's+@PCP_CHKPWD_EXEC_RULE@+'$(PCP_CHKPWD_EXEC_RULE)'+' \
	    -e 's+@PCP_INITRC_T@+'$(PCP_INITRC_T)'+' \
	    -e 's+@PCP_INITRC_RULE@+'$(PCP_INITRC_RULE)'+' \
	    -e 's+@PCP_VAR_LOG_T@+'$(PCP_VAR_LOG_T)'+' \
	    -e 's+@PCP_VAR_LOG_RULE@+'$(PCP_VAR_LOG_RULE)'+' \
	    -e 's+@PCP_LASTLOG_T@+'$(PCP_LASTLOG_T)'+' \
	    -e 's+@PCP_LASTLOG_RULE@+'$(PCP_LASTLOG_RULE)'+' \
	    -e 's+@PCP_XAUTH_EXEC_T@+'$(PCP_XAUTH_EXEC_T)'+' \
	    -e 's+@PCP_XAUTH_EXEC_RULE@+'$(PCP_XAUTH_EXEC_RULE)'+' \
	    -e 's+@PCP_LOGROTATE_T@+'$(PCP_LOGROTATE_T)'+' \
	    -e 's+@PCP_LOGROTATE_RULE_1@+'$(PCP_LOGROTATE_RULE_1)'+' \
	    -e 's+@PCP_LOGROTATE_RULE_2@+'$(PCP_LOGROTATE_RULE_2)'+' \
	    -e 's+@PCP_FAILLOG_T@+'$(PCP_FAILLOG_T)'+' \
	    -e 's+@PCP_FAILLOG_RULE@+'$(PCP_FAILLOG_RULE)'+' \
	    -e 's+@PCP_KMOD_EXEC_T@+'$(PCP_KMOD_EXEC_T)'+' \
	    -e 's+@PCP_KMOD_EXEC_RULE@+'$(PCP_KMOD_EXEC_RULE)'+' \
	    -e 's+@PCP_NETLINK_AUDIT_SOCKET_CLASS@+'$(PCP_NETLINK_AUDIT_SOCKET_CLASS)'+' \
	    -e 's+@PCP_NETLINK_AUDIT_SOCKET_RULE@+'$(PCP_NETLINK_AUDIT_SOCKET_RULE)'+' \
	    -e 's+@PCP_NETLINK_SELINUX_SOCKET_CLASS@+'$(PCP_NETLINK_SELINUX_SOCKET_CLASS)'+' \
	    -e 's+@PCP_NETLINK_SELINUX_SOCKET_RULE@+'$(PCP_NETLINK_SELINUX_SOCKET_RULE)'+' \
	    -e 's+@PCP_SOCK_FILE_CLASS@+'$(PCP_SOCK_FILE_CLASS)'+' \
	    -e 's+@PCP_SOCK_FILE_RULE@+'$(PCP_SOCK_FILE_RULE)'+' \
	    -e 's+@PCP_SECURITY_CLASS@+'$(PCP_SECURITY_CLASS)'+' \
	    -e 's+@PCP_SECURITY_RULE@+'$(PCP_SECURITY_RULE)'+' \
	    -e 's+@PCP_DIR_CLASS@+'$(PCP_DIR_CLASS)'+' \
	    -e 's+@PCP_DIR_RULE@+'$(PCP_DIR_RULE)'+' \
	    -e 's+@PCP_RAWIP_SOCKET_CLASS@+'$(PCP_RAWIP_SOCKET_CLASS)'+' \
	    -e 's+@PCP_RAWIP_SOCKET_RULE@+'$(PCP_RAWIP_SOCKET_RULE)'+' \
	    -e 's+@PCP_MAP_OP@+'$(PCP_MAP_OP)'+' \
	    -e 's+@PCP_MAP_RULE@+'$(PCP_MAP_RULE)'+' \
	# end
