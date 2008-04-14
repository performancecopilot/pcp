TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SCRIPTS = kmsnap.sh
CONFIGS = crontab.IN Snap control
HTMLDOC = summary.html
LSRCFILES = $(SCRIPTS) $(CONFIGS) $(HTMLDOC)
LDIRT = crontab

default: crontab

crontab : crontab.IN
	$(SED) -e '/\# .*/b' -e 's;PCP_BINADM_DIR;$(PCP_BINADM_DIR);g' < $< > $@

include $(BUILDRULES)

ifndef PCP_KMSNAPCONTROL_PATH
PCP_KMSNAPCONTROL_PATH = $(PCP_VAR_DIR)/config/kmsnap/control
endif

install: default
	$(INSTALL) -m 755 -d $(PCP_BINADM_DIR)
	$(INSTALL) -m 755 kmsnap.sh $(PCP_BINADM_DIR)/kmsnap
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/kmsnap
	$(INSTALL) -m 644 Snap crontab summary.html $(PCP_VAR_DIR)/config/kmsnap
	$(INSTALL) -m 755 -d `dirname $(PCP_KMSNAPCONTROL_PATH)`
	$(INSTALL) -m 644 control $(PCP_KMSNAPCONTROL_PATH)
