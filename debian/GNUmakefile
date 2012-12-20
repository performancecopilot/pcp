TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

LINTIAN = source.lintian-overrides
LINTDIR = $(PCP_SHARE_DIR)/../lintian/overrides

PCP_FILES = pcp.preinst pcp.prerm pcp.postinst pcp.postrm pcp.conffiles
PCP_LINT = pcp.lintian 

PCPTESTSUITE = pcp-testsuite
PCPTESTSUITE_FILES = pcp-testsuite.dirs pcp-testsuite.postinst
PCPTESTSUITE_DOC_DIR = $(PCP_DOC_DIR)/../pcp-testsuite

PCPIMPORTSAR = pcp-import-sar2pcp
PCPIMPORTSAR_FILES = pcp-import-sar2pcp.install
PCPIMPORTSAR_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-sar2pcp

PCPIMPORTMRTG = pcp-import-mrtg2pcp
PCPIMPORTMRTG_FILES = pcp-import-mrtg2pcp.install
PCPIMPORTMRTG_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-mrtg2pcp

PCPIMPORTSHEET = pcp-import-sheet2pcp
PCPIMPORTSHEET_FILES = pcp-import-sheet2pcp.install
PCPIMPORTSHEET_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-sheet2pcp

PCPIMPORTIOSTAT = pcp-import-iostat2pcp
PCPIMPORTIOSTAT_FILES = pcp-import-iostat2pcp.install
PCPIMPORTIOSTAT_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-iostat2pcp

LIBPCPPCP = libpcp3
LIBPCP_FILES = libpcp3.dirs libpcp3.install
DEVPCP_FILES = libpcp3-dev.dirs libpcp3-dev.install
LIBPCP_DOC_DIR = $(PCP_DOC_DIR)/../libpcp3
DEVPCP_DOC_DIR = $(PCP_DOC_DIR)/../libpcp3-dev

LIBGUIPCP = libpcp-gui2
LIBGUI_FILES = libpcp-gui2.install
DEVGUI_FILES = libpcp-gui2-dev.dirs libpcp-gui2-dev.install
LIBGUI_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-gui2
DEVGUI_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-gui2-dev

LIBMMVPCP = libpcp-mmv1
LIBMMV_FILES = libpcp-mmv1.install
DEVMMV_FILES = libpcp-mmv1-dev.dirs libpcp-mmv1-dev.install
LIBMMV_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-mmv1
DEVMMV_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-mmv1-dev

LIBPMDAPCP = libpcp-pmda3
LIBPMDA_FILES = libpcp-pmda3.install
DEVPMDA_FILES = libpcp-pmda3-dev.dirs libpcp-pmda3-dev.install
LIBPMDA_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-pmda3
DEVPMDA_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-pmda3-dev

LIBTRACEPCP = libpcp-trace2
LIBTRACE_FILES = libpcp-trace2.install
DEVTRACE_FILES = libpcp-trace2-dev.dirs libpcp-trace2-dev.install
LIBTRACE_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-trace2
DEVTRACE_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-trace2-dev

LIBIMPORTPCP = libpcp-import1
LIBIMPORT_FILES = libpcp-import1.install
DEVIMPORT_FILES = libpcp-import1-dev.dirs libpcp-import1-dev.install
LIBIMPORT_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-import1
DEVIMPORT_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-import1-dev

SUBDIRS = source
LDIRT = *.debhelper *.substvars *.log pcp files pcp.preinst pcp.preinst.head \
	$(LIBPCPPCP) $(LIBPCPPCP)-dev \
	$(LIBPMDAPCP) $(LIBPMDAPCP)-dev $(LIBTRACEPCP) $(LIBTRACEPCP)-dev \
	$(LIBGUIPCP) $(LIBGUIPCP)-dev $(LIBMMVPCP) $(LIBMMVPCP)-dev \
	$(LIBIMPORTPCP) $(LIBIMPORTPCP)-dev \
	$(PCPIMPORTSAR) $(PCPIMPORTMRTG) $(PCPIMPORTSHEET) $(PCPIMPORTIOSTAT)
