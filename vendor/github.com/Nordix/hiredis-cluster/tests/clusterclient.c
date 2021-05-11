/*
 * This program connects to a cluster and then reads commands from stdin, such
 * as "SET foo bar", one per line and prints the results to stdout.
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
        size_t len = strlen(command);
        if (command[len - 1] == '\n') // Chop trailing line break
            command[len - 1] = '\0';
        redisReply *reply = (redisReply *)redisClusterCommand(cc, command);
        if (cc->err) {
            fprintf(stderr, "redisClusterCommand error: %s\n", cc->errstr);
            exit(101);
        }
        printf("%s\n", reply->str);
        freeReplyObject(reply);
    }

    redisClusterFree(cc);
    return 0;
}
