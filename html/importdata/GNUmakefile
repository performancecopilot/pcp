TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

BUNDLE = importdata
SCRIPT = mover2pcp
LSRCFILES = $(SCRIPT) \
	README moverv1 moverv2 moverv3 moverv4 \
	mk.mover.log mover.log

default:

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PCP_HTML_DIR)/$(BUNDLE)
	$(INSTALL) -m 644 $(SCRIPT) $(PCP_HTML_DIR)/$(BUNDLE)/$(SCRIPT)

default_pcp : default

install_pcp : install
