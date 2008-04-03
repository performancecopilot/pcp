TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = src archives views

COMMONS = common.cmd common.filter common.sh common.test
SCRIPTS = check new remake
INTERACT = $(shell echo *-testcases)
AUTOTEST = $(shell echo [0-9][0-9][0-9])
OUTFILES = $(shell echo *.out)
LSRCFILES = $(COMMONS) $(SCRIPTS) $(INTERACT) $(AUTOTEST) $(OUTFILES)

LDIRT = check.log check.time *.out.bad

default install : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
