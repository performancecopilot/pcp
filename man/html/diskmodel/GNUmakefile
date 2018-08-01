TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

BUNDLE = diskmodel
BINTAR = $(BUNDLE).tar.gz
PCPLOGS = $(shell echo *.0 *.meta *.index)
CONFIGS = model.fio model.folio model.view
MODELS = model.pl model.xls nasread.out naswrite.out
LSRCFILES = $(PCPLOGS) $(CONFIGS) $(MODELS)
LDIRT = $(BINTAR) manifest

default: $(BINTAR)

$(BINTAR): $(PCPLOGS) $(CONFIGS) $(MODELS)
	@ CDIR=`pwd`; cd ..; rm -f manifest; \
	for f in `echo $^`; do \
	    echo $(BUNDLE)/$$f >> $$CDIR/manifest; \
	done; \
	$(TAR) --format ustar -T $$CDIR/manifest -cf - | $(ZIP) --best --no-name > $$CDIR/$(BINTAR); \
	echo "Created $(BINTAR)"

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PCP_DEMOS_DIR)/tutorials
	$(INSTALL) -m 644 $(BINTAR) $(PCP_DEMOS_DIR)/tutorials/$(BINTAR)
	$(INSTALL) -m 755 -d $(PCP_BOOKS_DIR)/html/$(BUNDLE)
	$(INSTALL) -m 644 $(MODELS) $(CONFIGS) $(PCP_BOOKS_DIR)/html/$(BUNDLE)

default_pcp : default

install_pcp : install
