#
# Copyright (c) 2003-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/src/include/builddefs

LSRCFILES = \
	pkg.m4 \
	manual_format.m4 \
	package_globals.m4 \
	package_utilies.m4 \
	package_pthread.m4 \
	package_qtdev.m4 \
	multilib.m4

default:

include $(BUILDRULES)

install install-dev install-lib: default
