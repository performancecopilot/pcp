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

#include "pmapi.h"
#include "sds.h"
#include "ini.h"

int pmIniFileParse(sds tool, ini_handler handler, void* user)
{
    int sts;
    sds cwd_config = sdscatprintf(sdsempty(), "./%s.conf", tool);
    sds homedir_config = sdscatprintf(sdsempty(), "%s/.%s.conf", getenv("HOME"), tool); /* threadsafe */
    sds homedir_pcp_config = sdscatprintf(sdsempty(), "%s/.pcp/%s.conf", getenv("HOME"), tool); /* threadsafe */
    sds sysconf_dir_config = sdscatprintf(sdsempty(), "%s/%s/%s.conf", pmGetConfig("PCP_SYSCONF_DIR"), tool, tool);

    sts = ini_parse(sysconf_dir_config, handler, user);
    if (sts >= 0)
	sts = ini_parse(homedir_pcp_config, handler, user);
    if (sts >= 0)
	sts = ini_parse(homedir_config, handler, user);
    if (sts >= 0)
	sts = ini_parse(cwd_config, handler, user);

    sdsfree(cwd_config);
    sdsfree(homedir_config);
    sdsfree(homedir_pcp_config);
    sdsfree(sysconf_dir_config);

    return sts;
}
