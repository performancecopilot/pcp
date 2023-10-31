#
# Copyright (c) 2020-2023 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

COPYRIGHT = "(C) 2004-2019 Hisham Muhammad. (C) 2020-2023 htop dev team."

CMDTARGET = pcp-htop
DISTARGET = $(PCP_BINADM_DIR)/$(CMDTARGET)
LLDLIBS = $(PCPLIB) $(LIB_FOR_NCURSESW) $(LIB_FOR_MATH)
NCURSES_CFLAGS = -DHAVE_LIBNCURSESW=$(HAVE_NCURSESW)
LCFLAGS = $(NCURSES_CFLAGS) $(C99_CFLAGS) -Ipcp -I.
LCFLAGS += -DVERSION=\"$(PCP_VERSION)\" -DSYSCONFDIR=\"$(PCP_SYSCONF_DIR)\"
LCFLAGS += -DCOPYRIGHT=\"$(COPYRIGHT)\" -DPACKAGE=\"$(CMDTARGET)\"
LCFLAGS += -D_XOPEN_SOURCE_EXTENDED -DMAJOR_IN_SYSMACROS=1 -DNDEBUG

TOPCFILES = \
	pcp-htop.c \
	Action.c \
	AvailableColumnsPanel.c \
	AvailableMetersPanel.c \
	BatteryMeter.c \
	CategoriesPanel.c \
	ClockMeter.c \
	ColorsPanel.c \
	ColumnsPanel.c \
	CommandLine.c \
	CommandScreen.c \
	Compat.c \
	CPUMeter.c \
	CRT.c \
	DateMeter.c \
	DateTimeMeter.c \
	DiskIOMeter.c \
	DisplayOptionsPanel.c \
	DynamicColumn.c \
	DynamicMeter.c \
	DynamicScreen.c \
	EnvScreen.c \
	FileDescriptorMeter.c \
	FunctionBar.c \
	Hashtable.c \
	Header.c \
	HeaderOptionsPanel.c \
	HostnameMeter.c \
	IncSet.c \
	InfoScreen.c \
	ListItem.c \
	LoadAverageMeter.c \
	Machine.c \
	MainPanel.c \
	MemoryMeter.c \
	MemorySwapMeter.c \
	Meter.c \
	MetersPanel.c \
	NetworkIOMeter.c \
	Object.c \
	OpenFilesScreen.c \
	OptionItem.c \
	Panel.c \
	Process.c \
	ProcessLocksScreen.c \
	ProcessTable.c \
	RichString.c \
	Row.c \
	Scheduling.c \
	ScreenManager.c \
	ScreensPanel.c \
	ScreenTabsPanel.c \
	Settings.c \
	SignalsPanel.c \
	SwapMeter.c \
	SysArchMeter.c \
	Table.c \
	TasksMeter.c \
	TraceScreen.c \
	UptimeMeter.c \
	UsersTable.c \
	Vector.c \
	XUtils.c \

SUBCFILES = \
	linux/CGroupUtils.c \
	linux/PressureStallMeter.c \
	linux/ZramMeter.c \
	zfs/ZfsArcMeter.c \
	zfs/ZfsCompressedArcMeter.c \
	pcp/InDomTable.c \
	pcp/Instance.c \
	pcp/Metric.c \
	pcp/PCPDynamicColumn.c \
	pcp/PCPDynamicMeter.c \
	pcp/PCPDynamicScreen.c \
	pcp/PCPMachine.c \
	pcp/PCPProcess.c \
	pcp/PCPProcessTable.c \
	pcp/Platform.c \

TOPHFILES = \
	Action.h \
	AvailableColumnsPanel.h \
	AvailableMetersPanel.h \
	BatteryMeter.h \
	CategoriesPanel.h \
	ClockMeter.h \
	ColorsPanel.h \
	ColumnsPanel.h \
	CommandLine.h \
	CommandScreen.h \
	Compat.h \
	CPUMeter.h \
	CRT.h \
	DateMeter.h \
	DateTimeMeter.h \
	DiskIOMeter.h \
	DisplayOptionsPanel.h \
	DynamicColumn.h \
	DynamicMeter.h \
	DynamicScreen.h \
	EnvScreen.h \
	FileDescriptorMeter.h \
	FunctionBar.h \
	Hashtable.h \
	Header.h \
	HeaderLayout.h \
	HeaderOptionsPanel.h \
	HostnameMeter.h \
	IncSet.h \
	InfoScreen.h \
	ListItem.h \
	LoadAverageMeter.h \
	Machine.h \
	Macros.h \
	MainPanel.h \
	MemoryMeter.h \
	MemorySwapMeter.h \
	Meter.h \
	MetersPanel.h \
	NetworkIOMeter.h \
	Object.h \
	OpenFilesScreen.h \
	OptionItem.h \
	Panel.h \
	Process.h \
	ProcessLocksScreen.h \
	ProcessTable.h \
	ProvideCurses.h \
	ProvideTerm.h \
	RichString.h \
	Row.h \
	RowField.h \
	Scheduling.h \
	ScreenManager.h \
	ScreensPanel.h \
	ScreenTabsPanel.h \
	Settings.h \
	SignalsPanel.h \
	SwapMeter.h \
	SysArchMeter.h \
	Table.h \
	TasksMeter.h \
	TraceScreen.h \
	UptimeMeter.h \
	UsersTable.h \
	Vector.h \
	XUtils.h \

