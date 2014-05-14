TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SCRIPTS = pmsnap.sh
CONFIGS = crontab.IN Snap control
HTMLDOC = summary.html
LSRCFILES = $(SCRIPTS) $(CONFIGS) $(HTMLDOC)
LDIRT = crontab

default: crontab

crontab : crontab.IN
	$(SED) -e '/\# .*/b' -e 's;PCP_BINADM_DIR;$(PCP_BINADM_DIR);g' < $< > $@

include $(BUILDRULES)

ifndef PCP_PMSNAPCONTROL_PATH
PCP_PMSNAPCONTROL_PATH = $(PCP_VAR_DIR)/config/pmsnap/control
endif

install: default
	$(INSTALL) -m 755 -d $(PCP_BINADM_DIR)
	$(INSTALL) -m 755 pmsnap.sh $(PCP_BINADM_DIR)/pmsnap
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmsnap
	$(INSTALL) -m 644 Snap crontab summary.html $(PCP_VAR_DIR)/config/pmsnap
	$(INSTALL) -m 755 -d `dirname $(PCP_PMSNAPCONTROL_PATH)`
	$(INSTALL) -m 644 control $(PCP_PMSNAPCONTROL_PATH)

default_pcp:	default

install_pcp:	install
