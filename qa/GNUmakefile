TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = src archives views interact

COMMONS = common.cmd common.filter common.sh common.test
SCRIPTS = check new remake
AUTOTEST = $(shell echo [0-9][0-9][0-9])
OUTFILES = $(shell echo *.out)
LSRCFILES = $(COMMONS) $(SCRIPTS) $(AUTOTEST) $(OUTFILES)

LDIRT = check.log check.time pmlogger.log *.out.bad *.full

default install : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
