TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

HTML	= index.html
IMAGES	= favicon.ico vector.png \
	  blinkenlights.png grafana1.png grafana2.png graphite.png
LDIRT	= $(IMAGES)

default: build-me $(IMAGES)

ifeq "$(HAVE_WEBAPPS)" "true"
build-me:	$(SUBDIRS)
	$(SUBDIRS_MAKERULE)
install:	default
	$(INSTALL) -m 755 $(PCP_SHARE_DIR)/webapps
ifneq ($(PACKAGE_DISTRIBUTION), debian)
	$(INSTALL) -m 644 $(HTML) $(IMAGES) $(PCP_SHARE_DIR)/webapps
endif
else
build-me:
install:
endif

include $(BUILDRULES)

$(IMAGES):
	$(LN_S) $(TOPDIR)/images/$@ $@

default_pcp:	default

install_pcp:	install
