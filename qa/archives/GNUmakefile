TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives

SRCARCH = $(shell echo *.[0-9])
SRCMETA = $(subst .0,.meta,$(SRCARCH))
SRCIDX = $(subst .0,.index,$(SRCARCH))
FOLIOS = $(shell echo *.folio)

SCRIPTS = mk.diff.sh mk.gap.sh mk.mysql.sh mk.schizo.sh mk.foo+.sh \
	  mk.dynmetric.sh mk.instant.sh \
	  mkbadlen mkbadti mkbadlabel mkbadlog
CONFIGS = config.verify config.shping YWhcCi.toium.config
OTHERS = ds390-access.gz collectl-qa

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
