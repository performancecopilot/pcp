#
# Copyright (c) 2020-2021 Red Hat.
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

COPYRIGHT = "(C) 2004-2019 Hisham Muhammad. (C) 2020-2021 htop dev team."

CMDTARGET = pcp-htop
DISTARGET = $(PCP_BINADM_DIR)/$(CMDTARGET)
LLDLIBS = $(PCPLIB) $(LIB_FOR_NCURSESW) $(LIB_FOR_MATH)
NCURSES_CFLAGS = -DHAVE_LIBNCURSESW=$(HAVE_NCURSESW)
LCFLAGS = $(NCURSES_CFLAGS) $(C99_CFLAGS) -Ipcp -I.
LCFLAGS += -DVERSION=\"$(PCP_VERSION)\" -DSYSCONFDIR=\"$(PCP_SYSCONF_DIR)\"
LCFLAGS += -DCOPYRIGHT=\"$(COPYRIGHT)\" -DPACKAGE=\"$(CMDTARGET)\"
LCFLAGS += -DMAJOR_IN_SYSMACROS=1

CFILES = \
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
	EnvScreen.c \
	FunctionBar.c \
	Hashtable.c \
	Header.c \
	HostnameMeter.c \
	IncSet.c \
	InfoScreen.c \
	ListItem.c \
	LoadAverageMeter.c \
	MainPanel.c \
	MemoryMeter.c \
	Meter.c \
	MetersPanel.c \
	NetworkIOMeter.c \
	Object.c \
	OpenFilesScreen.c \
	OptionItem.c \
	Panel.c \
	Process.c \
	ProcessList.c \
	ProcessLocksScreen.c \
	RichString.c \
	ScreenManager.c \
	Settings.c \
	SignalsPanel.c \
	SwapMeter.c \
	SysArchMeter.c \
	TasksMeter.c \
	TraceScreen.c \
	UptimeMeter.c \
	UsersTable.c \
	Vector.c \
	XUtils.c \
	linux/PressureStallMeter.c \
	linux/ZramMeter.c \
	zfs/ZfsArcMeter.c \
	zfs/ZfsCompressedArcMeter.c \
	pcp/PCPProcess.c \
	pcp/PCPProcessList.c \
	pcp/Platform.c \
	pcp-htop.c \

HFILES = \
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
	EnvScreen.h \
	FunctionBar.h \
	Hashtable.h \
	Header.h \
	HostnameMeter.h \
	IncSet.h \
	InfoScreen.h \
	ListItem.h \
	LoadAverageMeter.h \
	Macros.h \
	MainPanel.h \
	MemoryMeter.h \
	Meter.h \
	MetersPanel.h \
	NetworkIOMeter.h \
	Object.h \
	OpenFilesScreen.h \
	OptionItem.h \
	Panel.h \
	Process.h \
	ProcessList.h \
	ProcessLocksScreen.h \
	ProvideCurses.h \
	RichString.h \
	ScreenManager.h \
	Settings.h \
	SignalsPanel.h \
	SwapMeter.h \
	SysArchMeter.h \
	TasksMeter.h \
	TraceScreen.h \
	UptimeMeter.h \
	UsersTable.h \
	Vector.h \
	XUtils.h \
	linux/PressureStallMeter.h \
	linux/ZramMeter.h \
	linux/ZramStats.h \
	zfs/ZfsArcMeter.h \
	zfs/ZfsArcStats.h \
	zfs/ZfsCompressedArcMeter.h \
	pcp/PCPProcess.h \
	pcp/PCPProcessList.h \
	pcp/Platform.h \
	pcp/ProcessField.h \

MAN_PAGES = $(CMDTARGET).1

default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_NCURSESW)" "true"
build-me: $(CMDTARGET) $(DISTLINKS)

install: default
	$(INSTALL) -m 755 $(CMDTARGET) $(DISTARGET)
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp: default

install_pcp: install

$(DISTLINKS):	$(CMDTARGET)
	@$(LN_S) -f $(CMDTARGET) $@

check::	$(CFILES) $(HFILES)
	$(CLINT) $^

check::	$(MAN_PAGES)
	$(MANLINT) $^
