#
# Copyright (c) 2013-2014 Red Hat.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

MAN_SECTION	= 3

MAN_PAGES	= \
	pcpintro.3 pmaddprofile.3 pmapi.3 pmatomstr.3 \
	pmconvscale.3 pmctime.3 pmda.3 pmdaconnect.3 pmdadaemon.3 \
	pmdadesc.3 pmdadso.3 pmdafetch.3 pmdagetopt.3 pmdainit.3 \
	pmdainstance.3 pmdamain.3 pmdaopenlog.3 pmdaprofile.3 \
	pmdastore.3 pmdatext.3 pmdatrace.3 pmdelprofile.3 \
	pmdestroycontext.3 pmdiscoverservices.3 pmdupcontext.3 pmerrstr.3 \
	pmextractvalue.3 \
	pmfetch.3 pmfetcharchive.3 pmfreeresult.3 pmgetarchiveend.3 \
	pmgetarchivelabel.3 pmgetchildren.3 pmgetchildrenstatus.3 \
	pmgetconfig.3 pmgetcontexthostname.3 pmgetindom.3 \
	pmgetindomarchive.3 pmgetpmnslocation.3 pmidstr.3 pmindomstr.3 \
	pmloadasciinamespace.3 pmloadnamespace.3 pmlocaltime.3 \
	pmlookupdesc.3 pmlookupindom.3 pmlookupindomarchive.3 \
	pmlookupindomtext.3 pmlookupname.3 pmlookuptext.3 pmnameall.3 \
	pmnameid.3 pmnameindom.3 pmnameindomarchive.3 pmnewcontext.3 \
	pmnewcontextzone.3 pmnewzone.3 pmnumberstr.3 pmparseinterval.3 \
	pmparsemetricspec.3 pmparsetimewindow.3 pmprintf.3 \
	pmprintvalue.3 pmreconnectcontext.3 pmsetmode.3 \
	pmsortinstances.3 pmstore.3 pmtraversepmns.3 \
	pmtrimnamespace.3 pmtypestr.3 pmunitsstr.3 pmunloadnamespace.3 \
	pmusecontext.3 pmusezone.3 pmwhichcontext.3 pmwhichzone.3 \
	pmdacache.3 pmdahelp.3 pmdaattribute.3 \
	pmafm.3 pmtime.3 \
	pmdaname.3 pmdachildren.3 pmdapmid.3 \
	mmv_inc_value.3 mmv_lookup_value_desc.3 mmv_stats_init.3 \
	pmloadderivedconfig.3 pmregisterderived.3 pmderivederrstr.3 \
	pmparsehostspec.3 pmlocalpmda.3 pmspeclocalpmda.3 \
	logimport.3 pmistart.3 pmisethostname.3 pmisettimezone.3 \
	pmiusecontext.3 pmiunits.3 pmigethandle.3 pmiaddmetric.3 \
	pmiaddinstance.3 pmiend.3 pmierrstr.3 \
	pmiputresult.3 pmiputvalue.3 pmiputvaluehandle.3 pmiwrite.3 \
	pmunpackeventrecords.3 pmfreeeventresult.3 pmeventflagsstr.3 \
	pmdaeventarray.3 pmdaeventclient.3 pmdaeventqueue.3 \
	pmconverttime.3 pmmktime.3 pmparsectime.3 pmfreeprofile.3 \
	pmlookupipc.3 pmopenlog.3 pmparsedebug.3 pmparsetime.3 pmaf.3 \
	pmconnectlogger.3 pmcontrollog.3 pmparsehostattrsspec.3 \
	pmwebapi.3

MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)
LSRCFILES	= $(MAN_PAGES)

default :: $(MAN_PAGES)

default_pcp : $(MAN_PAGES)

include $(BUILDRULES)

install :: install_pcp

install_pcp : default
	$(INSTALL_MAN)


