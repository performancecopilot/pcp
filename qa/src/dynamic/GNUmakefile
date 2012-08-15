TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

MYTESTDIR = $(subst pcp,pcp-gui,$(PCP_VAR_DIR))/testsuite/src/dynamic
MYSCRIPTS = Install Remove
PMDAFILES = domain.h GNUmakefile.install pmns help dynamic.c
LSRCFILES = $(MYSCRIPTS) $(PMDAFILES)

default:

install:
	$(INSTALL) -m 755 -d $(MYTESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(MYTESTDIR)
	$(INSTALL) -m 644 $(PMDAFILES) $(MYTESTDIR)

include $(BUILDRULES)
