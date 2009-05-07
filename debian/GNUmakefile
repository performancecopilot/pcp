TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

PCP_FILES = pcp.prerm pcp.postinst pcp.postrm

LIBPCPPCP = libpcp3
LIBPCP_FILES = libpcp3.install
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

LDIRT = *.debhelper *.substvars *.log pcp files $(LIBPCPPCP) $(LIBPCPPCP)-dev \
	$(LIBPMDAPCP) $(LIBPMDAPCP)-dev $(LIBTRACEPCP) $(LIBTRACEPCP)-dev \
	$(LIBGUIPCP) $(LIBGUIPCP)-dev $(LIBMMVPCP) $(LIBMMVPCP)-dev
LSRCFILES = changelog compat control copyright rules source.lintian-overrides \
		$(PCP_FILES) $(LIBPCP_FILES) $(DEVPCP_FILES) shlibs.local \
		$(LIBGUI_FILES) $(DEVGUI_FILES) \
		$(LIBMMV_FILES) $(DEVMMV_FILES) \
		$(LIBPMDA_FILES) $(DEVPMDA_FILES) \
		$(LIBTRACE_FILES) $(DEVTRACE_FILES)

default:

include $(BUILDRULES)

install: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCP_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCP_DOC_DIR)/changelog.Debian
endif

default_pcp : default

install_pcp : install

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
