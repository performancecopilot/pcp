#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <pmapi.h>
#include <impl.h>
#include <import.h>

MODULE = PCP::LogImport              PACKAGE = PCP::LogImport

# helper methods
#

# name here is a little odd ... follows impl.h definition rather
# than pmi* naming so calls from C and Perl are the same
pmID
pmid_build(domain, cluster, item)
	unsigned int	domain;
	unsigned int	cluster;
	unsigned int	item;
    CODE:
	pmID id;
	__pmid_int(&id)->flag = 0;
	__pmid_int(&id)->domain = domain;
	__pmid_int(&id)->cluster = cluster;
	__pmid_int(&id)->item = item;
	RETVAL = id;
    OUTPUT:
	RETVAL

# name here is a little odd ... follows impl.h definition rather
# than pmi* naming so calls from C and Perl are the same
pmInDom
pmInDom_build(domain, serial)
	unsigned int	domain;
	unsigned int	serial;
    CODE:
	pmInDom indom;
	__pmindom_int(&indom)->flag = 0;
	__pmindom_int(&indom)->domain = domain;
	__pmindom_int(&indom)->serial = serial;
	RETVAL = indom;
    OUTPUT:
	RETVAL

# libpcp_import wrappers
#

void
pmiDump()

pmUnits
pmiUnits(dimSpace, dimTime, dimCount, scaleSpace, scaleTime, scaleCount)
	int	dimSpace;
	int	dimTime;
	int	dimCount;
	int	scaleSpace;
	int	scaleTime;
	int	scaleCount;

pmID
pmiID(domain, cluster, item)
	int	domain;
	int	cluster;
	int	item;

pmInDom
pmiInDom(domain, serial)
	int	domain;
	int	serial;

const char *
pmiErrStr(sts)
	int	sts;

int
pmiStart(archive, inherit)
	char	*archive;
	int	inherit;

int
pmiUseContext(context)
	int	context;

int
pmiEnd()

int
pmiSetHostname(value)
	char	*value;

int
pmiSetTimezone(value)
	char	*value;

int
pmiAddMetric(name, pmid, type, indom, sem, units)
	const char	*name;
	pmID		pmid;
	int		type;
	pmInDom		indom;
	int		sem;
	pmUnits		units;

int
pmiAddInstance(indom, instance, inst)
	pmInDom		indom;
	const char	*instance;
	int		inst;

int
pmiPutValue(name, instance, value)
	const char	*name;
	const char	*instance;
	const char	*value;

int
pmiGetHandle(name, instance)
	const char	*name;
	const char	*instance;

int
pmiPutValueHandle(handle, value)
	int		handle;
	const char	*value;

int
pmiWrite(sec, usec)
	int		sec;
	int		usec;
