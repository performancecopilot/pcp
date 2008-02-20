TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

LIBPCPPKG = libpcp3
LIBPCP_FILES = libpcp3.install
DEVPCP_FILES = libpcp3-dev.install
LIBPCP_DOC_DIR = $(PKG_DOC_DIR)/../libpcp3
DEVPCP_DOC_DIR = $(PKG_DOC_DIR)/../libpcp3-dev

LIBPMDAPKG = libpcp-pmda3
LIBPMDA_FILES = libpcp-pmda3.install
DEVPMDA_FILES = libpcp-pmda3-dev.install
LIBPMDA_DOC_DIR = $(PKG_DOC_DIR)/../libpcp-pmda3
DEVPMDA_DOC_DIR = $(PKG_DOC_DIR)/../libpcp-pmda3-dev

LIBTRACEPKG = libpcp-trace2
LIBTRACE_FILES = libpcp-trace2.install
DEVTRACE_FILES = libpcp-trace2-dev.install
LIBTRACE_DOC_DIR = $(PKG_DOC_DIR)/../libpcp-trace2
DEVTRACE_DOC_DIR = $(PKG_DOC_DIR)/../libpcp-trace2-dev

LDIRT = compat
LSRCFILES = changelog control copyright rules \
		$(LIBPCP_FILES) $(DEVPCP_FILES) \
		$(LIBPMDA_FILES) $(DEVPMDA_FILES) \
		$(LIBTRACE_FILES) $(DEVTRACE_FILES)

default:

include $(BUILDRULES)

install: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PKG_DOC_DIR)/changelog.Debian
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
