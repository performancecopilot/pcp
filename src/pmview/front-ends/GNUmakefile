TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

VIEWS	= clustervis dkvis mpvis nfsvis osvis weblogvis webpingvis webvis
LOGGERS	= config.clustervis config.dkvis config.mpvis config.nfsvis config.osvis \
	  config.weblogvis config.webpingvis config.webvis

default build-me: $(VIEWS) $(LOGGERS)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(VIEWS) $(PCP_BIN_DIR)
	$(INSTALL) -m 644 -t $(PCP_VAR_DIR)/config/pmlogger $(LOGGERS) $(PCP_SYSCONF_DIR)/pmlogger
	$(INSTALL) -m 644 -t $(PCP_SHARE_DIR)/lib/pmview-args pmview-args $(PCP_LIBADM_DIR)/pmview-args

default_pcp:	default

install_pcp:	install
