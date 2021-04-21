#include "hircluster.h"
#include "test_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE "127.0.0.1:7000"

void test_exists(redisClusterContext *cc) {
    redisReply *reply;
    reply = (redisReply *)redisClusterCommand(cc, "SET key1 Hello");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "EXISTS key1");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "EXISTS nosuchkey");
    CHECK_REPLY_INT(cc, reply, 0);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "SET key2 World");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "EXISTS key1 key2 nosuchkey");
    CHECK_REPLY_INT(cc, reply, 2);
    freeReplyObject(reply);
}

void test_mset(redisClusterContext *cc) {
    redisReply *reply;
    reply = (redisReply *)redisClusterCommand(
        cc, "MSET key1 mset1 key2 mset2 key3 mset3");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "GET key1");
    CHECK_REPLY_STR(cc, reply, "mset1");
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "GET key2");
    CHECK_REPLY_STR(cc, reply, "mset2");
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "GET key3");
    CHECK_REPLY_STR(cc, reply, "mset3");
    freeReplyObject(reply);
}

void test_mget(redisClusterContext *cc) {
    redisReply *reply;
    reply = (redisReply *)redisClusterCommand(cc, "SET key1 mget1");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "SET key2 mget2");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "SET key3 mget3");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "MGET key1 key2 key3");
    CHECK_REPLY_ARRAY(cc, reply, 3);
    CHECK_REPLY_STR(cc, reply->element[0], "mget1");
    CHECK_REPLY_STR(cc, reply->element[1], "mget2");
    CHECK_REPLY_STR(cc, reply->element[2], "mget3");
    freeReplyObject(reply);
}

void test_hset_hget_hdel_hexists(redisClusterContext *cc) {
    redisReply *reply;

    // Prepare
    reply = (redisReply *)redisClusterCommand(cc, "HDEL myhash field1");
    CHECK_REPLY(cc, reply);
    freeReplyObject(reply);
    reply = (redisReply *)redisClusterCommand(cc, "HDEL myhash field2");
    CHECK_REPLY(cc, reply);
    freeReplyObject(reply);

    // Set hash field
    reply =
        (redisReply *)redisClusterCommand(cc, "HSET myhash field1 hsetvalue");
    CHECK_REPLY_INT(cc, reply, 1); // Set 1 field
    freeReplyObject(reply);

    // Set second hash field
    reply =
        (redisReply *)redisClusterCommand(cc, "HSET myhash field3 hsetvalue3");
    CHECK_REPLY_INT(cc, reply, 1); // Set 1 field
    freeReplyObject(reply);

    // Get field value
    reply = (redisReply *)redisClusterCommand(cc, "HGET myhash field1");
    CHECK_REPLY_STR(cc, reply, "hsetvalue");
    freeReplyObject(reply);

    // Get field that is not present
    reply = (redisReply *)redisClusterCommand(cc, "HGET myhash field2");
    CHECK_REPLY_NIL(cc, reply);
    freeReplyObject(reply);

    // Delete a field
    reply = (redisReply *)redisClusterCommand(cc, "HDEL myhash field1");
    CHECK_REPLY_INT(cc, reply, 1); // Delete 1 field
    freeReplyObject(reply);

    // Delete a field that is not present
    reply = (redisReply *)redisClusterCommand(cc, "HDEL myhash field2");
    CHECK_REPLY_INT(cc, reply, 0); // Nothing to delete
    freeReplyObject(reply);

    // Check if field exists
    reply = (redisReply *)redisClusterCommand(cc, "HEXISTS myhash field3");
    CHECK_REPLY_INT(cc, reply, 1); // exists
    freeReplyObject(reply);

    // Delete multiple fields at once
    reply = (redisReply *)redisClusterCommand(
        cc, "HDEL myhash field1 field2 field3");
    CHECK_REPLY_INT(cc, reply, 1); // field3 deleted
    freeReplyObject(reply);

    // Make sure field3 is deleted now
    reply = (redisReply *)redisClusterCommand(cc, "HEXISTS myhash field3");
    CHECK_REPLY_INT(cc, reply, 0); // no field
    freeReplyObject(reply);

    // As of Redis 4.0.0, HSET is variadic i.e. multiple field/value,
    // but this is currently not supported.
    reply = (redisReply *)redisClusterCommand(
        cc, "HSET myhash field1 hsetvalue1 field2 hsetvalue2");
    assert(reply == NULL);
}

