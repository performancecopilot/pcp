TOPDIR = ../../../..
include $(TOPDIR)/src/include/builddefs

SCRIPT = pcp-mpstat.py
MODULE = pcp_mpstat.py

LDIRT = $(MODULE)

default default_pcp build-me install install_pcp:

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
test:
	@rm -f $(MODULE)
	$(LN_S) ../$(SCRIPT) $(MODULE)
	pmpython -m unittest discover -s . -p '*_test.py'
endif
