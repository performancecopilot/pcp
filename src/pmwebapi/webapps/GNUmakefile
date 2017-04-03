TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

HTML	= index.html
IMAGES	= favicon.ico vector.png \
	  blinkenlights.png grafana1.png grafana2.png graphite.png
LDIRT	= $(IMAGES)

default build-me: $(IMAGES)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(PCP_SHARE_DIR)/webapps
ifneq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 644 $(HTML) $(IMAGES) $(PCP_SHARE_DIR)/webapps
endif

$(IMAGES):
	$(LN_S) $(TOPDIR)/images/$@ $@

default_pcp:	default

install_pcp:	install