// Command layout:
// eval <script> <no of keys> <keys..> <args..>
void test_eval(redisClusterContext *cc) {
    redisReply *reply;

    // Single key
    reply = (redisReply *)redisClusterCommand(
        cc, "eval %s 1 %s", "return redis.call('set',KEYS[1],'bar')", "foo");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    // Single key, string response
    reply = (redisReply *)redisClusterCommand(cc, "eval %s 1 %s",
                                              "return KEYS[1]", "key1");
    CHECK_REPLY_STR(cc, reply, "key1");
    freeReplyObject(reply);

    // Single key with single argument
    reply = (redisReply *)redisClusterCommand(
        cc, "eval %s 1 %s %s", "return {KEYS[1],ARGV[1]}", "key1", "first");
    CHECK_REPLY_ARRAY(cc, reply, 2);
    CHECK_REPLY_STR(cc, reply->element[0], "key1");
    CHECK_REPLY_STR(cc, reply->element[1], "first");
    freeReplyObject(reply);

    // Multi key, but handled by same instance
    reply = (redisReply *)redisClusterCommand(
        cc, "eval %s 2 %s %s", "return {KEYS[1],KEYS[2]}", "key1", "key1");
    CHECK_REPLY_ARRAY(cc, reply, 2);
    CHECK_REPLY_STR(cc, reply->element[0], "key1");
    CHECK_REPLY_STR(cc, reply->element[1], "key1");
    freeReplyObject(reply);

    // Error handling in EVAL

    // Request without key, fails due to no key given.
    reply = (redisReply *)redisClusterCommand(
        cc, "eval %s 0", "return redis.call('set','foo','bar')");
    assert(reply == NULL);

    // Prepare a key to be a list-type, then run a script the attempts
    // to access it as a simple type.
    reply = (redisReply *)redisClusterCommand(cc, "del foo");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "lpush foo a");
    CHECK_REPLY_INT(cc, reply, 1);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(
        cc, "eval %s 1 %s", "return redis.call('get',KEYS[1])", "foo");
    CHECK_REPLY_ERROR(cc, reply, "ERR Error running script");
    freeReplyObject(reply);

    // Two keys handled by different instances,
    // will be retried multiple times and fail due to CROSSSLOT.
    reply = (redisReply *)redisClusterCommand(
        cc, "eval %s 2 %s %s %s %s", "return {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}",
        "key1", "key2", "first", "second");
    assert(reply == NULL);
}

void test_xack(redisClusterContext *cc) {
    redisReply *r;

    /* Prepare a stream and group */
    r = redisClusterCommand(cc, "XADD mystream * field1 value1");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_STRING);
    freeReplyObject(r);
    r = redisClusterCommand(cc, "XGROUP DESTROY mystream mygroup");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);
    r = redisClusterCommand(cc, "XGROUP CREATE mystream mygroup 0");
    CHECK_REPLY_OK(cc, r);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XACK mystream mygroup 1526569495631-0");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);
}

void test_xadd(redisClusterContext *cc) {
    redisReply *r;

    r = redisClusterCommand(cc, "XADD mystream * field1 value1");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_STRING);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XADD mystream * field1 value1 field2 value2");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_STRING);
    freeReplyObject(r);

    r = redisClusterCommand(
        cc, "XADD mystream * field1 value1 field2 value2 field3 value3");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_STRING);
    freeReplyObject(r);
}

void test_xautoclaim(redisClusterContext *cc) {
    redisReply *r;

    r = redisClusterCommand(
        cc, "XAUTOCLAIM mystream mygroup Alice 3600000 0-0 COUNT 25");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xclaim(redisClusterContext *cc) {
    redisReply *r;

    r = redisClusterCommand(
        cc, "XCLAIM mystream mygroup Alice 3600000 1526569498055-0");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xdel(redisClusterContext *cc) {
    redisReply *r;
    char *id;

    r = redisClusterCommand(cc, "XADD mystream * field value");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_STRING);
    id = strdup(r->str); /* Keep the id */
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XDEL mystream %s", id);
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);

    /* Verify client handling of multiple id values / arguments */
    r = redisClusterCommand(cc, "XDEL mystream %s %s", id, id);
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);

    free(id);
}

