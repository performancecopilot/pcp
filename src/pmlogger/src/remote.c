/*
 * Copyright (c) 2025 Red Hat.
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

#include "logger.h"

#define AJ_STR "application/json"
#define AJ_LEN (sizeof(AJ_STR)-1)
#define AO_STR "application/octet-stream"

static int
remote_http_error(int code, char *buf, size_t buflen)
{
    switch (code) {
    case 400:   /* BAD_REQUEST */
	pmsprintf(buf, buflen, "bad request (%d)", code);
	return code;
    case 409:   /* CONFLICT */
	pmsprintf(buf, buflen, "conflict (%d) - archive already exists", code);
	return code;
    case 410:   /* GONE */
	pmsprintf(buf, buflen, "resource gone (%d) - server timed out", code);
	return code;
    case 422:   /* UNPROCESSABLE_ENTITY */
	pmsprintf(buf, buflen, "unprocessable (%d) - bad archive label", code);
	return code;
    case 500: /* INTERNAL_SERVER_ERROR */
    default:
	if (code >= 200 && code < 300)	/* OK */
	    break;
	pmsprintf(buf, buflen, "server error (%d)", code);
	return code;
    }
    return 0;
}

int
remote_ping(void)
{
    const char		path[] = "/logger/ping";
    char		errorbuf[128];
    int			sts;

    sts = pmhttpClientGet(remote.client, remote.conn, path,
				&remote.body, &remote.body_bytes,
				&remote.type, &remote.type_bytes);
    if (sts < 0) {
	fprintf(stderr, "%s: %s GET from %s failed: %s\n",
		pmGetProgname(), remote.conn, path, pmErrStr(sts));
	return -EINVAL;
    }
    if (remote_http_error(sts, errorbuf, sizeof(errorbuf))) {
	fprintf(stderr, "%s: %s GET %s HTTP error: %s\n",
		pmGetProgname(), remote.conn, path, errorbuf);
	return -EINVAL;
    }
    if (remote.type_bytes != AJ_LEN || strcmp(remote.type, AJ_STR) != 0) {
	fprintf(stderr, "%s: %s unexpected response type: %s (%zu)\n",
		pmGetProgname(), path, remote.type, remote.type_bytes);
	return -EINVAL;
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: ping GET result: %s\n", __FUNCTION__, remote.body);

    if ((strncmp(remote.body, "{\"success\":true}", 16)) != 0) {
	fprintf(stderr, "%s: %s bad result (expected success, got %s)\n",
			pmGetProgname(), path, remote.body);
	return -EINVAL;
    }
    return 0;
}

int
remote_label(const __pmArchCtl *acp, int volume, void *buffer, size_t length,
		const char *caller)
{
    const char		path[] = "/logger/label";
    char		errorbuf[128];
    int			sts;

    sts = pmhttpClientPost(remote.client, remote.conn, path,
				buffer, length, AO_STR,
				&remote.body, &remote.body_bytes,
				&remote.type, &remote.type_bytes);
    if (sts < 0) {
	fprintf(stderr, "%s: %s POST to %s failed: %s\n",
		pmGetProgname(), remote.conn, path, pmErrStr(sts));
	return -EINVAL;
    }
    if (remote_http_error(sts, errorbuf, sizeof(errorbuf))) {
	fprintf(stderr, "%s: HTTP error: %s\n", pmGetProgname(), errorbuf);
	return -EINVAL;
    }
    if (remote.type_bytes != AJ_LEN || strcmp(remote.type, AJ_STR) != 0) {
	fprintf(stderr, "%s: %s unexpected response type: %s (%zu)\n",
		pmGetProgname(), path, remote.type, remote.type_bytes);
	return -EINVAL;
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: label POST result: %s\n",
			__FUNCTION__, remote.body);

    sts = sscanf(remote.body, "{\"archive\":%u,\"success\":true}", &remote.log);
    if (sts != 1) {
	fprintf(stderr, "%s: %s bad result (expected 1 archive, got %d): %s\n",
			pmGetProgname(), path, sts, remote.body);
	return -EINVAL;
    }
    return 0;
}

int
remote_write(const __pmArchCtl *acp, int volume, void *buffer, size_t length,
                const char *caller)
{
    char		errorbuf[128];
    char		path[64];
    int			sts;

    if (volume == PM_LOG_VOL_META)
	pmsprintf(path, sizeof(path), "/logger/meta/%u", remote.log);
    else if (volume == PM_LOG_VOL_TI)
	pmsprintf(path, sizeof(path), "/logger/index/%u", remote.log);
    else
	pmsprintf(path, sizeof(path), "/logger/volume/%d/%u", volume, remote.log);

    sts = pmhttpClientPost(remote.client, remote.conn, path,
				buffer, length, AO_STR,
				&remote.body, &remote.body_bytes,
				&remote.type, &remote.type_bytes);
    if (sts < 0) {
	fprintf(stderr, "%s: %s POST to %s failed: %s\n",
		pmGetProgname(), remote.conn, path, pmErrStr(sts));
	return sts;
    }
    if (remote_http_error(sts, errorbuf, sizeof(errorbuf))) {
	fprintf(stderr, "%s: HTTP error: %s\n", pmGetProgname(), errorbuf);
	return -EINVAL;
    }
    if (remote.type_bytes != AJ_LEN || strcmp(remote.type, AJ_STR) != 0) {
	fprintf(stderr, "%s: %s unexpected response type: %s (%zu)\n",
		pmGetProgname(), path, remote.type, remote.type_bytes);
	return -EINVAL;
    }
    if (remote.body_bytes < 10) {
	fprintf(stderr, "%s: %s unexpected body size\n", pmGetProgname(), path);
	return -EINVAL;
    }
    if (volume == PM_LOG_VOL_META)
	remote.total_meta += remote.body_bytes;
    else if (volume == PM_LOG_VOL_TI)
	remote.total_index += remote.body_bytes;
    else
	remote.total_volume += remote.body_bytes;

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: %s POST success\n", __FUNCTION__, path);

    return 0;
}
