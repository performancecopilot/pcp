#include "adapters/libevent.h"
#include "hircluster.h"
#include "test_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define CLUSTER_NODE "127.0.0.1:7000"

void getCallback(redisClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    redisReply *reply = (redisReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);

    /* Disconnect after receiving the first reply to GET */
    redisClusterAsyncDisconnect(acc);
}

void setCallback(redisClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    redisReply *reply = (redisReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);
}

void connectCallback(const redisAsyncContext *ac, int status) {
    ASSERT_MSG(status == REDIS_OK, ac->errstr);
    printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void disconnectCallback(const redisAsyncContext *ac, int status) {
    ASSERT_MSG(status == REDIS_OK, ac->errstr);
    printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

int main() {
    redisClusterAsyncContext *acc =
        redisClusterAsyncConnect(CLUSTER_NODE, HIRCLUSTER_FLAG_NULL);
    assert(acc);
    ASSERT_MSG(acc->err == 0, acc->errstr);

    int status;
    struct event_base *base = event_base_new();
    status = redisClusterLibeventAttach(acc, base);
    assert(status == REDIS_OK);

    redisClusterAsyncSetConnectCallback(acc, connectCallback);
    redisClusterAsyncSetDisconnectCallback(acc, disconnectCallback);

    status = redisClusterAsyncCommand(acc, setCallback, (char *)"ID",
                                      "SET key12345 value");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    status = redisClusterAsyncCommand(acc, getCallback, (char *)"ID",
                                      "GET key12345");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    status = redisClusterAsyncCommand(acc, setCallback, (char *)"ID",
                                      "SET key23456 value2");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    status = redisClusterAsyncCommand(acc, getCallback, (char *)"ID",
                                      "GET key23456");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
    return 0;
}
