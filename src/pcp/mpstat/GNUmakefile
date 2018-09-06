TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TARGET = pcp-mpstat
SCRIPT = $(TARGET).py
MAN_SECTION = 1
MAN_PAGES = $(TARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: $(SCRIPT) $(MAN_PAGES)

include $(BUILDRULES)

install: default
ifeq "$(HAVE_PYTHON)" "true"
	$(INSTALL) -m 755 $(SCRIPT) $(PCP_BINADM_DIR)/$(TARGET)
	@$(INSTALL_MAN)
endif

default_pcp : default

install_pcp : install

ifeq "$(HAVE_PYTHON)" "true"
check:: $(SCRIPT)
	pmpython -m unittest discover -s test -p '*_test.py'
	#$(PYLINT) $^
endif
