TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

#VIEWDIR	= $(PCP_VAR_DIR)/config/pmview
VIEWDIR	= $(PCP_BIN_DIR)
LOGDIR	= $(PCP_VAR_DIR)/config/pmlogger
ARGSDIR	= $(PCP_SHARE_DIR)/lib

VIEWS	= clustervis dkvis mpvis nfsvis osvis weblogvis webpingvis webvis
LOGGERS	= config.dkvis config.mpvis config.nfsvis config.osvis \
	  config.weblogvis config.webpingvis config.webvis

LSRCFILES = $(VIEWS) $(LOGGERS) pmview-args

default build-me:

include $(BUILDRULES)

install:	default
	#$(INSTALL) -m 755 -d $(VIEWDIR)
	$(INSTALL) -m 755 $(VIEWS) $(VIEWDIR)
	$(INSTALL) -m 755 -d $(LOGDIR)
	$(INSTALL) -m 444 $(LOGGERS) $(LOGDIR)
	$(INSTALL) -m 755 -d $(ARGSDIR)
	$(INSTALL) -m 755 pmview-args $(ARGSDIR)/pmview-args
