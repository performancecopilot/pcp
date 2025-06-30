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
	pmNotifyErr(LOG_INFO, "%s: GET %s from %s failed: %s\n",
		__FUNCTION__, path, remote.conn, pmErrStr(sts));
	return -EINVAL;
    }
    if (remote_http_error(sts, errorbuf, sizeof(errorbuf))) {
	pmNotifyErr(LOG_INFO, "%s: GET %s from  %s HTTP error: %s\n",
		__FUNCTION__, path, remote.conn, errorbuf);
	return -EINVAL;
    }
    if (remote.type_bytes != AJ_LEN || strcmp(remote.type, AJ_STR) != 0) {
	pmNotifyErr(LOG_ERR, "%s: GET %s from  %s unexpected response type: %s bytes: %zu\n",
		__FUNCTION__, path, remote.conn, remote.type, remote.type_bytes);
	return -EINVAL;
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: GET %s from %s result: %s\n",
		__FUNCTION__, path, remote.conn, remote.body);

    if ((strncmp(remote.body, "{\"success\":true}", 16)) != 0) {
	pmNotifyErr(LOG_ERR, "%s: GET %s from %s bad result (expected success, got %s)\n",
		__FUNCTION__, path, remote.conn, remote.body);
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
	pmNotifyErr(LOG_INFO, "%s: POST %s to %s failed: %s\n",
		__FUNCTION__, path, remote.conn, pmErrStr(sts));
	return -EINVAL;
    }
    if (remote_http_error(sts, errorbuf, sizeof(errorbuf))) {
	pmNotifyErr(LOG_INFO, "%s: POST %s to %s HTTP error: %s\n",
		__FUNCTION__, path, remote.conn, errorbuf);
	return -EINVAL;
    }
    if (remote.type_bytes != AJ_LEN || strcmp(remote.type, AJ_STR) != 0) {
	pmNotifyErr(LOG_ERR, "%s: POST %s to %s unexpected response type: %s (%zu)\n",
		__FUNCTION__, path, remote.conn, remote.type, remote.type_bytes);
	return -EINVAL;
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: POST %s to %s result: %s\n",
		__FUNCTION__, path, remote.conn, remote.body);

    sts = sscanf(remote.body, "{\"archive\":%u,\"success\":true}", &remote.log);
    if (sts != 1) {
	pmNotifyErr(LOG_ERR, "%s: POST %s to %s bad result (expected 1 archive, got %d): %s\n",
		__FUNCTION__, path, remote.conn, sts, remote.body);
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
	pmNotifyErr(LOG_INFO, "%s: POST %s to %s failed: %s\n",
		__FUNCTION__, path, remote.conn, pmErrStr(sts));
	return sts;
    }
    if (remote_http_error(sts, errorbuf, sizeof(errorbuf))) {
	pmNotifyErr(LOG_INFO, "%s: POST %s to %s HTTP error: %s\n",
		__FUNCTION__, path, remote.conn, errorbuf);
	return -EINVAL;
    }
    if (remote.type_bytes != AJ_LEN || strcmp(remote.type, AJ_STR) != 0) {
	pmNotifyErr(LOG_ERR, "%s: POST %s to %s unexpected response type: %s (%zu)\n",
		__FUNCTION__, path, remote.conn, remote.type, remote.type_bytes);
	return -EINVAL;
    }
    if (remote.body_bytes < 10) {
	pmNotifyErr(LOG_ERR, "%s: POST %s to %s unexpected body size: %zu\n",
		__FUNCTION__, path, remote.conn, remote.body_bytes);
	return -EINVAL;
    }
    if (volume == PM_LOG_VOL_META)
	remote.total_meta += remote.body_bytes;
    else if (volume == PM_LOG_VOL_TI)
	remote.total_index += remote.body_bytes;
    else
	remote.total_volume += remote.body_bytes;

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: POST %s to %s success\n",
		__FUNCTION__, path, remote.conn);

    return 0;
}
