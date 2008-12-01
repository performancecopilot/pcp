/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#ifdef IS_MINGW
/*
 * For native Win32 console tools, we need to translate the paths
 * used in scripts to native paths with PCP_DIR prefix prepended.
 */
static char *
rewrite_path(val)
{
    char *p = rindex(val, '_');

    if (p && (strcmp(p, "_PATH") == 0 || strcmp(p, "_DIR") == 0)) {
	for (p = val; *p; p++)
	    if (*p == '/') *p = '\\';
	return val;
    }
    return NULL;
}
#else
#define rewrite_path(path)	(0)
#endif

char *
pmGetConfig(const char *name)
{
    static int first = 1;
    static char *empty = "";
    char *val;

    if (first) {
	/*
	 * Scan ${PCP_CONF-$PCP_DIR/etc/pcp.conf} and put all PCP config
	 * variables found therein into the environment.
	 */
	FILE *fp;
	char dir[MAXPATHLEN];
	char var[MAXPATHLEN];
	char *prefix = getenv("PCP_DIR");
	char *conf;
	char *p;

	if ((conf = getenv("PCP_CONF")) == NULL) {
	    if (prefix == NULL)
		conf = "/etc/pcp.conf";
	    else {
		snprintf(dir, sizeof(dir), "%s/etc/pcp.conf", prefix);
		conf = dir;
	    }
	}

	if (access((const char *)conf, R_OK) < 0 ||
	   (fp = fopen(conf, "r")) == (FILE *)NULL) {
	    pmprintf("FATAL PCP ERROR: could not open config file \"%s\" : %s\n", conf, strerror(errno));
	    pmprintf("You may need to set PCP_CONF or PCP_DIR in your environment.\n");
	    pmflush();
	    exit(1);
	}

	while (fgets(var, sizeof(var), fp) != NULL) {
	    if (var[0] == '#' || (p = strchr(var, '=')) == NULL)
		continue;
	    *p = '\0';
	    val = p+1;
	    if ((p = strrchr(val, '\n')) != NULL)
		*p = '\0';
	    if ((p = getenv(var)) != NULL)
		val = p;
	    else {
		char envbuf[MAXPATHLEN];

		if (prefix && rewrite_path(val))
		    snprintf(envbuf, sizeof(envbuf), "%s%s=%s", prefix,var,val);
		else
		    snprintf(envbuf, sizeof(envbuf), "%s=%s", var, val);
		putenv(strdup(envbuf));
	    }

	    if (pmDebug & DBG_TRACE_CONFIG)
		fprintf(stderr, "pmGetConfig: (init) %s=%s\n", var, val);
	}
	fclose(fp);
	first = 0;
    }

    if ((val = getenv(name)) == NULL) {
	pmprintf("Error: \"%s\" is not set in the environment\n", name);
	val = empty;
    }

    if (pmDebug & DBG_TRACE_CONFIG)
	fprintf(stderr, "pmGetConfig: %s=%s\n", name, val);

    return val;
}
