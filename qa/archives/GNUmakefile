TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives
SRCARCH = chartqa1.0 viewqa1.0 viewqa2.0 viewqa3.0 rattle.0 oview-short.0 \
	  snort-disks.0 vldb-disks.0 moomba.pmkstat.0 gap.0 gap2.0 \
	  20071023-08-06-36.0
SRCEXTRAS = $(subst .0,.meta,$(SRCARCH)) $(subst .0,.index,$(SRCARCH))
OTHERS = mk.gap.sh GNUmakefile.install

LSRCFILES = $(SRCARCH) $(SRCEXTRAS) $(OTHERS)

default setup:

install:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 $(SRCARCH) $(SRCEXTRAS) $(TESTDIR)

include $(BUILDRULES)

default_pcp:

install_pcp:
