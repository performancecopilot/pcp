TOPDIR = ..
PWDDIR = $(shell pwd)
include $(TOPDIR)/src/include/builddefs
SCRIPTS = $(TOPDIR)/build

LINTIAN = source.lintian-overrides
LINTDIR = $(PCP_SHARE_DIR)/../lintian/overrides

PCP_FILES = pcp.preinst pcp.prerm pcp.postinst pcp.postrm pcp.conffiles
PCP_LINT = pcp.lintian 

PCPDOC = pcp-doc
PCPDOC_FILES = pcp-doc.dirs pcp-doc.install
PCPDOC_DOC_DIR = $(PCP_DOC_DIR)/../pcp-doc

PCPGUI = pcp-gui
PCPGUI_FILES = pcp-gui.dirs pcp-gui.install
PCPGUI_DOC_DIR = $(PCP_DOC_DIR)/../pcp-gui

PCPWEBAPI = pcp-webapi
PCPWEBAPI_FILES = pcp-webapi.dirs pcp-webapi.install pcp-webapi.prerm \
		  pcp-webapi.postinst pcp-webapi.postrm
PCPWEBAPI_DOC_DIR = $(PCP_DOC_DIR)/../pcp-webapi

PCPMANAGER = pcp-manager
PCPMANAGER_FILES = pcp-manager.dirs pcp-manager.install pcp-manager.prerm \
		   pcp-manager.postinst pcp-manager.postrm
PCPMANAGER_DOC_DIR = $(PCP_DOC_DIR)/../pcp-manager

PCPTESTSUITE = pcp-testsuite
PCPTESTSUITE_FILES = pcp-testsuite.dirs pcp-testsuite.postinst
PCPTESTSUITE_DOC_DIR = $(PCP_DOC_DIR)/../pcp-testsuite

PCPIMPORTSAR = pcp-import-sar2pcp
PCPIMPORTSAR_FILES = pcp-import-sar2pcp.install
PCPIMPORTSAR_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-sar2pcp

PCPIMPORTSAR = pcp-import-ganglia2pcp
PCPIMPORTSAR_FILES = pcp-import-ganglia2pcp.install
PCPIMPORTSAR_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-ganglia2pcp

PCPIMPORTMRTG = pcp-import-mrtg2pcp
PCPIMPORTMRTG_FILES = pcp-import-mrtg2pcp.install
PCPIMPORTMRTG_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-mrtg2pcp

PCPIMPORTSHEET = pcp-import-sheet2pcp
PCPIMPORTSHEET_FILES = pcp-import-sheet2pcp.install
PCPIMPORTSHEET_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-sheet2pcp

PCPIMPORTIOSTAT = pcp-import-iostat2pcp
PCPIMPORTIOSTAT_FILES = pcp-import-iostat2pcp.install
PCPIMPORTIOSTAT_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-iostat2pcp

PCPIMPORTCOLLECTL = pcp-import-collectl2pcp
PCPIMPORTCOLLECTL_FILES = pcp-import-collectl2pcp.install
PCPIMPORTCOLLECTL_DOC_DIR = $(PCP_DOC_DIR)/../pcp-import-collectl2pcp

PCPPMDAINFINIBAND = pcp-pmda-infiniband
PCPPMDAINFINIBAND_FILES = pcp-pmda-infiniband.dirs pcp-pmda-infiniband.install
PCPPMDAINFINIBAND_DOC_DIR = $(PCP_DOC_DIR)/../pcp-pmda-infiniband

PCPCFG = pcp-conf
PCPCFG_FILES = pcp-conf.dirs pcp-conf.install
PCPCFG_DOC_DIR = $(PCP_DOC_DIR)/../pcp-conf

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

LIBWEBPCP = libpcp-web1
LIBWEB_FILES = libpcp-web1.install
DEVWEB_FILES = libpcp-web1-dev.dirs libpcp-web1-dev.install
LIBWEB_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-web1
DEVWEB_DOC_DIR = $(PCP_DOC_DIR)/../libpcp-web1-dev

SUBDIRS = source
LDIRT = *.debhelper *.substvars *.log pcp files pcp.postrm \
	pcp.preinst pcp.preinst.head pcp.postinst pcp.postinst.head \
	pcp-webapp-vector.dirs pcp-webapp-vector.install \
	$(LIBPCPPCP) $(LIBPCPPCP)-dev \
	$(LIBPMDAPCP) $(LIBPMDAPCP)-dev $(LIBTRACEPCP) $(LIBTRACEPCP)-dev \
	$(LIBGUIPCP) $(LIBGUIPCP)-dev $(LIBMMVPCP) $(LIBMMVPCP)-dev \
	$(LIBIMPORTPCP) $(LIBIMPORTPCP)-dev \
	$(LIBWEBPCP) $(LIBWEBPCP)-dev \
	$(PCPIMPORTSAR) $(PCPIMPORTMRTG) $(PCPIMPORTSHEET) $(PCPIMPORTIOSTAT) \
	$(PCPIMPORTCOLLECTL)

