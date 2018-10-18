TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

TARGET = pcp-mpstat
SCRIPT = $(TARGET).py
MAN_SECTION = 1
MAN_PAGES = $(TARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

SUBDIRS = test

default: build-me

ifeq "$(HAVE_PYTHON)" "true"
build-me:: $(SCRIPT) $(MAN_PAGES)

build-me:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install: default
	$(INSTALL) -m 755 $(SCRIPT) $(PCP_BINADM_DIR)/$(TARGET)
	@$(INSTALL_MAN)

check:: $(SCRIPT)
	#$(PYLINT) $^
	$(SUBDIRS_MAKERULE)
else
build-me:
install:
endif

include $(BUILDRULES)

default_pcp : default

install_pcp : install
