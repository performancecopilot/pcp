#include "hircluster.h"
#include "test_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_NODE_IPV6 "::1:7200"

// Successful connection an IPv6 cluster
void test_successful_ipv6_connection() {

    redisClusterContext *cc = redisClusterContextInit();
    assert(cc);

    int status;
    struct timeval timeout = {0, 500000}; // 0.5s
    status = redisClusterSetOptionConnectTimeout(cc, timeout);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterSetOptionAddNodes(cc, CLUSTER_NODE_IPV6);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterSetOptionRouteUseSlots(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    status = redisClusterConnect2(cc);
    ASSERT_MSG(status == REDIS_OK, cc->errstr);

    redisReply *reply;
    reply = (redisReply *)redisClusterCommand(cc, "SET key_ipv6 value");
    CHECK_REPLY_OK(cc, reply);
    freeReplyObject(reply);

    reply = (redisReply *)redisClusterCommand(cc, "GET key_ipv6");
    CHECK_REPLY_STR(cc, reply, "value");
    freeReplyObject(reply);

    redisClusterFree(cc);
}

int main() {

    test_successful_ipv6_connection();

    return 0;
}
