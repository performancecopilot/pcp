/*
 * Copyright (c) 2019 Red Hat.
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
 */
#include "pmwebapi.h"
#include "libpcp.h"
#include "util.h"
#include <inih/ini.h>
#include <ctype.h>

int
pmIniFileParse(const char *progname, ini_handler handler, void *data)
{
    char	*dirname;
    char	path[MAXPATHLEN];
    int		sts, sep = pmPathSeparator();

    if (progname == NULL) {
	progname = pmGetProgname();
    } else if (__pmAbsolutePath((char *)progname)) {
	/* use user-supplied path in preferance to all else */
	if ((sts = ini_parse(progname, handler, data)) == -2)
	    return -ENOMEM;
	return 0;
    }

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

char *
construct_canonic_envvar_name(const char *group, const char *key)
{
    int i;
    size_t section_length = strlen(group);
    size_t option_length = strlen(key);
    char section[section_length+1];
    char option[option_length+1];
    char *tmp;

    /* Canonical form: "PCP_<SECTION>_<OPTION>\0" */
    int envvar_name_len = 4 + section_length + 1 + option_length + 1;
    char *envvar_name = malloc((size_t)envvar_name_len*sizeof(char));
    if (!envvar_name)
	return NULL;

    for (i = 0; i < section_length; i++)
	section[i] = toupper(group[i]);
    section[section_length] = '\0';

    for (i = 0; i < option_length; i++)
	if (key[i] == '.')
		option[i] = '_';
	else
		option[i] = toupper(key[i]);
    option[option_length] = '\0';

    tmp = memcpy(envvar_name, "PCP_", 4) + 4;
    tmp = memcpy(tmp, section, section_length) + section_length;
    tmp = memcpy(tmp, "_", 1) + 1;
    tmp = memcpy(tmp, option, option_length);
    envvar_name[envvar_name_len - 1] = '\0';

    return envvar_name;
}

const char *
check_envvar_override(const char *group, const char *key, const char *value)
{
    char *tmp_value;
    char *envvar_name = construct_canonic_envvar_name(group, key);
    if (!envvar_name)
	return value;

    if ((tmp_value = getenv(envvar_name)) != NULL)
	value = tmp_value;

    free(envvar_name);
    return value;
}

static int
dict_handler(void *arg, const char *group, const char *key, const char *value)
{
    dict	*config = (dict *)arg;
    sds		name = sdsempty();

    if (!group)
	group = pmGetProgname();

    value = check_envvar_override(group, key, value);
    name = sdscatfmt(name, "%s.%s", group, key);
    if (pmDebugOptions.libweb)
	fprintf(stderr, "pmIniFileParse set %s = %s\n", name, value);
    return dictReplace(config, name, sdsnew(value)) != DICT_OK;
}

dict *
pmIniFileSetup(const char *progname)
{
    dict	*config;

    if ((config = dictCreate(&sdsOwnDictCallBacks, "pmIniFileSetup")) == NULL)
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