void test_xgroup(redisClusterContext *cc) {
    redisReply *r;

    /* Test of missing subcommand */
    r = redisClusterCommand(cc, "XGROUP");
    assert(r == NULL);

    /* Test of missing stream/key */
    r = redisClusterCommand(cc, "XGROUP CREATE");
    assert(r == NULL);

    /* Test the destroy command first as preparation */
    r = redisClusterCommand(cc, "XGROUP DESTROY mystream consumer-group-name");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XGROUP CREATE mystream consumer-group-name 0");
    CHECK_REPLY_OK(cc, r);
    freeReplyObject(r);

    /* Attempting to create an already existing group gives error */
    r = redisClusterCommand(cc, "XGROUP CREATE mystream consumer-group-name 0");
    CHECK_REPLY_ERROR(cc, r, "BUSYGROUP");
    freeReplyObject(r);

    r = redisClusterCommand(
        cc, "XGROUP CREATECONSUMER mystream consumer-group-name myconsumer123");
    CHECK_REPLY_INT(cc, r, 1);
    freeReplyObject(r);

    r = redisClusterCommand(
        cc, "XGROUP DELCONSUMER mystream consumer-group-name myconsumer123");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XGROUP SETID mystream consumer-group-name 0");
    CHECK_REPLY_OK(cc, r);
    freeReplyObject(r);
}

void test_xinfo(redisClusterContext *cc) {
    redisReply *r;

    /* Test of missing subcommand */
    r = redisClusterCommand(cc, "XINFO");
    assert(r == NULL);

    /* Test of missing stream/key */
    r = redisClusterCommand(cc, "XINFO STREAM");
    assert(r == NULL);

    r = redisClusterCommand(cc, "XINFO STREAM mystream");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);

    /* Test of subcommand STREAM with arguments*/
    r = redisClusterCommand(cc, "XINFO STREAM mystream FULL COUNT 1");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XINFO GROUPS mystream");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XINFO CONSUMERS mystream consumer-group-name");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xlen(redisClusterContext *cc) {
    redisReply *r;

    r = (redisReply *)redisClusterCommand(cc, "XLEN mystream");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);
}

void test_xpending(redisClusterContext *cc) {
    redisReply *r;

    r = redisClusterCommand(cc, "XPENDING mystream mygroup");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XPENDING mystream mygroup - + 10");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_ARRAY);
    freeReplyObject(r);
}

void test_xrange(redisClusterContext *cc) {
    redisReply *r;

    r = redisClusterCommand(cc, "XRANGE mystream 0 0");
    CHECK_REPLY_ARRAY(cc, r, 0); /* No entries due to 0-range */
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XRANGE mystream - + COUNT 1");
    CHECK_REPLY_ARRAY(cc, r, 1); /* Single entry due to count argument */
    freeReplyObject(r);
}

void test_xrevrange(redisClusterContext *cc) {
    redisReply *r;

    r = redisClusterCommand(cc, "XREVRANGE mystream 0 0");
    CHECK_REPLY_ARRAY(cc, r, 0); /* No entries due to 0-range */
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XREVRANGE mystream + - COUNT 1");
    CHECK_REPLY_ARRAY(cc, r, 1); /* Single entry due to count argument */
    freeReplyObject(r);
}

void test_xtrim(redisClusterContext *cc) {
    redisReply *r;

    r = redisClusterCommand(cc, "XTRIM mystream MAXLEN 200");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);

    r = redisClusterCommand(cc, "XTRIM mystream MAXLEN ~ 100");
    CHECK_REPLY_TYPE(r, REDIS_REPLY_INTEGER);
    freeReplyObject(r);
}

int main() {
    struct timeval timeout = {0, 500000};

    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);
    redisClusterSetOptionAddNodes(cc, CLUSTER_NODE);
    redisClusterSetOptionConnectTimeout(cc, timeout);

    int status;
    status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    test_exists(cc);
    test_mset(cc);
    test_mget(cc);
    test_hset_hget_hdel_hexists(cc);
    test_eval(cc);

    test_xack(cc);
    test_xadd(cc);
    test_xautoclaim(cc);
    test_xclaim(cc);
    test_xdel(cc);
    test_xgroup(cc);
    test_xinfo(cc);
    test_xlen(cc);
    test_xpending(cc);
    test_xrange(cc);
    test_xrevrange(cc);
    test_xtrim(cc);

    redisClusterFree(cc);
    return 0;
}
