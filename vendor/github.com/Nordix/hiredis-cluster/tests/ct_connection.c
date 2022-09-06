#include "adapters/libevent.h"
#include "hircluster.h"
#include "test_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"
#define CLUSTER_NODE_WITH_PASSWORD "127.0.0.1:7100"
#define CLUSTER_USERNAME "default"
#define CLUSTER_PASSWORD "secretword"

// Connecting to a password protected cluster and
// providing a correct password.
void test_password_ok() {
    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionPassword(cc, CLUSTER_PASSWORD);

    int status;
    status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);
    load_redis_version(cc);

    // Test connection
    redisReply *reply;
    reply = redisClusterCommand(cc, "SET key1 Hello");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    redisClusterFree(cc);
}

// Connecting to a password protected cluster and
// providing wrong password.
void test_password_wrong() {
    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionPassword(cc, "faultypass");

    int status;
    status = redisClusterConnect2(cc);
    assert(status == REDIS_ERR);

    assert(cc->err == REDIS_ERR_OTHER);
    if (redis_version_less_than(6, 0))
        assert(strcmp(cc->errstr, "ERR invalid password") == 0);
    else
        assert(strncmp(cc->errstr, "WRONGPASS", 9) == 0);

    redisClusterFree(cc);
}

// Connecting to a password protected cluster and
// not providing any password.
void test_password_missing() {
    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE_WITH_PASSWORD);

    // A password is not configured..
    int status;
    status = redisClusterConnect2(cc);
    assert(status == REDIS_ERR);

    assert(cc->err == REDIS_ERR_OTHER);
    assert(strncmp(cc->errstr, "NOAUTH", 6) == 0);

    redisClusterFree(cc);
}

// Connect to a cluster and authenticate using username and password,
// i.e. 'AUTH <username> <password>'
void test_username_ok() {
    if (redis_version_less_than(6, 0))
        return;

    // Connect to the cluster using username and password
    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionUsername(cc, CLUSTER_USERNAME);
    redisClusterSetOptionPassword(cc, CLUSTER_PASSWORD);

    int ret = redisClusterConnect2(cc);
    ASSERT_MSG(ret == REDIS_OK, cc->errstr);

    // Test connection
    redisReply *reply = redisClusterCommand(cc, "SET key1 Hello");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    redisClusterFree(cc);
}

// Test of disabling the use of username after it was enabled.
void test_username_disabled() {
    if (redis_version_less_than(6, 0))
        return;

    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionUsername(cc, "missing-user");
    redisClusterSetOptionPassword(cc, CLUSTER_PASSWORD);

    // Connect using 'AUTH <username> <password>' should fail
    int ret = redisClusterConnect2(cc);
    assert(ret == REDIS_ERR);
    assert(cc->err == REDIS_ERR_OTHER);
    assert(strncmp(cc->errstr, "WRONGPASS invalid username-password pair",
                   40) == 0);

    // Disable use of username (2 alternatives)
    ret = redisClusterSetOptionUsername(cc, NULL);
    ASSERT_MSG(ret == REDIS_OK, cc->errstr);
    ret = redisClusterSetOptionUsername(cc, "");
    ASSERT_MSG(ret == REDIS_OK, cc->errstr);

    // Connect using 'AUTH <password>' should pass
    ret = redisClusterConnect2(cc);
    ASSERT_MSG(ret == REDIS_OK, cc->errstr);

    // Test connection
    redisReply *reply = redisClusterCommand(cc, "SET key1 Hello");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    redisClusterFree(cc);
}

