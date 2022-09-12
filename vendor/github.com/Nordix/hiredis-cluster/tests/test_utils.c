#include "hircluster.h"

#include "test_utils.h"
#include <stdlib.h>
#include <string.h>

static int redis_version_major;
static int redis_version_minor;

/* Helper to extract Redis version information. */
#define REDIS_VERSION_FIELD "redis_version:"
void load_redis_version(redisClusterContext *cc) {
    nodeIterator ni;
    cluster_node *node;
    char *eptr, *s, *e;
    redisReply *reply = NULL;

    initNodeIterator(&ni, cc);
    if ((node = nodeNext(&ni)) == NULL)
        goto abort;

    reply = redisClusterCommandToNode(cc, node, "INFO");
    if (reply == NULL || cc->err || reply->type != REDIS_REPLY_STRING)
        goto abort;
    if ((s = strstr(reply->str, REDIS_VERSION_FIELD)) == NULL)
        goto abort;

    s += strlen(REDIS_VERSION_FIELD);

    /* We need a field terminator and at least 'x.y.z' (5) bytes of data */
    if ((e = strstr(s, "\r\n")) == NULL || (e - s) < 5)
        goto abort;

    /* Extract version info */
    redis_version_major = strtol(s, &eptr, 10);
    if (*eptr != '.')
        goto abort;
    redis_version_minor = strtol(eptr + 1, NULL, 10);

    freeReplyObject(reply);
    return;

abort:
    freeReplyObject(reply);
    fprintf(stderr, "Error: Cannot get Redis version, aborting..\n");
    exit(1);
}

/* Helper to verify Redis version information. */
int redis_version_less_than(int major, int minor) {
    if (redis_version_major == 0) {
        fprintf(stderr, "Error: Redis version not loaded, aborting..\n");
        exit(1);
    }

    if (redis_version_major < major)
        return 1;
    if (redis_version_major == major && redis_version_minor < minor)
        return 1;
    return 0;
}
