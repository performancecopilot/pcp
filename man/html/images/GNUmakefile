TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

PNGFILES = $(shell echo *.png)
LSRCFILES = $(PNGFILES)
ICONLINKS = pmcharticon.png pmtimeicon.png stepfwd_on.png
WINDOWLINKS = \
	pmchart_blank_canvas.png \
	pmchart_open_view.png \
	pmchart_cpu_disk.png \
	pmchart_edit_chart.png \
	pmchart_new_chart.png \
	pmchart_new_chart_select.png \
	pmchart_new_chart_colors.png \
	pmchart_cpu_disk_load.png \
	pmchart_stop_recording.png \
	pmchart_cpu_disk_record.png \
	pmtime_live.png \
	pmtime_archive.png \
	pmtime_clients.png \
	pmtime_bounds.png
INSTFILES = $(ICONLINKS) $(WINDOWLINKS)
LDIRT = $(ICONLINKS) $(WINDOWLINKS)

# TODO: generate images for Win32 and rejig LN_S use here
ifneq ($(PKG_PLATFORM),mingw)
default: iconlinks windowlinks
else
default:
	touch $(INSTFILES)
endif

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)/html/images
	$(INSTALL) -m 644 $(INSTFILES) $(PKG_DOC_DIR)/html/images