// Connect and handle two clusters simultaneously
void test_multicluster() {
    int ret;
    redisReply *reply;

    // Connect to first cluster
    redisClusterContext *cc1 = redisClusterContextInit();
    assert(cc1);
    redisClusterSetOptionAddNodes(cc1, CLUSTER_NODE);
    ret = redisClusterConnect2(cc1);
    ASSERT_MSG(ret == REDIS_OK, cc1->errstr);

    // Connect to second cluster
    redisClusterContext *cc2 = redisClusterContextInit();
    assert(cc2);
    redisClusterSetOptionAddNodes(cc2, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionPassword(cc2, CLUSTER_PASSWORD);
    ret = redisClusterConnect2(cc2);
    ASSERT_MSG(ret == REDIS_OK, cc2->errstr);

    // Set keys differently in clusters
    reply = redisClusterCommand(cc1, "SET key Hello1");
    CHECK_REPLY_OK(cc1, reply);
    freeReplyObject(reply);

    reply = redisClusterCommand(cc2, "SET key Hello2");
    CHECK_REPLY_OK(cc2, reply);
    freeReplyObject(reply);

    // Verify keys in clusters
    reply = redisClusterCommand(cc1, "GET key");
    CHECK_REPLY_STR(cc1, reply, "Hello1");
    freeReplyObject(reply);

    reply = redisClusterCommand(cc2, "GET key");
    CHECK_REPLY_STR(cc2, reply, "Hello2");
    freeReplyObject(reply);

    // Disconnect from first cluster
    redisClusterFree(cc1);

    // Verify that key is still accessible in connected cluster
    reply = redisClusterCommand(cc2, "GET key");
    CHECK_REPLY_STR(cc2, reply, "Hello2");
    freeReplyObject(reply);

    redisClusterFree(cc2);
}

/* Connect to a non-routable address which results in a connection timeout. */
void test_connect_timeout() {
    struct timeval timeout = {0, 200000};

    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);

    /* Configure a non-routable IP address and a timeout */
    redisClusterSetOptionAddNodes(cc, "192.168.0.0:7000");
    redisClusterSetOptionConnectTimeout(cc, timeout);

    int status = redisClusterConnect2(cc);
    assert(status == REDIS_ERR);
    assert(cc->err == REDIS_ERR_IO);
    assert(strcmp(cc->errstr, "Connection timed out") == 0);

    redisClusterFree(cc);
}

/* Connect using a pre-configured command timeout */
void test_command_timeout() {
    struct timeval timeout = {0, 10000};

    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE);
    redisClusterSetOptionTimeout(cc, timeout);

    int status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    nodeIterator ni;
    initNodeIterator(&ni, cc);
    cluster_node *node = nodeNext(&ni);
    assert(node);

    /* Simulate a command timeout */
    redisReply *reply;
    reply = redisClusterCommandToNode(cc, node, "DEBUG SLEEP 0.2");
    assert(reply == NULL);
    assert(cc->err == REDIS_ERR_IO);

    /* Make sure debug sleep is done before leaving testcase */
    for (int i = 0; i < 20; ++i) {
        reply = redisClusterCommandToNode(cc, node, "SET key1 Hello");
        if (reply && reply->type == REDIS_REPLY_STATUS)
            break;
    }
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    redisClusterFree(cc);
}

/* Connect and configure a command timeout while connected. */
void test_command_timeout_set_while_connected() {
    struct timeval timeout = {0, 10000};

    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE);

    int status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    nodeIterator ni;
    initNodeIterator(&ni, cc);
    cluster_node *node = nodeNext(&ni);
    assert(node);

    redisReply *reply;
    reply = redisClusterCommandToNode(cc, node, "DEBUG SLEEP 0.2");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    /* Set command timeout while connected */
    redisClusterSetOptionTimeout(cc, timeout);

    reply = redisClusterCommandToNode(cc, node, "DEBUG SLEEP 0.2");
    assert(reply == NULL);
    assert(cc->err == REDIS_ERR_IO);

    /* Make sure debug sleep is done before leaving testcase */
    for (int i = 0; i < 20; ++i) {
        reply = redisClusterCommandToNode(cc, node, "SET key1 Hello");
        if (reply && reply->type == REDIS_REPLY_STATUS)
            break;
    }
    CHECK_REPLY_OK(cc, reply);
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
        if (reply->type == REDIS_REPLY_ERROR ||
            reply->type == REDIS_REPLY_STATUS ||
            reply->type == REDIS_REPLY_STRING ||
            reply->type == REDIS_REPLY_DOUBLE ||
            reply->type == REDIS_REPLY_VERB) {
            assert(strcmp(reply->str, expect->str) == 0);
        }
    }
    if (expect->disconnect)
        redisClusterAsyncDisconnect(cc);
}