LSRCFILES = changelog compat control copyright rules \
		pcp.preinst.head.in cleanconfigs pcp.preinst.tail \
		$(LINTIAN) $(PCP_LINT) \
		$(PCP_FILES) $(LIBPCP_FILES) $(DEVPCP_FILES) \
		$(PCPTESTSUITE_FILES) \
		$(LIBGUI_FILES) $(DEVGUI_FILES) \
		$(LIBMMV_FILES) $(DEVMMV_FILES) \
		$(LIBPMDA_FILES) $(DEVPMDA_FILES) \
		$(LIBTRACE_FILES) $(DEVTRACE_FILES) \
		$(LIBIMPORT_FILES) $(DEVIMPORT_FILES) \
		$(PCPIMPORTSAR_FILES) $(PCPIMPORTMRTG_FILES) \
		$(PCPIMPORTSHEET_FILES) $(PCPIMPORTIOSTAT_FILES)

default: pcp.preinst

include $(BUILDRULES)

install: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LINTDIR)
	$(INSTALL) -m 644 $(PCP_LINT) $(LINTDIR)/pcp
endif

default_pcp : default

install_pcp : install

install-testsuite: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPTESTSUITEDIR)
	$(INSTALL) -m 644 copyright $(PCPTESTSUITEDIR)
	$(INSTALL) -m 644 changelog $(PCPTESTSUITEDIR)/changelog.Debian
endif

install-libpcp: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LIBPCP_DOC_DIR)
	$(INSTALL) -m 644 copyright $(LIBPCP_DOC_DIR)
	$(INSTALL) -m 644 changelog $(LIBPCP_DOC_DIR)/changelog.Debian
endif

install-devpcp: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(DEVPCP_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEVPCP_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEVPCP_DOC_DIR)/changelog.Debian
endif

install-libgui: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LIBGUI_DOC_DIR)
	$(INSTALL) -m 644 copyright $(LIBGUI_DOC_DIR)
	$(INSTALL) -m 644 changelog $(LIBGUI_DOC_DIR)/changelog.Debian
endif

install-devgui: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(DEVGUI_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEVGUI_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEVGUI_DOC_DIR)/changelog.Debian
endif

install-libmmv: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LIBMMV_DOC_DIR)
	$(INSTALL) -m 644 copyright $(LIBMMV_DOC_DIR)
	$(INSTALL) -m 644 changelog $(LIBMMV_DOC_DIR)/changelog.Debian
endif

install-devmmv: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(DEVMMV_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEVMMV_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEVMMV_DOC_DIR)/changelog.Debian
endif

install-libpmda: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LIBPMDA_DOC_DIR)
	$(INSTALL) -m 644 copyright $(LIBPMDA_DOC_DIR)
	$(INSTALL) -m 644 changelog $(LIBPMDA_DOC_DIR)/changelog.Debian
endif

install-devpmda: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(DEVPMDA_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEVPMDA_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEVPMDA_DOC_DIR)/changelog.Debian
endif

install-libtrace: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LIBTRACE_DOC_DIR)
	$(INSTALL) -m 644 copyright $(LIBTRACE_DOC_DIR)
	$(INSTALL) -m 644 changelog $(LIBTRACE_DOC_DIR)/changelog.Debian
endif

install-devtrace: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(DEVTRACE_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEVTRACE_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEVTRACE_DOC_DIR)/changelog.Debian
endif

install-libimport: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LIBIMPORT_DOC_DIR)
	$(INSTALL) -m 644 copyright $(LIBIMPORT_DOC_DIR)
	$(INSTALL) -m 644 changelog $(LIBIMPORT_DOC_DIR)/changelog.Debian
endif

install-devimport: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(DEVIMPORT_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEVIMPORT_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEVIMPORT_DOC_DIR)/changelog.Debian
endif

install-pcpimportsar: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPIMPORTSAR_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPIMPORTSAR_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPIMPORTSAR_DOC_DIR)/changelog.Debian
endif

install-pcpimportmrtg: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPIMPORTMRTG_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPIMPORTMRTG_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPIMPORTMRTG_DOC_DIR)/changelog.Debian
endif

install-pcpimportsheet: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPIMPORTSHEET_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPIMPORTSHEET_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPIMPORTSHEET_DOC_DIR)/changelog.Debian
endif

install-pcpimportiostat: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPIMPORTIOSTAT_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPIMPORTIOSTAT_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPIMPORTIOSTAT_DOC_DIR)/changelog.Debian
endif

pcp.preinst:	pcp.preinst.head cleanconfigs pcp.preinst.tail
	rm -f $@
	cat $? >$@
