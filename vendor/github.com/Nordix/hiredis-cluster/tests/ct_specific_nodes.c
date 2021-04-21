#include "adapters/libevent.h"
#include "hircluster.h"
#include "test_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"

void test_command_to_single_node(redisClusterContext *cc) {
    redisReply *reply;

    dictIterator di;
    dictInitIterator(&di, cc->nodes);

    dictEntry *de = dictNext(&di);
    assert(de);
    cluster_node *node = dictGetEntryVal(de);
    assert(node);

    reply = redisClusterCommandToNode(cc, node, "DBSIZE");
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
}

void test_command_to_all_nodes(redisClusterContext *cc) {

    nodeIterator ni;
    initNodeIterator(&ni, cc);

    cluster_node *node;
    while ((node = nodeNext(&ni)) != NULL) {

        redisReply *reply;
        reply = redisClusterCommandToNode(cc, node, "DBSIZE");
        CHECK_REPLY(cc, reply);
        CHECK_REPLY_TYPE(reply, REDIS_REPLY_INTEGER);
        freeReplyObject(reply);
    }
}

void test_transaction(redisClusterContext *cc) {

    cluster_node *node = redisClusterGetNodeByKey(cc, "foo");
    assert(node);

    redisReply *reply;
    reply = redisClusterCommandToNode(cc, node, "MULTI");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = redisClusterCommandToNode(cc, node, "SET foo 99");
    CHECK_REPLY_QUEUED(cc, reply);
    freeReplyObject(reply);

    reply = redisClusterCommandToNode(cc, node, "INCR foo");
    CHECK_REPLY_QUEUED(cc, reply);
    freeReplyObject(reply);

    reply = redisClusterCommandToNode(cc, node, "EXEC");
    CHECK_REPLY_ARRAY(cc, reply, 2);
    CHECK_REPLY_OK(cc, reply->element[0]);
    CHECK_REPLY_INT(cc, reply->element[1], 100);
    freeReplyObject(reply);
}

void test_streams(redisClusterContext *cc) {
    redisReply *reply;
    char *id;

    /* Get the node that handles given stream */
    cluster_node *node = redisClusterGetNodeByKey(cc, "mystream");
    assert(node);

    /* Preparation: remove old stream/key */
    reply = redisClusterCommandToNode(cc, node, "DEL mystream");
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_INTEGER);
    freeReplyObject(reply);

    /* Query wrong node */
    cluster_node *wrongNode = redisClusterGetNodeByKey(cc, "otherstream");
    assert(node != wrongNode);
    reply = redisClusterCommandToNode(cc, wrongNode, "XLEN mystream");
    CHECK_REPLY_ERROR(cc, reply, "MOVED");
    freeReplyObject(reply);

    /* Verify stream length before adding entries */
    reply = redisClusterCommandToNode(cc, node, "XLEN mystream");
    CHECK_REPLY_INT(cc, reply, 0);
    freeReplyObject(reply);

    /* Add entries to a created stream */
    reply = redisClusterCommandToNode(cc, node, "XADD mystream * name t800");
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_STRING);
    freeReplyObject(reply);

    reply = redisClusterCommandToNode(
        cc, node, "XADD mystream * name Sara surname OConnor");
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_STRING);
    id = strdup(reply->str); /* Keep this id for later inspections */
    freeReplyObject(reply);

    /* Verify stream length after adding entries */
    reply = redisClusterCommandToNode(cc, node, "XLEN mystream");
    CHECK_REPLY_INT(cc, reply, 2);
    freeReplyObject(reply);

    /* Modify the stream */
    reply = redisClusterCommandToNode(cc, node, "XTRIM mystream MAXLEN 1");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    /* Verify stream length after modifying the stream */
    reply = redisClusterCommandToNode(cc, node, "XLEN mystream");
    CHECK_REPLY_INT(cc, reply, 1); /* 1 entry left */
    freeReplyObject(reply);

    /* Read from the stream */
    reply =
        redisClusterCommandToNode(cc, node, "XREAD COUNT 2 STREAMS mystream 0");
    CHECK_REPLY_ARRAY(cc, reply, 1); /* Reply from a single stream */

    /* Verify the reply from stream */
    CHECK_REPLY_ARRAY(cc, reply->element[0], 2);
    CHECK_REPLY_STR(cc, reply->element[0]->element[0], "mystream");
    CHECK_REPLY_ARRAY(cc, reply->element[0]->element[1], 1); /* single entry */

    /* Verify the entry, an array of id and field+value elements */
    redisReply *entry = reply->element[0]->element[1]->element[0];
    CHECK_REPLY_ARRAY(cc, entry, 2);
    CHECK_REPLY_STR(cc, entry->element[0], id);

    CHECK_REPLY_ARRAY(cc, entry->element[1], 4);
    CHECK_REPLY_STR(cc, entry->element[1]->element[0], "name");
    CHECK_REPLY_STR(cc, entry->element[1]->element[1], "Sara");
    CHECK_REPLY_STR(cc, entry->element[1]->element[2], "surname");
    CHECK_REPLY_STR(cc, entry->element[1]->element[3], "OConnor");
    freeReplyObject(reply);

    /* Delete the entry in stream */
    reply = redisClusterCommandToNode(cc, node, "XDEL mystream %s", id);
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    /* Blocking read of stream */
    reply = redisClusterCommandToNode(
        cc, node, "XREAD COUNT 2 BLOCK 200 STREAMS mystream 0");
    CHECK_REPLY_NIL(cc, reply);
    freeReplyObject(reply);

    /* Create a consumer group */
    reply = redisClusterCommandToNode(cc, node,
                                      "XGROUP CREATE mystream mygroup1 0");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    /* Create a consumer */
    reply = redisClusterCommandToNode(
        cc, node, "XGROUP CREATECONSUMER mystream mygroup1 myconsumer123");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    /* Blocking read of consumer group */
    reply = redisClusterCommandToNode(cc, node,
                                      "XREADGROUP GROUP mygroup1 myconsumer123 "
                                      "COUNT 2 BLOCK 200 STREAMS mystream 0");
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_ARRAY);
    freeReplyObject(reply);

    free(id);
}