// Connecting to a password protected cluster using
// the async API, providing correct password.
void test_async_password_ok() {
    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionPassword(acc->cc, CLUSTER_PASSWORD);

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc, base);

    int ret;
    ret = redisClusterConnect2(acc->cc);
    assert(ret == REDIS_OK);
    assert(acc->err == 0);
    assert(acc->cc->err == 0);

    // Test connection
    ExpectedResult r = {
        .type = REDIS_REPLY_STATUS, .str = "OK", .disconnect = true};
    ret = redisClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == REDIS_OK);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

// Connecting to a password protected cluster using
// the async API, providing wrong password.
void test_async_password_wrong() {
    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionPassword(acc->cc, "faultypass");

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc, base);

    int ret;
    ret = redisClusterConnect2(acc->cc);
    assert(ret == REDIS_ERR);
    assert(acc->err == REDIS_OK); // TODO: This must be wrong!
    assert(acc->cc->err == REDIS_ERR_OTHER);
    if (redis_version_less_than(6, 0))
        assert(strcmp(acc->cc->errstr, "ERR invalid password") == 0);
    else
        assert(strncmp(acc->cc->errstr, "WRONGPASS", 9) == 0);

    // No connection
    ExpectedResult r;
    ret = redisClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == REDIS_ERR);
    assert(acc->err == REDIS_ERR_OTHER);
    assert(strcmp(acc->errstr, "node get by table error") == 0);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

// Connecting to a password protected cluster using
// the async API, not providing a password.
void test_async_password_missing() {
    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE_WITH_PASSWORD);
    // Password not configured

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc, base);

    int ret;
    ret = redisClusterConnect2(acc->cc);
    assert(ret == REDIS_ERR);
    assert(acc->err == REDIS_OK); // TODO: This must be wrong!
    assert(acc->cc->err == REDIS_ERR_OTHER);
    assert(strncmp(acc->cc->errstr, "NOAUTH", 6) == 0);

    // No connection
    ExpectedResult r;
    ret = redisClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == REDIS_ERR);
    assert(acc->err == REDIS_ERR_OTHER);
    assert(strcmp(acc->errstr, "node get by table error") == 0);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

// Connect to a cluster and authenticate using username and password
void test_async_username_ok() {
    if (redis_version_less_than(6, 0))
        return;

    // Connect to the cluster using username and password
    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionUsername(acc->cc, "missing-user");
    redisClusterSetOptionPassword(acc->cc, CLUSTER_PASSWORD);

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc, base);

    // Connect using wrong username should fail
    int ret = redisClusterConnect2(acc->cc);
    assert(ret == REDIS_ERR);
    assert(acc->cc->err == REDIS_ERR_OTHER);
    assert(strncmp(acc->cc->errstr, "WRONGPASS invalid username-password pair",
                   40) == 0);

    // Set correct username
    ret = redisClusterSetOptionUsername(acc->cc, CLUSTER_USERNAME);
    ASSERT_MSG(ret == REDIS_OK, acc->cc->errstr);

    // Connect using correct username should pass
    ret = redisClusterConnect2(acc->cc);
    assert(ret == REDIS_OK);
    assert(acc->err == 0);
    assert(acc->cc->err == 0);

    // Test connection
    ExpectedResult r = {
        .type = REDIS_REPLY_STATUS, .str = "OK", .disconnect = true};
    ret = redisClusterAsyncCommand(acc, commandCallback, &r, "SET key1 Hello");
    assert(ret == REDIS_OK);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

