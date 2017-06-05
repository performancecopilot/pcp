TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

CONFIGS	= sample.json
METADIR	= $(PCP_PMDAS_DIR)/prometheus/metadata

default build-me:

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 -d $(METADIR)
	$(INSTALL) -m 644 $(CONFIGS) $(METADIR)/$(CONFIGS)

default_pcp:	default

install_pcp:	install
