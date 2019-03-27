/*
 * Copyright (c) 2019 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmwebapi.h"
#include "util.h"
#include "ini.h"

int
pmIniFileParse(const char *progname, ini_handler handler, void *data)
{
    char	*dirname;
    char	path[MAXPATHLEN];
    int		sts, sep = pmPathSeparator();

    if (progname == NULL)
	progname = pmGetProgname();

    if ((dirname = pmGetOptionalConfig("PCP_SYSCONF_DIR")) != NULL) {
	pmsprintf(path, sizeof(path), "%s%c%s%c%s.conf", dirname, sep,
			progname, sep, progname);
	if ((sts = ini_parse(path, handler, data)) == -2)
	    return -ENOMEM;
    }
    if ((dirname = getenv("HOME")) != NULL) {
	pmsprintf(path, sizeof(path), "%s%c.%s.conf", dirname, sep, progname);
	if ((sts = ini_parse(path, handler, data)) == -2)
	    return -ENOMEM;
	pmsprintf(path, sizeof(path), "%s%c.pcp%c%s.conf", dirname, sep, sep, progname);
	if ((sts = ini_parse(path, handler, data)) == -2)
	    return -ENOMEM;
    }
    pmsprintf(path, sizeof(path), ".%c/%s.conf", sep, progname);
    if ((sts = ini_parse(path, handler, data)) == -2)
	return -ENOMEM;

    return 0;
}

static int
dict_handler(void *arg, const char *group, const char *key, const char *value)
{
    dict	*config = (dict *)arg;
    sds		name = sdsempty();

    name = sdscatfmt(name, "%s.%s", group ? group : pmGetProgname(), key);
    if (pmDebugOptions.libweb)
	fprintf(stderr, "pmIniFileParse set %s = %s\n", name, value);
    return dictReplace(config, name, sdsnew(value)) != DICT_OK;
}

dict *
pmIniFileSetup(const char *progname)
{
    dict	*config;

    if ((config = dictCreate(&sdsDictCallBacks, "pmIniFileSetup")) == NULL)
	return NULL;
    if (pmIniFileParse(progname, dict_handler, config) == 0)
	return config;
    dictRelease(config);
    return NULL;
}

void
pmIniFileUpdate(dict *config, const char *group, const char *key, sds value)
{
    sds		name = sdsempty();

    name = sdscatfmt(name, "%s.%s", group ? group : pmGetProgname(), key);
    if (pmDebugOptions.libweb)
	fprintf(stderr, "pmIniFileUpdate set %s = %s\n", name, value);
    dictReplace(config, name, value);
    sdsfree(name);
}

sds
pmIniFileLookup(dict *config, const char *group, const char *key)
{
    dictEntry	*entry;
    sds		name = sdsempty();

    name = sdscatfmt(name, "%s.%s", group ? group : pmGetProgname(), key);
    entry = dictFind(config, name);
    sdsfree(name);
    if (entry)
	return (sds)dictGetVal(entry);
    return NULL;
}

void
pmIniFileFree(dict *config)
{
    dictRelease(config);
}
