TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

MYTESTDIR = $(PCP_VAR_DIR)/../pcp-gui/testsuite/src/dynamic
MYSCRIPTS = Install Remove
PMDAFILES = domain.h GNUmakefile.install pmns help pmda.c
LSRCFILES = $(SCRIPTS) $(PMDAFILES)

default:

install:
	$(INSTALL) -m 755 -d $(MYTESTDIR)
	$(INSTALL) -m 755 $(MYSCRIPTS) $(MYTESTDIR)
	$(INSTALL) -m 644 $(PMDAFILES) $(MYTESTDIR)

include $(BUILDRULES)
