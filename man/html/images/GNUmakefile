TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

PNGFILES = $(shell echo *.png)
LSRCFILES = $(PNGFILES)
ICONLINKS = kmcharticon.png kmtimeicon.png stepfwd_on.png
WINDOWLINKS = \
	kmchart_blank_canvas.png \
	kmchart_open_view.png \
	kmchart_cpu_disk.png \
	kmchart_edit_chart.png \
	kmchart_new_chart.png \
	kmchart_new_chart_select.png \
	kmchart_new_chart_colors.png \
	kmchart_cpu_disk_load.png \
	kmchart_stop_recording.png \
	kmchart_cpu_disk_record.png \
	kmtime_live.png \
	kmtime_archive.png \
	kmtime_clients.png \
	kmtime_bounds.png
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
