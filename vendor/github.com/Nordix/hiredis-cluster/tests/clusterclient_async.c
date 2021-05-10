/*
 * This program connects to a cluster and then reads commands from stdin, such
 * as "SET foo bar", one per line and prints the results to stdout.
 *
 * The behaviour is the same as that of clusterclient.c, but the asynchronous
 * API of the library is used rather than the synchronous API.
 */

#include "adapters/libevent.h"
#include "hircluster.h"
#include "test_utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int num_running = 0;

/*
void printReply(redisReply *reply) {
    switch (reply->type) {
    case REDIS_REPLY_INTEGER: printf("%lld", reply->integer); break;
    case REDIS_REPLY_DOUBLE:  printf("%s", reply->str); break;
    case REDIS_REPLY_ERROR:   printf("-%s", reply->str); break;
    // TODO: Escape special chars in strings
    case REDIS_REPLY_STRING:  printf("\"%s\"", reply->str); break;
    case REDIS_REPLY_ARRAY:
        printf("[");
        for (size_t i = 0; i < reply->elements; i++) {
            printReply(reply->element[i]);
            if (i < reply->elements - 1)
                printf(", ");
        }
        printf("]");
        break;
    default:
        printf("UNKNOWN TYPE %d", reply->type);
    }
}
*/

void replyCallback(redisClusterAsyncContext *acc, void *r, void *privdata) {
    UNUSED(privdata);
    redisReply *reply = (redisReply *)r;
    ASSERT_MSG(reply != NULL, acc->errstr);

    /* printReply(reply); */
    /* printf("\n"); */
    printf("%s\n", reply->str);

    if (--num_running == 0) {
        // Disconnect after receiving all replies
        redisClusterAsyncDisconnect(acc);
    }
}

void connectCallback(const redisAsyncContext *ac, int status) {
    ASSERT_MSG(status == REDIS_OK, ac->errstr);
    // printf("Connected to %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

void disconnectCallback(const redisAsyncContext *ac, int status) {
    ASSERT_MSG(status == REDIS_OK, ac->errstr);
    // printf("Disconnected from %s:%d\n", ac->c.tcp.host, ac->c.tcp.port);
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        fprintf(stderr, "Usage: clusterclient_async HOST:PORT\n");
        exit(1);
    }
    const char *initnode = argv[1];

    redisClusterAsyncContext *acc = redisClusterAsyncContextInit();
    assert(acc);
    redisClusterAsyncSetConnectCallback(acc, connectCallback);
    redisClusterAsyncSetDisconnectCallback(acc, disconnectCallback);
    redisClusterSetOptionAddNodes(acc->cc, initnode);
    redisClusterSetOptionRouteUseSlots(acc->cc);
    redisClusterConnect2(acc->cc);
    if (acc->err) {
        printf("Connect error: %s\n", acc->errstr);
        exit(-1);
    }

    int status;
    struct event_base *base = event_base_new();
    status = redisClusterLibeventAttach(acc, base);
    assert(status == REDIS_OK);

    // Forward commands from stdin to redis cluster
    char command[256];

    // Make sure num_running doesn't reach 0 in replyCallback() before all
    // commands have been sent.
    num_running++;

    while (fgets(command, 256, stdin)) {
        size_t len = strlen(command);
        if (command[len - 1] == '\n') // Chop trailing line break
            command[len - 1] = '\0';
        status =
            redisClusterAsyncCommand(acc, replyCallback, (char *)"ID", command);
        ASSERT_MSG(status == REDIS_OK, acc->errstr);
        num_running++;
    }
    num_running--; // all commands sent

    event_base_dispatch(base);

    redisClusterAsyncFree(acc);
    event_base_free(base);
    return 0;
}
