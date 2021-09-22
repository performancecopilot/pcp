#include "adapters/libuv.h"
#include "hircluster.h"
#include "test_utils.h"
#include <assert.h>

#define CLUSTER_NODE "127.0.0.1:7000"

void setCallback(redisClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    redisReply *reply = (redisReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);
}

void getCallback(redisClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    redisReply *reply = (redisReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);

    /* Disconnect after receiving the first reply to GET */
    redisClusterAsyncDisconnect(acc);
}

void connectCallback(const redisAsyncContext *ac, int status) {
    ASSERT_MSG(status == REDIS_OK, ac->errstr);
    printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void disconnectCallback(const redisAsyncContext *ac, int status) {
    ASSERT_MSG(status == REDIS_OK, ac->errstr);
    printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    redisClusterAsyncContext *acc =
        redisClusterAsyncConnect(CLUSTER_NODE, HIRCLUSTER_FLAG_NULL);
    assert(acc);
    ASSERT_MSG(acc->err == 0, acc->errstr);

    int status;
    uv_loop_t *loop = uv_default_loop();
    status = redisClusterLibuvAttach(acc, loop);
    assert(status == REDIS_OK);

    redisClusterAsyncSetConnectCallback(acc, connectCallback);
    redisClusterAsyncSetDisconnectCallback(acc, disconnectCallback);

    status = redisClusterAsyncCommand(acc, setCallback, (char *)"ID",
                                      "SET key value");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    status =
        redisClusterAsyncCommand(acc, getCallback, (char *)"ID", "GET key");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    uv_run(loop, UV_RUN_DEFAULT);

    redisClusterAsyncFree(acc);
    uv_loop_delete(loop);
    return 0;
}
