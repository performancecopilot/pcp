TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/archives
SRCARCH = 20071023-08-06-36.0 archive-20141125-000009.0 chartqa1.0 \
	diff1.0 diff2.0 dm-io.0 gap.0 gap2.0 moomba.pmkstat.0 mysql-1.0 \
	oview-short.0 rattle.0 schizo-A.0 schizo-B.0 snort-disks.0 \
	spotinstance.0 viewqa1.0 viewqa2.0 viewqa3.0 vldb-disks.0 \
	bug-1044.0 20150105.17.57.0 20150105.17.57-00.0 cgroups-units.0 \
	foo+.0 \
	badlen-0.0 badlen-1.0 badlen-2.0 badlen-3.0 badlen-4.0 \
	badlen-5.0 badlen-6.0 badlen-7.0 badlen-8.0 badlen-9.0 \
	badlen-10.0 badlen-11.0 \
	badti-1.0 badti-2.0 badti-3.0 badti-4.0 \
	badlabel-0.0 badlabel-1.0 badlabel-2.0 \
	badlog-1.0 badlog-2.0 badlog-3.0
SRCEXTRAS = $(subst .0,.meta,$(SRCARCH)) $(subst .0,.index,$(SRCARCH))
SCRIPTS = mk.diff.sh mk.gap.sh mk.mysql.sh mk.schizo.sh mk.foo+.sh \
	mkbadlen mkbadti mkbadlabel mkbadlog
OTHERS = GNUmakefile.install ds390-access.gz

default default_pcp setup:

install install_pcp:
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SCRIPTS) $(TESTDIR)
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
	$(INSTALL) -m 644 $(OTHERS) $(SRCARCH) $(SRCEXTRAS) $(TESTDIR)

include $(BUILDRULES)
