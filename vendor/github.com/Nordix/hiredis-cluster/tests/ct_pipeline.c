#include "adapters/libevent.h"
#include "hircluster.h"
#include "test_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"

// Test of two pipelines using sync API
void test_pipeline() {
    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);

    int status;
    status = redisClusterSetOptionAddNodes(cc, CLUSTER_NODE);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterAppendCommand(cc, "SET foo one");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);
    status = redisClusterAppendCommand(cc, "SET bar two");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);
    status = redisClusterAppendCommand(cc, "GET foo");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);
    status = redisClusterAppendCommand(cc, "GET bar");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    redisReply *reply;
    redisClusterGetReply(cc, (void *)&reply); // reply for: SET foo one
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    redisClusterGetReply(cc, (void *)&reply); // reply for: SET bar two
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    redisClusterGetReply(cc, (void *)&reply); // reply for: GET foo
    CHECK_REPLY_STR(cc, reply, "one");
    freeReplyObject(reply);

    redisClusterGetReply(cc, (void *)&reply); // reply for: GET bar
    CHECK_REPLY_STR(cc, reply, "two");
    freeReplyObject(reply);

    redisClusterFree(cc);
}

// Test of pipelines containing multi-node commands
void test_pipeline_with_multinode_commands() {
    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);

    int status;
    status = redisClusterSetOptionAddNodes(cc, CLUSTER_NODE);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterAppendCommand(cc, "MSET key1 Hello key2 World key3 !");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterAppendCommand(cc, "MGET key1 key2 key3");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    redisReply *reply;
    redisClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    redisClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY_ARRAY(cc, reply, 3);
    CHECK_REPLY_STR(cc, reply->element[0], "Hello");
    CHECK_REPLY_STR(cc, reply->element[1], "World");
    CHECK_REPLY_STR(cc, reply->element[2], "!");
    freeReplyObject(reply);

    redisClusterFree(cc);
}

//------------------------------------------------------------------------------
// Async API
//------------------------------------------------------------------------------

typedef struct ExpectedResult {
    int type;
    char *str;
    bool disconnect;
} ExpectedResult;

// Callback for Redis connects and disconnects
void callbackExpectOk(const redisAsyncContext *ac, int status) {
    UNUSED(ac);
    assert(status == REDIS_OK);
}

// Callback for async commands, verifies the redisReply
void commandCallback(redisClusterAsyncContext *cc, void *r, void *privdata) {
    redisReply *reply = (redisReply *)r;
    ExpectedResult *expect = (ExpectedResult *)privdata;
    assert(reply != NULL);
    assert(reply->type == expect->type);
    assert(strcmp(reply->str, expect->str) == 0);

    if (expect->disconnect) {
        redisClusterAsyncDisconnect(cc);
    }
}

// Test of two pipelines using async API
// In an asynchronous context, commands are automatically pipelined due to the
// nature of an event loop. Therefore, unlike the synchronous API, there is only
// a single way to send commands.
void test_async_pipeline() {
    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE);

    int status;
    status = redisClusterConnect2(acc->cc);
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    struct event_base *base = event_base_new();
    status = redisClusterLibeventAttach(acc, base);
    assert(status == REDIS_OK);

    ExpectedResult r1 = {.type = REDIS_REPLY_STATUS, .str = "OK"};
    status = redisClusterAsyncCommand(acc, commandCallback, &r1, "SET foo six");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    ExpectedResult r2 = {.type = REDIS_REPLY_STATUS, .str = "OK"};
    status = redisClusterAsyncCommand(acc, commandCallback, &r2, "SET bar ten");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    ExpectedResult r3 = {.type = REDIS_REPLY_STRING, .str = "six"};
    status = redisClusterAsyncCommand(acc, commandCallback, &r3, "GET foo");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    ExpectedResult r4 = {
        .type = REDIS_REPLY_STRING, .str = "ten", .disconnect = true};
    status = redisClusterAsyncCommand(acc, commandCallback, &r4, "GET bar");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

int main() {

    test_pipeline();
    test_pipeline_with_multinode_commands();

    test_async_pipeline();
    // Asynchronous API does not support multi-key commands

    return 0;
}
