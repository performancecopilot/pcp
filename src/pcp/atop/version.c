/* No manual changes in this file */
#include <stdio.h>
#include <string.h>
#include "version.h"

static char atopversion[] = ATOPVERS;
static char atopdate[]    = ATOPDATE;

char *
getstrvers(void)
{
	static char vers[256];

	snprintf(vers, sizeof vers,
		"Version: %s - %s     <gerlof.langeveld@atoptool.nl>",
		atopversion, atopdate);

	return vers;
}

unsigned short
getnumvers(void)
{
	int	vers1, vers2;

	sscanf(atopversion, "%u.%u", &vers1, &vers2);

	return (unsigned short) ((vers1 << 8) + vers2);
}