SUBHFILES = \
	linux/CGroupUtils.h \
	linux/PressureStallMeter.h \
	linux/ZramMeter.h \
	linux/ZramStats.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/ZfsCompressedArcMeter.h \
	pcp/InDomTable.h \
	pcp/Instance.h \
	pcp/Metric.h \
	pcp/PCPDynamicColumn.h \
	pcp/PCPDynamicMeter.h \
	pcp/PCPDynamicScreen.h \
	pcp/PCPMachine.h \
	pcp/PCPProcess.h \
	pcp/PCPProcessTable.h \
	pcp/Platform.h \
	pcp/ProcessField.h \

DOCFILES = AUTHORS pcp-htop.5.in
COLUMNS = container delayacct fdcount guest memory sched swap tcp udp wchan
METERS = entropy freespace ipc locks memcache mysql postfix redis tcp
SCREENS = cgroups cgroupsio cgroupsmem devices filesystems \
	  biosnoop execsnoop exitsnoop opensnoop
CFGMETERS = $(patsubst %,pcp/meters/%,$(METERS))
CFGCOLUMNS = $(patsubst %,pcp/columns/%,$(COLUMNS))
CFGSCREENS = $(patsubst %,pcp/screens/%,$(SCREENS))

CFGXFILES = $(CFGMETERS) $(CFGCOLUMNS) $(CFGSCREENS)
TOPXFILES = $(TOPCFILES) $(TOPHFILES) $(DOCFILES)
SUBXFILES = $(SUBCFILES) $(SUBHFILES)
CFILES = $(TOPCFILES) $(SUBCFILES)
HFILES = $(TOPHFILES) $(SUBHFILES)
LDIRT = $(TOPXFILES) $(SUBXFILES) $(CFGXFILES) \
	htop.1.in $(CMDTARGET).1.in $(CMDTARGET).1 $(CMDTARGET).5

MAN_PAGES = $(CMDTARGET).1 $(CMDTARGET).5

default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_NCURSESW)" "true"
build-me: $(TOPXFILES) $(SUBXFILES) $(CFGXFILES) $(CMDTARGET) $(DISTLINKS) $(MAN_PAGES)

install: default
	$(INSTALL) -m 755 $(CMDTARGET) $(DISTARGET)
	$(INSTALL_MAN)
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/htop
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/htop/meters
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/htop/columns
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/htop/screens
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/htop
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/htop/meters
	@for m in $(METERS); do \
	    $(INSTALL) -m 644 pcp/meters/$$m $(PCP_SHARE_DIR)/htop/meters/$$m; \
	done
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/htop/columns
	@for c in $(COLUMNS); do \
	    $(INSTALL) -m 644 pcp/columns/$$c $(PCP_SHARE_DIR)/htop/columns/$$c; \
	done
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/htop/screens
	@for c in $(SCREENS); do \
	    $(INSTALL) -m 644 pcp/screens/$$c $(PCP_SHARE_DIR)/htop/screens/$$c; \
	done
else
build-me:
install:
endif

default_pcp: default

install_pcp: install

$(TOPXFILES):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/htop-dev/htop/$@ $@

$(SUBXFILES):
	mkdir -p linux pcp zfs
	$(LN_S) -f $(TOPDIR)/../vendor/github.com/htop-dev/htop/$@ $@

$(CFGXFILES):
	mkdir -p pcp/columns pcp/meters pcp/screens
	$(LN_S) -f $(TOPDIR)/../../vendor/github.com/htop-dev/htop/$@ $@

$(DISTLINKS):	$(CMDTARGET)
	@$(LN_S) -f $(CMDTARGET) $@

$(CMDTARGET).1.in:
	$(LN_S) -f $(TOPDIR)/vendor/github.com/htop-dev/htop/htop.1.in $@

$(MAN_PAGES): $(CMDTARGET).1.in $(CMDTARGET).5.in
	for s in 1 5; do \
	    $(SED) < $(CMDTARGET).$$s.in > $(CMDTARGET).$$s \
		-e 's;@PACKAGE_STRING@;Performance Co-Pilot;g' \
		-e 's;^htop, pcp-htop;\\f3pcp-htop\\f1;g' \
		-e 's;@sysconfdir@;$(PCP_SYSCONF_DIR);g' \
		-e 's;@datarootdir@;$(PCP_SHARE_DIR);g' \
		-e 's;^\.SH "NAME";.SH NAME;g' \
		-e 's;pcp/pcp;pcp;g'; \
	done

check::	$(CFILES) $(HFILES)
	$(CLINT) $^

check::	$(MAN_PAGES)
	$(MANLINT) $^
