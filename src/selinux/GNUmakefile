#
# Copyright (c) 2017-2018 Red Hat.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
ifeq "$(ENABLE_SELINUX)" "true"
-include ./GNUlocaldefs
endif
IAM = pcpupstream

LDIRT = $(IAM).cil $(IAM).pp $(IAM).mod $(IAM).te tmp \
	$(IAM)-docker.cil $(IAM)-docker.pp $(IAM)-docker.mod \
	$(IAM)-container.cil $(IAM)-container.pp $(IAM)-container.mod


default: build-me

include $(BUILDRULES)

ifeq "$(ENABLE_SELINUX)" "true"
build-me: $(IAM).te selinux-setup.sh

$(IAM).te: $(IAM).te.in
	$(SED) <$< >$@ \
		-e 's+@PCP_CONTAINER_RUNTIME_T@+'$(PCP_CONTAINER_RUNTIME_T)'+' \
		-e 's+@PCP_CONTAINER_RUNTIME_RULE@+'$(PCP_CONTAINER_RUNTIME_RULE)'+' \
		-e 's+@PCP_NSFS_T@+'$(PCP_NSFS_T)'+' \
		-e 's+@PCP_NSFS_RULE@+'$(PCP_NSFS_RULE)'+' \
		-e 's+@PCP_DOCKER_VAR_LIB_T@+'$(PCP_DOCKER_VAR_LIB_T)'+' \
		-e 's+@PCP_DOCKER_VAR_LIB_RULE@+'$(PCP_DOCKER_VAR_LIB_RULE)'+' \
		-e 's+@PCP_CAPUSERNS_PTRACE@+'$(PCP_CAPUSERNS_PTRACE)'+' \
		-e 's+@PCP_CAPUSERNS_PTRACE_RULE_PMIE@+'$(PCP_CAPUSERNS_PTRACE_RULE_PMIE)'+' \
		-e 's+@PCP_UNRESERVED_PORT@+'$(PCP_UNRESERVED_PORT)'+' \
		-e 's+@PCP_UNRESERVED_PORT_RULE@+'$(PCP_UNRESERVED_PORT_RULE)'+' \
		-e 's+@PCP_UNRESERVED_PORT_RULE_PMMGR@+'$(PCP_UNRESERVED_PORT_RULE_PMMGR)'+' \
		-e 's+@PCP_TRACEFS@+'$(PCP_TRACEFS)'+' \
		-e 's+@PCP_TRACEFS_FS_RULE@+'$(PCP_TRACEFS_FS_RULE)'+' \
		-e 's+@PCP_TRACEFS_DIR_RULE@+'$(PCP_TRACEFS_DIR_RULE)'+' \
		-e 's+@PCP_TRACEFS_FILE_RULE@+'$(PCP_TRACEFS_FILE_RULE)'+' \
		-e 's+@PCP_HOSTNAME_EXEC_MAP@+'$(PCP_HOSTNAME_EXEC_MAP)'+' \
		-e 's+@PCP_TMP_MAP@+'$(PCP_TMP_MAP)'+' \
		-e 's+@PCP_DEFAULT_MAP_RULE@+'$(PCP_DEFAULT_MAP_RULE)'+' \
		-e 's+@PCP_LDCONFIG_EXEC_MAP_RULE@+'$(PCP_LDCONFIG_EXEC_MAP_RULE)'+' \
		-e 's+@PCP_UNCONFINED_SERVICE@+'$(PCP_UNCONFINED_SERVICE)'+' \
		-e 's+@PCP_UNCONFINED_SERVICE_RULE@+'$(PCP_UNCONFINED_SERVICE_RULE)'+' \
		-e 's+@PCP_PMIE_UNCONFINED_SERVICE_RULE@+'$(PCP_PMIE_UNCONFINED_SERVICE_RULE)'+' \
		-e 's+@PCP_PMLOGGER_UNCONFINED_SERVICE_RULE@+'$(PCP_PMLOGGER_UNCONFINED_SERVICE_RULE)'+' \
		-e 's+@PCP_NUMAD_CONTEXT@+'$(PCP_NUMAD_CONTEXT)'+' \
		-e 's+@PCP_NUMAD_RULE@+'$(PCP_NUMAD_RULE)'+' \
		-e 's+@PCP_FSADM_EXEC_MAP@+'$(PCP_FSADM_EXEC_MAP)'+' \
		-e 's+@PCP_MMAP_ALL@+'$(PCP_MMAP_ALL)'+' \
		-e 's+@PCP_BPF_STATUS_CLASS@+'$(PCP_BPF_STATUS_CLASS)'+' \
		-e 's+@PCP_BPF_STATUS_RULE@+'$(PCP_BPF_STATUS_RULE)'+' \
		-e 's+@PCP_RPM_VAR_LIB_T@+'$(PCP_RPM_VAR_LIB_T)'+' \
		-e 's+@PCP_RPM_VAR_LIB_RULE@+'$(PCP_RPM_VAR_LIB_RULE)'+' \
		-e 's+@PCP_VIRT_VAR_RUN_T@+'$(PCP_VIRT_VAR_RUN_T)'+' \
		-e 's+@PCP_VIRT_VAR_RUN_RULE@+'$(PCP_VIRT_VAR_RUN_RULE)'+' \
		-e 's+@PCP_CAP2_SYSLOG_CLASS@+'$(PCP_CAP2_SYSLOG_CLASS)'+' \
		-e 's+@PCP_CAP2_SYSLOG_RULE@+'$(PCP_CAP2_SYSLOG_RULE)'+' \
		-e 's+@PCP_ICMP_SOCKET_CLASS@+'$(PCP_ICMP_SOCKET_CLASS)'+' \
		-e 's+@PCP_ICMP_SOCKET_RULE@+'$(PCP_ICMP_SOCKET_RULE)'+' \
		-e 's+@PCP_SELINUX_MACRO_RULE@+'$(PCP_SELINUX_MACRO_RULE)'+' \
		-e 's+@PACKAGE_VERSION@+'$(PACKAGE_VERSION)'+' \

	# END
	make -f /usr/share/selinux/devel/Makefile

install: default
	$(INSTALL) -m 755 selinux-setup.sh $(PCP_BINADM_DIR)/selinux-setup
	$(INSTALL) -m 755 -d $(PCP_SELINUX_DIR)
	$(INSTALL) -m 644 $(IAM).pp $(PCP_SELINUX_DIR)/$(IAM).pp
	$(INSTALL) -m 644 $(IAM).te $(PCP_SELINUX_DIR)/$(IAM).te
ifneq "$(PCP_DOCKER_VAR_LIB)" ""
	$(INSTALL) -m 644 $(IAM)-docker.pp $(PCP_SELINUX_DIR)/$(IAM)-docker.pp
	$(INSTALL) -m 644 $(IAM)-docker.te $(PCP_SELINUX_DIR)/$(IAM)-docker.te
endif
ifneq "$(PCP_CONTAINER_RUNTIME_T)" ""
	$(INSTALL) -m 644 $(IAM)-container.pp $(PCP_SELINUX_DIR)/$(IAM)-container.pp
	$(INSTALL) -m 644 $(IAM)-container.te $(PCP_SELINUX_DIR)/$(IAM)-container.te
endif

else
build-me:
install:
endif

default_pcp: default

install_pcp : install