// Connect and handle two clusters simultaneously using the async API
void test_async_multicluster() {
    int ret;

    redisClusterAsyncContext *acc1 = redisClusterAsyncContextInit();
    assert(acc1);
    redisClusterAsyncSetConnectCallback(acc1, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc1, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc1->cc, CLUSTER_NODE);

    redisClusterAsyncContext *acc2 = redisClusterAsyncContextInit();
    assert(acc2);
    redisClusterAsyncSetConnectCallback(acc2, callbackExpectOk);
    redisClusterAsyncSetDisconnectCallback(acc2, callbackExpectOk);
    redisClusterSetOptionAddNodes(acc2->cc, CLUSTER_NODE_WITH_PASSWORD);
    redisClusterSetOptionPassword(acc2->cc, CLUSTER_PASSWORD);

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc1, base);
    redisClusterLibeventAttach(acc2, base);

    // Connect to first cluster
    ret = redisClusterConnect2(acc1->cc);
    assert(ret == REDIS_OK);
    assert(acc1->err == 0);
    assert(acc1->cc->err == 0);

    // Connect to second cluster
    ret = redisClusterConnect2(acc2->cc);
    assert(ret == REDIS_OK);
    assert(acc2->err == 0);
    assert(acc2->cc->err == 0);

    // Set keys differently in clusters
    ExpectedResult r1 = {.type = REDIS_REPLY_STATUS, .str = "OK"};
    ret = redisClusterAsyncCommand(acc1, commandCallback, &r1, "SET key A");
    assert(ret == REDIS_OK);

    ExpectedResult r2 = {.type = REDIS_REPLY_STATUS, .str = "OK"};
    ret = redisClusterAsyncCommand(acc2, commandCallback, &r2, "SET key B");
    assert(ret == REDIS_OK);

    // Verify key in first cluster
    ExpectedResult r3 = {.type = REDIS_REPLY_STRING, .str = "A"};
    ret = redisClusterAsyncCommand(acc1, commandCallback, &r3, "GET key");
    assert(ret == REDIS_OK);

    // Verify key in second cluster and disconnect
    ExpectedResult r4 = {
        .type = REDIS_REPLY_STRING, .str = "B", .disconnect = true};
    ret = redisClusterAsyncCommand(acc2, commandCallback, &r4, "GET key");
    assert(ret == REDIS_OK);

    // Verify that key is still accessible in connected cluster
    ExpectedResult r5 = {
        .type = REDIS_REPLY_STRING, .str = "A", .disconnect = true};
    ret = redisClusterAsyncCommand(acc1, commandCallback, &r5, "GET key");
    assert(ret == REDIS_OK);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc1);
    redisClusterAsyncFree(acc2);
    event_base_free(base);
}

/* Connect to a non-routable address which results in a connection timeout. */
void test_async_connect_timeout() {
    struct timeval timeout = {0, 200000};

    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);

    /* Configure a non-routable IP address and a timeout */
    redisClusterSetOptionAddNodes(acc->cc, "192.168.0.0:7000");
    redisClusterSetOptionConnectTimeout(acc->cc, timeout);

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc, base);

    int status = redisClusterConnect2(acc->cc);
    assert(status == REDIS_ERR);
    assert(acc->cc->err == REDIS_ERR_IO);
    assert(strcmp(acc->cc->errstr, "Connection timed out") == 0);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

/* Connect using a pre-configured command timeout */
void test_async_command_timeout() {
    struct timeval timeout = {0, 10000};

    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterSetOptionAddNodes(acc->cc, CLUSTER_NODE);
    redisClusterSetOptionTimeout(acc->cc, timeout);

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(acc, base);

    int status = redisClusterConnect2(acc->cc);
    assert(status == REDIS_OK);
    assert(acc->cc->err == 0);

    nodeIterator ni;
    initNodeIterator(&ni, acc->cc);
    cluster_node *node = nodeNext(&ni);
    assert(node);

    /* Simulate a command timeout and expect a timeout error */
    ExpectedResult r = {
        .noreply = true, .errstr = "Timeout", .disconnect = true};
    status = redisClusterAsyncCommandToNode(acc, node, commandCallback, &r,
                                            "DEBUG SLEEP 0.2");
    assert(status == REDIS_OK);

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
}

int main() {

    test_password_ok();
    test_password_wrong();
    test_password_missing();
    test_username_ok();
    test_username_disabled();
    test_multicluster();
    test_connect_timeout();
    test_command_timeout();
    test_command_timeout_set_while_connected();

    test_async_password_ok();
    test_async_password_wrong();
    test_async_password_missing();
    test_async_username_ok();
    test_async_multicluster();
    test_async_connect_timeout();
    test_async_command_timeout();

    return 0;
}
