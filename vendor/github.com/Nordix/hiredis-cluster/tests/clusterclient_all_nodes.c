/*
 * This program connects to a cluster and then reads commands from stdin, such
 * as "DBSIZE", one per line and sends each command to every master node in
 * the cluster, and prints the results to stdout.
 */

#include "hircluster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc <= 1) {
        fprintf(stderr, "Usage: clusterclient HOST:PORT\n");
        exit(1);
    }
    const char *initnode = argv[1];

    struct timeval timeout = {1, 500000}; // 1.5s

    redisClusterContext *cc = redisClusterContextInit();
    redisClusterSetOptionAddNodes(cc, initnode);
    redisClusterSetOptionConnectTimeout(cc, timeout);
    redisClusterSetOptionRouteUseSlots(cc);
    redisClusterConnect2(cc);
    if (cc && cc->err) {
        fprintf(stderr, "Connect error: %s\n", cc->errstr);
        exit(100);
    }

    char command[256];
    while (fgets(command, 256, stdin)) {
        if (command[0] == '#')
            continue; // Skip comments

        size_t len = strlen(command);
        if (command[len - 1] == '\n') // Chop trailing line break
            command[len - 1] = '\0';

        nodeIterator ni;
        initNodeIterator(&ni, cc);

        cluster_node *node;
        while ((node = nodeNext(&ni)) != NULL) {

            redisReply *reply;
            reply = redisClusterCommandToNode(cc, node, command);
            if (!reply || cc->err) {
                fprintf(stderr, "redisClusterCommand error: %s\n", cc->errstr);
                exit(101);
            }

            switch (reply->type) {
            case REDIS_REPLY_STRING:
            case REDIS_REPLY_ERROR:
            case REDIS_REPLY_VERB:
                printf("%s\n", reply->str);
                break;
            case REDIS_REPLY_INTEGER:
                printf("%lld\n", reply->integer);
                break;
            case REDIS_REPLY_ARRAY:
                printf("ARRAY %ld\n", reply->elements);
                break;
            case REDIS_REPLY_DOUBLE:
                printf("%f\n", reply->dval);
                break;
            }
            freeReplyObject(reply);
        }
    }

    redisClusterFree(cc);
    return 0;
}
