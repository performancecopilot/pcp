TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

BUNDLE = cpuperf
BINTAR = $(BUNDLE).tar.gz
PCPLOGS = $(shell echo *.0 *.meta *.index)
CONFIGS = 
LSRCFILES = $(PCPLOGS) $(CONFIGS)
LDIRT = $(BINTAR) manifest

default: $(BINTAR)

$(BINTAR): $(PCPLOGS) $(CONFIGS)
	@ CDIR=`pwd`; cd ..; rm -f manifest; \
	for f in `echo $^`; do \
	    echo $(BUNDLE)/$$f >> $$CDIR/manifest; \
	done; \
	$(TAR) -T $$CDIR/manifest -cf - | $(ZIP) --best > $$CDIR/$(BINTAR); \
	echo "Created $(BINTAR)"

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PCP_DEMOS_DIR)/tutorials
	$(INSTALL) -m 644 $(BINTAR) $(PCP_DEMOS_DIR)/tutorials/$(BINTAR)

default_pcp : default

install_pcp : install
