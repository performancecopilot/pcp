TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

#VIEWDIR	= $(PCP_VAR_DIR)/config/pmview
VIEWDIR	= $(PCP_BIN_DIR)
ARGSDIR	= $(PCP_SHARE_DIR)/lib

VIEWS	= clustervis dkvis mpvis nfsvis osvis weblogvis webpingvis webvis
LOGGERS	= config.dkvis config.mpvis config.nfsvis config.osvis \
	  config.weblogvis config.webpingvis config.webvis

default build-me:

include $(BUILDRULES)

install:	default
	#$(INSTALL) -m 755 -d $(VIEWDIR)
	$(INSTALL) -m 755 $(VIEWS) $(VIEWDIR)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogger
	$(INSTALL) -m 644 $(LOGGERS) $(PCP_VAR_DIR)/config/pmlogger
	$(INSTALL) -m 755 -d $(ARGSDIR)
	$(INSTALL) -m 755 pmview-args $(ARGSDIR)/pmview-args
