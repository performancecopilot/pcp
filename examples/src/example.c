#include "hiredis_cluster/hircluster.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    struct timeval timeout = {1, 500000}; // 1.5s

    redisClusterContext *cc = redisClusterContextInit();
    redisClusterSetOptionAddNodes(cc, "127.0.0.1:7000");
    redisClusterSetOptionConnectTimeout(cc, timeout);
    redisClusterSetOptionRouteUseSlots(cc);
    redisClusterConnect2(cc);
    if (cc && cc->err) {
        printf("Error: %s\n", cc->errstr);
        // handle error
        exit(-1);
    }

    redisReply *reply =
        (redisReply *)redisClusterCommand(cc, "SET %s %s", "key", "value");
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    redisReply *reply2 = (redisReply *)redisClusterCommand(cc, "GET %s", "key");
    printf("GET: %s\n", reply2->str);
    freeReplyObject(reply2);

    redisClusterFree(cc);
    return 0;
}
