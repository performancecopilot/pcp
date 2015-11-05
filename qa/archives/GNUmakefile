TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives

SRCARCH = $(shell echo *.[0-9])
SRCMETA = $(subst .0,.meta,$(SRCARCH))
SRCIDX = $(subst .0,.index,$(SRCARCH))
FOLIOS = $(shell echo *.folio)

# now add in the special and odd ones
#
SRCMETA += bozo.client_v2.meta.bad.1 bozo.client_v2.meta.bad.2

SCRIPTS = mk.diff.sh mk.gap.sh mk.mysql.sh mk.schizo.sh mk.foo+.sh \
	  mk.dynmetric.sh mk.instant.sh \
	  mkbadlen mkbadti mkbadlabel mkbadlog mkmirage
CONFIGS = config.verify config.shping YWhcCi.toium.config
OTHERS = ds390-access.gz collectl-qa small.0.xz small.meta small.index \
	bigace.pmns

default default_pcp setup:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 $(SRCARCH) $(TESTDIR)
	$(INSTALL) -m 644 $(SRCMETA) $(TESTDIR)
	$(INSTALL) -m 644 $(SRCIDX) $(TESTDIR)
	$(INSTALL) -m 644 $(OTHERS) $(CONFIGS) $(FOLIOS) $(TESTDIR)

include $(BUILDRULES)

# recipes for remaking some of the archives here

mirage.0:
	rm -f mirage.index mirage.meta mirage.0
	pmlogger -c config.mirage -s 20 mirage

mirage-1.0 mirage-2.0:
	./mkmirage
