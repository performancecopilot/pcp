TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/archives
SRCARCH = chartqa1.0 viewqa1.0 viewqa2.0 viewqa3.0 rattle.0 oview-short.0 \
	  snort-disks.0 vldb-disks.0 moomba.pmkstat.0 fcsw_indom.0 gap.0 gap2.0
# make substitution/expansion does funky things with these filenames
DATEARCH = 20071023.08\:06\:36.0 20071023.08\:06\:36.index 20071023.08\:06\:36.meta
SRCEXTRAS = $(subst 0,meta,$(SRCARCH)) $(subst 0,index,$(SRCARCH))
OTHERS = mk.gap.sh GNUmakefile.install

LSRCFILES = $(SRCARCH) $(SRCEXTRAS) $(DATEARCH) $(OTHERS)

default setup:

install:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 755 $(SRCARCH) $(SRCEXTRAS) $(DATEARCH) $(TESTDIR)

include $(BUILDRULES)
