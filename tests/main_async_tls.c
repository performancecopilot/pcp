#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "adapters/libevent.h"
#include "hircluster.h"

#define CLUSTER_NODE_TLS "127.0.0.1:7300"

void getCallback(redisClusterAsyncContext *cc, void *r, void *privdata) {
    redisReply *reply = (redisReply *)r;
    if (reply == NULL) {
        if (cc->err) {
            printf("errstr: %s\n", cc->errstr);
        }
        return;
    }
    printf("privdata: %s reply: %s\n", (char *)privdata, reply->str);

    /* Disconnect after receiving the reply to GET */
    redisClusterAsyncDisconnect(cc);
}

void setCallback(redisClusterAsyncContext *cc, void *r, void *privdata) {
    redisReply *reply = (redisReply *)r;
    if (reply == NULL) {
        if (cc->err) {
            printf("errstr: %s\n", cc->errstr);
        }
        return;
    }
    printf("privdata: %s reply: %s\n", (char *)privdata, reply->str);
}

void connectCallback(const redisAsyncContext *ac, int status) {
    if (status != REDIS_OK) {
        printf("Error: %s\n", ac->errstr);
        return;
    }

    printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void disconnectCallback(const redisAsyncContext *ac, int status) {
    if (status != REDIS_OK) {
        printf("Error: %s\n", ac->errstr);
        return;
    }
    printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

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

    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, connectCallback);
    redisClusterAsyncSetDisconnectCallback(acc, disconnectCallback);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE_TLS);
    redisClusterSetOptionRouteUseSlots(acc->cc);
    redisClusterSetOptionParseSlaves(acc->cc);
    redisClusterSetOptionEnableSSL(acc->cc, ssl);
    redisClusterConnect2(acc->cc);
    if (acc->err) {
        printf("Error: %s\n", acc->errstr);
        exit(-1);
    }

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc, base);

    int status;
    status = redisClusterAsyncCommand(acc, setCallback, (char *)"THE_ID",
                                      "SET %s %s", "key", "value");
    if (status != REDIS_OK) {
        printf("error: err=%d errstr=%s\n", acc->err, acc->errstr);
    }

    status = redisClusterAsyncCommand(acc, getCallback, (char *)"THE_ID",
                                      "GET %s", "key");
    if (status != REDIS_OK) {
        printf("error: err=%d errstr=%s\n", acc->err, acc->errstr);
    }

    printf("Dispatch..\n");
    event_base_dispatch(base);

    printf("Done..\n");
    redisClusterAsyncFree(acc);
    redisFreeSSLContext(ssl);
    event_base_free(base);
    return 0;
}
