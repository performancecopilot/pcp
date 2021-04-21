#include "hiredis_cluster/adapters/libevent.h"
#include "hiredis_cluster/hircluster.h"
#include <stdio.h>
#include <stdlib.h>

void getCallback(redisClusterAsyncContext *cc, void *r, void *privdata) {
    redisReply *reply = (redisReply *)r;
    if (reply == NULL) {
        if (cc->errstr) {
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
        if (cc->errstr) {
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
    printf("Connecting...\n");
    redisClusterAsyncContext *cc =
        redisClusterAsyncConnect("127.0.0.1:7000", HIRCLUSTER_FLAG_NULL);
    if (cc && cc->err) {
        printf("Error: %s\n", cc->errstr);
        return 1;
    }

    struct event_base *base = event_base_new();
    redisClusterLibeventAttach(cc, base);
    redisClusterAsyncSetConnectCallback(cc, connectCallback);
    redisClusterAsyncSetDisconnectCallback(cc, disconnectCallback);

    int status;
    status = redisClusterAsyncCommand(cc, setCallback, (char *)"THE_ID",
                                      "SET %s %s", "key", "value");
    if (status != REDIS_OK) {
        printf("error: err=%d errstr=%s\n", cc->err, cc->errstr);
    }

    status = redisClusterAsyncCommand(cc, getCallback, (char *)"THE_ID",
                                      "GET %s", "key");
    if (status != REDIS_OK) {
        printf("error: err=%d errstr=%s\n", cc->err, cc->errstr);
    }

    status = redisClusterAsyncCommand(cc, setCallback, (char *)"THE_ID",
                                      "SET %s %s", "key2", "value2");
    if (status != REDIS_OK) {
        printf("error: err=%d errstr=%s\n", cc->err, cc->errstr);
    }

    status = redisClusterAsyncCommand(cc, getCallback, (char *)"THE_ID",
                                      "GET %s", "key2");
    if (status != REDIS_OK) {
        printf("error: err=%d errstr=%s\n", cc->err, cc->errstr);
    }

    printf("Dispatch..\n");
    event_base_dispatch(base);

    printf("Done..\n");
    redisClusterAsyncFree(cc);
    event_base_free(base);
    return 0;
}