void test_pipeline_to_single_node(redisClusterContext *cc) {
    int status;
    redisReply *reply;

    dictIterator di;
    dictInitIterator(&di, cc->nodes);

    dictEntry *de = dictNext(&di);
    assert(de);
    cluster_node *node = dictGetEntryVal(de);
    assert(node);

    status = redisClusterAppendCommandToNode(cc, node, "DBSIZE");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    // Trigger send of pipeline commands
    redisClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
}

void test_pipeline_to_all_nodes(redisClusterContext *cc) {

    nodeIterator ni;
    initNodeIterator(&ni, cc);

    cluster_node *node;
    while ((node = nodeNext(&ni)) != NULL) {
        int status = redisClusterAppendCommandToNode(cc, node, "DBSIZE");
        ASSERT_MSG(status == REDIS_OK, cc->errstr);
    }

    // Get replies from 3 node cluster
    redisReply *reply;
    redisClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_INTEGER);
    freeReplyObject(reply);

    redisClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_INTEGER);
    freeReplyObject(reply);

    redisClusterGetReply(cc, (void *)&reply);
    CHECK_REPLY(cc, reply);
    CHECK_REPLY_TYPE(reply, REDIS_REPLY_INTEGER);
    freeReplyObject(reply);

    redisClusterGetReply(cc, (void *)&reply);
    assert(reply == NULL);
}

void test_pipeline_transaction(redisClusterContext *cc) {
    int status;
    redisReply *reply;

    cluster_node *node = redisClusterGetNodeByKey(cc, "foo");
    assert(node);

    status = redisClusterAppendCommandToNode(cc, node, "MULTI");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);
    status = redisClusterAppendCommandToNode(cc, node, "SET foo 199");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);
    status = redisClusterAppendCommandToNode(cc, node, "INCR foo");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);
    status = redisClusterAppendCommandToNode(cc, node, "EXEC");
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    // Trigger send of pipeline commands
    {
        redisClusterGetReply(cc, (void *)&reply); // MULTI
        CHECK_REPLY_OK(cc, reply);
        freeReplyObject(reply);

        redisClusterGetReply(cc, (void *)&reply); // SET
        CHECK_REPLY_QUEUED(cc, reply);
        freeReplyObject(reply);

        redisClusterGetReply(cc, (void *)&reply); // INCR
        CHECK_REPLY_QUEUED(cc, reply);
        freeReplyObject(reply);

        redisClusterGetReply(cc, (void *)&reply); // EXEC
        CHECK_REPLY_ARRAY(cc, reply, 2);
        CHECK_REPLY_OK(cc, reply->element[0]);
        CHECK_REPLY_INT(cc, reply->element[1], 200);
        freeReplyObject(reply);
    }
}

