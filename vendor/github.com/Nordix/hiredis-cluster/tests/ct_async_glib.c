#include "adapters/glib.h"
#include "hircluster.h"
#include "test_utils.h"
#include <assert.h>

#define CLUSTER_NODE "127.0.0.1:7000"

static GMainLoop *mainloop;

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
    g_main_loop_quit(mainloop);
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

    GMainContext *context = NULL;
    mainloop = g_main_loop_new(context, FALSE);

    redisClusterAsyncContext *acc =
        redisClusterAsyncConnect(CLUSTER_NODE, HIRCLUSTER_FLAG_NULL);
    assert(acc);
    ASSERT_MSG(acc->err == 0, acc->errstr);

    int status;
    redisClusterGlibAdapter adapter = {.context = context};
    status = redisClusterGlibAttach(acc, &adapter);
    assert(status == REDIS_OK);

    redisClusterAsyncSetConnectCallback(acc, connectCallback);
    redisClusterAsyncSetDisconnectCallback(acc, disconnectCallback);

    status = redisClusterAsyncCommand(acc, setCallback, "id", "SET key value");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    status = redisClusterAsyncCommand(acc, getCallback, "id", "GET key");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    g_main_loop_run(mainloop);

    redisClusterAsyncFree(acc);
    return 0;
}