default: pcp.preinst pcp.postinst pcp.postrm vector control

include $(BUILDRULES)

install: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LINTDIR)
	$(INSTALL) -m 644 $(PCP_LINT) $(LINTDIR)/pcp
endif

default_pcp : default

install_pcp : install

install-pcpgui:
ifeq ($(ENABLE_QT), true)
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPGUI_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPGUI_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPGUI_DOC_DIR)/changelog.Debian
endif
endif

install-webapi:
ifeq ($(ENABLE_WEBAPI), true)
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPWEBAPI_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPWEBAPI_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPWEBAPI_DOC_DIR)/changelog.Debian
endif
endif

install-manager: default
ifeq ($(ENABLE_MANAGER), true)
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPMANAGER_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPMANAGER_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPMANAGER_DOC_DIR)/changelog.Debian
endif
endif

install-testsuite: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPTESTSUITE_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPTESTSUITE_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPTESTSUITE_DOC_DIR)/changelog.Debian
endif

install-pcpcfg: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPCFG_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPCFG_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPCFG_DOC_DIR)/changelog.Debian
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

install-libweb: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(LIBWEB_DOC_DIR)
	$(INSTALL) -m 644 copyright $(LIBWEB_DOC_DIR)
	$(INSTALL) -m 644 changelog $(LIBWEB_DOC_DIR)/changelog.Debian
endif

install-devweb: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(DEVWEB_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEVWEB_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEVWEB_DOC_DIR)/changelog.Debian
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

install-pcpimportcollectl: default
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPIMPORTCOLLECTL_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPIMPORTCOLLECTL_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPIMPORTCOLLECTL_DOC_DIR)/changelog.Debian
endif

install-pcppmdainfiniband: default
ifeq ($(PMDA_INFINIBAND), true)
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PCPPMDAINFINIBAND_DOC_DIR)
	$(INSTALL) -m 644 copyright $(PCPPMDAINFINIBAND_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PCPPMDAINFINIBAND_DOC_DIR)/changelog.Debian
endif
endif

pcp.preinst:	pcp.preinst.head $(SCRIPTS)/cleanconfigs configversions pcp.preinst.tail
	rm -f $@
	cat pcp.preinst.head $(SCRIPTS)/cleanconfigs configversions pcp.preinst.tail > $@

pcp.postinst:	pcp.postinst.head $(SCRIPTS)/cleantmpdirs configversions pcp.postinst.tail
	rm -f $@
	cat pcp.postinst.head $(SCRIPTS)/cleantmpdirs configversions pcp.postinst.tail > $@

pcp.postrm:	pcp.postrm.head configversions pcp.postrm.tail
	rm -f $@
	cat pcp.postrm.head configversions pcp.postrm.tail > $@

VECTOR = $(shell echo $(PCP_SHARE_DIR)/webapps/vector | sed -e 's,^/,,g')
VECTORPKG = pcp-webapp-vector
VECTORTGZ = $(PWDDIR)/../../../tar/$(VECTORPKG).tar.gz

vector:
ifeq ($(HAVE_WEBAPPS), true)
ifeq ($(PACKAGE_DISTRIBUTION), debian)
	mkdir -p pcp/$(VECTOR) 2>/dev/null || exit 0
	cd pcp/$(VECTOR) && $(TAR) xzf $(VECTORTGZ) --strip-components=1
	( cd pcp/$(VECTOR) && find . -type d ) \
		| sed -e "s,^.,$(VECTOR),g" > $(VECTORPKG).dirs
	( cd pcp/$(VECTOR) && find . -type f ) \
		| sed -e "s,^.,$(VECTOR),g" > $(VECTORPKG).install
endif
endif

control:	control.master control.webapi control.pcpgui fixcontrol.master
	./fixcontrol.master >control
ifeq ($(HAVE_WEBAPPS), true)
	cat control.vector >>control
endif
ifeq ($(ENABLE_WEBAPI), true)
	cat control.webapi >>control
endif
ifeq ($(ENABLE_QT), true)
	cat control.pcpgui >>control
endif
ifeq ($(PMDA_INFINIBAND), true)
	cat control.pmdaib >>control
endif