//------------------------------------------------------------------------------
// Async API
//------------------------------------------------------------------------------
typedef struct ExpectedResult {
    int type;
    char *str;
    bool disconnect;
    bool noreply;
    char *errstr;
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
    if (expect->noreply) {
        assert(reply == NULL);
        assert(strcmp(cc->errstr, expect->errstr) == 0);
    } else {
        assert(reply != NULL);
        assert(reply->type == expect->type);
        if (reply->type != REDIS_REPLY_INTEGER)
            assert(strcmp(reply->str, expect->str) == 0);
    }
    if (expect->disconnect)
        redisClusterAsyncDisconnect(cc);
}

void test_async_to_single_node() {
    int status;

    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE);
    redisClusterSetOptionMaxRedirect(acc->cc, 1);
    redisClusterSetOptionRouteUseSlots(acc->cc);
    status = redisClusterConnect2(acc->cc);
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    struct event_base *base = event_base_new();
    status = redisClusterLibeventAttach(acc, base);
    assert(status == REDIS_OK);

    dictIterator di;
    dictInitIterator(&di, acc->cc->nodes);

    dictEntry *de = dictNext(&di);
    assert(de);
    cluster_node *node = dictGetEntryVal(de);
    assert(node);

    ExpectedResult r1 = {.type = REDIS_REPLY_INTEGER, .disconnect = true};
    status = redisClusterAsyncCommandToNode(acc, node, commandCallback, &r1,
                                            "DBSIZE");
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

void test_async_formatted_to_single_node() {
    int status;

    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE);
    redisClusterSetOptionMaxRedirect(acc->cc, 1);
    redisClusterSetOptionRouteUseSlots(acc->cc);
    status = redisClusterConnect2(acc->cc);
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    struct event_base *base = event_base_new();
    status = redisClusterLibeventAttach(acc, base);
    assert(status == REDIS_OK);

    dictIterator di;
    dictInitIterator(&di, acc->cc->nodes);

    dictEntry *de = dictNext(&di);
    assert(de);
    cluster_node *node = dictGetEntryVal(de);
    assert(node);

    ExpectedResult r1 = {.type = REDIS_REPLY_INTEGER, .disconnect = true};
    char command[] = "*1\r\n$6\r\nDBSIZE\r\n";
    status = redisClusterAsyncFormattedCommandToNode(
        acc, node, commandCallback, &r1, command, strlen(command));
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

void test_async_to_all_nodes() {
    int status;

    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE);
    redisClusterSetOptionMaxRedirect(acc->cc, 1);
    redisClusterSetOptionRouteUseSlots(acc->cc);
    status = redisClusterConnect2(acc->cc);
    ASSERT_MSG(status == REDIS_OK, acc->errstr);

    struct event_base *base = event_base_new();
    status = redisClusterLibeventAttach(acc, base);
    assert(status == REDIS_OK);

    nodeIterator ni;
    initNodeIterator(&ni, acc->cc);

    ExpectedResult r1 = {.type = REDIS_REPLY_INTEGER};

    cluster_node *node;
    while ((node = nodeNext(&ni)) != NULL) {

        status = redisClusterAsyncCommandToNode(acc, node, commandCallback, &r1,
                                                "DBSIZE");
        ASSERT_MSG(status == REDIS_OK, acc->errstr);
    }

    // Normal command to trigger disconnect
    ExpectedResult r2 = {
        .type = REDIS_REPLY_STATUS, .str = "OK", .disconnect = true};
    status = redisClusterAsyncCommand(acc, commandCallback, &r2, "SET foo bar");

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

int main() {
    int status;

    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE);
    redisClusterSetOptionRouteUseSlots(cc);
    redisClusterSetOptionMaxRedirect(cc, 1);
    status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    // Synchronous API
    test_command_to_single_node(cc);
    test_command_to_all_nodes(cc);
    test_transaction(cc);
    test_streams(cc);

    // Pipeline API
    test_pipeline_to_single_node(cc);
    test_pipeline_to_all_nodes(cc);
    test_pipeline_transaction(cc);

    redisClusterFree(cc);

    // Asynchronous API
    test_async_to_single_node();
    test_async_formatted_to_single_node();
    test_async_to_all_nodes();

    return 0;
}
