TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

BUNDLE = diskmodel
BINTAR = $(BUNDLE).tar.gz
PCPLOGS = $(shell echo *.0 *.meta *.index)
CONFIGS = model.fio model.folio model.view
MODELS = model.pl model.xls nasread.out naswrite.out
LSRCFILES = $(PCPLOGS) $(CONFIGS) $(MODELS)
LDIRT = $(BINTAR) manifest

# Following a suggestion from Vagrant Cascadian (@reproducible-builds.org)
# to use $(SOURCE_DATE_EPOCH) if it is set in the build environment to make
# the datestamps in the tarball deterministic.
#
DATE_FMT = +%Y-%m-%d %H:%M:%S
ifdef SOURCE_DATE_EPOCH
BUILD_DATE ?= $(shell date -u -d "@$(SOURCE_DATE_EPOCH)" "$(DATE_FMT)" 2>/dev/null || date -u -r "$(SOURCE_DATE_EPOCH)" "$(DATE_FMT)" 2>/dev/null || date -u "$(DATE_FMT)")
else
BUILD_DATE ?= $(shell date -u "$(DATE_FMT)")
endif

default: $(BINTAR)

$(BINTAR): $(PCPLOGS) $(CONFIGS) $(MODELS)
	@ CDIR=`pwd`; cd ..; rm -f manifest; \
	for f in `echo $^`; do \
	    echo $(BUNDLE)/$$f >> $$CDIR/manifest; \
	done; \
	$(TAR) --format ustar -T $$CDIR/manifest --sort=name --mtime="$(BUILD_DATE)" --owner=0 --group=0 --numeric-owner --mode=u=wrX,og=rX -cf - | $(ZIP) --best --no-name > $$CDIR/$(BINTAR); \
	echo "Created $(BINTAR)"

include $(BUILDRULES)

install install-dev: default
	$(INSTALL) -m 755 -d $(PCP_DEMOS_DIR)/tutorials
	$(INSTALL) -m 644 $(BINTAR) $(PCP_DEMOS_DIR)/tutorials/$(BINTAR)
	$(INSTALL) -m 755 -d $(PCP_HTML_DIR)/$(BUNDLE)
	$(INSTALL) -m 644 $(MODELS) $(CONFIGS) $(PCP_HTML_DIR)/$(BUNDLE)

default_pcp : default

install_pcp : install
