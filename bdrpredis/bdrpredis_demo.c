#include <stdio.h>
#include "bdrpredis.h"
#include "libevent.h"
#include "async.h"

/* define  Callback function */
void connectcallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("get AsyncConnect Error: %s\n", c->errstr);
        return;
    }
    printf("Async Connected...\n");
    return;
}
void disconnectcallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("disconnect AsyncConnect Error: %s\n", c->errstr);
        return;
    }
    printf("Async Disconnected...\n");
    return;
}
void getcallback(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = r;
    if (reply == NULL) {
        return;
    }
    printf("argv[%s]: %s\n", (char*)privdata, reply->str);

    redisAsyncDisconnect(c);

}

/* excute command with redisContext */
int setcommand(redisContext* context, char *cmd)
{
    if (NULL == context) {
        printf("parsed context is NULL\n");
        return BDRP_ERR;
    }

    redisReply *reply = redisCommand(context, cmd);
    
    if (NULL == reply) {
        printf("get reply NULL\n");
        return BDRP_ERR;
    }
    
    printf("set Command and return : %s\n", reply->str);
    
    freeReplyObject(reply);
    reply = NULL;

    return BDRP_OK;
}

/* function test redisAsyncContext which use libevent and excute command */
int proc_async(char* bnsname)
{
    struct event_base *base = event_base_new();

    if (NULL == base) {
        printf("event_base creat failed\n");
        return BDRP_ERR;
    }

    redisAsyncContext *ac = NULL;
    ac = bdrpasyncconnect(bnsname, 3);

    if (NULL == ac) {
        printf("async context get failed\n");
        return BDRP_ERR;
    }
    
    redisLibeventAttach(ac, base);
    redisAsyncSetConnectCallback(ac, connectcallback);
    redisAsyncSetDisconnectCallback(ac, disconnectcallback);
   
    redisAsyncCommand(ac, NULL, NULL, "SET async_test_key async_test");
    redisAsyncCommand(ac, getcallback, (char*)"end-1", "GET async_test_key");
    event_base_dispatch(base);

    /* release async context */
    if (NULL != ac) {
        ac = NULL;
    }

    return BDRP_OK;
}

/* function test redisContext and command */
int proc_sync(char* bnsname)
{
    /* test redisContext and Command */
    redisContext *c = NULL;
    char cmd[] = "set context sync_test";
    c = bdrpconnect(bnsname, 3, 0);
    if (NULL == c) {
        printf("get context failed\n");
        return BDRP_ERR;
    }
    if (BDRP_OK != setcommand(c, cmd)) {
        printf("set command failed\n");
        redisFree(c);
        c = NULL;
        return BDRP_ERR;
    }

    /* release context */
    redisFree(c);
    c = NULL;

    /* test redisContext with timeout */
    c = bdrpconnect(bnsname, 3, 60000);
    if (NULL == c) {
        printf("get context with timeout failed\n");
        return BDRP_ERR;
    }
    if (BDRP_OK != setcommand(c, cmd)) {
        printf("set command with timeout context failed\n");
        redisFree(c);
        c = NULL;
        return BDRP_ERR;
    }
    redisFree(c);
    c = NULL;

    return BDRP_OK;
}

/* function to test setlogconf */
void proc_setlogconf(char *logpath)
{
    if (BDRP_OK != setlogconf(logpath, LOG_ERROR)) {
        printf("set log conf error\n");
    }
    return;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: %s <bnsname> <logpath>\n", argv[0]);
        exit(1);
    }
    char *bnsname = argv[1];
    char *logpath = argv[2];
    
    /* test setlogconf */
    proc_setlogconf(logpath);
    
    /* test setBnsCacheTime */
    setbnscachetime(1000);

    /* test sync context and commadn */
    if (BDRP_OK != proc_sync(bnsname)) {
        printf("test sync context failed\n");
        return -1;
    }
    /* test async context and commadn */
    if (BDRP_OK != proc_async(bnsname)) {
        printf("test async context failed\n");
        return -1;
    }
    return 0;
}
