#include "hiredis_cluster/hircluster.h"

#include "hiredis/hiredis_ssl.h"
#include <stdio.h>
#include <stdlib.h>

#define CLUSTER_NODE_TLS "127.0.0.1:7301"

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    redisSSLContext *ssl;
    redisSSLContextError ssl_error;

    redisInitOpenSSL();
    ssl = redisCreateSSLContext("ca.crt", NULL, "client.crt", "client.key",
                                NULL, &ssl_error);
    if (!ssl) {
        printf("SSL Context error: %s\n", redisSSLContextGetError(ssl_error));
        exit(1);
    }

    struct timeval timeout = {1, 500000}; // 1.5s

    redisClusterContext *cc = redisClusterContextInit();
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE_TLS);
    redisClusterSetOptionConnectTimeout(cc, timeout);
    redisClusterSetOptionRouteUseSlots(cc);
    redisClusterSetOptionParseSlaves(cc);
    redisClusterSetOptionEnableSSL(cc, ssl);
    redisClusterConnect2(cc);
    if (cc && cc->err) {
        printf("Error: %s\n", cc->errstr);
        // handle error
        exit(-1);
    }

    redisReply *reply =
        (redisReply *)redisClusterCommand(cc, "SET %s %s", "key", "value");
    if (!reply) {
        printf("Reply missing: %s\n", cc->errstr);
        exit(-1);
    }
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    redisReply *reply2 = (redisReply *)redisClusterCommand(cc, "GET %s", "key");
    if (!reply2) {
        printf("Reply missing: %s\n", cc->errstr);
        exit(-1);
    }
    printf("GET: %s\n", reply2->str);
    freeReplyObject(reply2);

    redisClusterFree(cc);
    redisFreeSSLContext(ssl);
    return 0;
}
