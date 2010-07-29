TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

BINTAR = diskmodel.tar.gz
PCPLOGS = $(shell echo *.0 *.meta *.index)
CONFIGS = model.fio model.folio model.view
MODELS = model.pl model.xls nasread.out naswrite.out

LSRCFILES = $(HTMLPAGES) $(CONFIGURE)
LDIRT = $(BINTAR) manifest

default: $(BINTAR)

$(BINTAR): $(PCPLOGS) $(CONFIGS) $(MODELS)
	@ CDIR=`pwd`; cd ..; rm -f manifest; \
	for f in `echo $^`; do \
	    echo diskmodel/$$f >> $$CDIR/manifest; \
	done; \
	$(TAR) -T $$CDIR/manifest -cf - | $(ZIP) --best > $$CDIR/$(BINTAR); \
	echo "Created $(BINTAR)"

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PKG_DEMOS_DIR)/tutorials
	$(INSTALL) -m 644 $(BINTAR) $(PKG_DEMOS_DIR)/tutorials
	$(INSTALL) -m 755 -d $(PKG_HTML_DIR)/diskmodel
	$(INSTALL) -m 644 $(MODELS) $(CONFIGS) $(PKG_HTML_DIR)/tutorials
