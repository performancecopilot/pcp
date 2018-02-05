#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/prometheus
SERVER = prometheus_client_server.python
ENDPOINT = prometheus_endpoint.python
SUBDIRS = samples scripts

default default_pcp setup: $(SERVER) $(ENDPOINT) $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install install_pcp: $(SUBDIRS)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SERVER) $(TESTDIR)/$(SERVER)
	$(INSTALL) -m 755 $(ENDPOINT) $(TESTDIR)/$(ENDPOINT)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
