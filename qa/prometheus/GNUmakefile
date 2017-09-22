#!gmake

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/prometheus
SERVER = prometheus_client_server.python
ENDPOINT = prometheus_endpoint.python

default default_pcp setup: prometheus_client_server.python prometheus_endpoint.python

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SERVER) $(TESTDIR)/$(SERVER)
	$(INSTALL) -m 755 $(ENDPOINT) $(TESTDIR)/$(ENDPOINT)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

include $(BUILDRULES)
