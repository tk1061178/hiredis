#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bdrpredis_inner.h"
#include "bdrpredis.h"
#include "webfoot_naming.h"

/* timeout for get bns by network */
const int TIMEOUT_GETBNS = 3000;
/* max retry times */
const int MAX_RETRY_TIMES = 10;

struct timeval g_total_rsp_time = {0, 0};

/* log conf */
int g_bdrplog_level = LOG_ERROR;
char *g_bdrplog_path = NULL;

/* Cache BNS interval time, the default is 100ms */
void setbnscachetime (int time_ms) {
    webfoot::set_local_cache_update_interval_ms_min(time_ms);
    return;
}

/* Set Log info */
int setlogconf(char *logpath, int loglevel) {
    /* if logpath is NULL, close log */
    if (NULL == logpath) {
        if (NULL != g_bdrplog_path) {
            bdrplog(LOG_WARNING, "User set logpath to NULL, close bdrp log");
            free(g_bdrplog_path);
            g_bdrplog_path = NULL;
        }
        return BDRP_OK;
    }
    /* if bdrp_log_path exist, compare if they are the same  */
    if (NULL != g_bdrplog_path) {
        if (0 == strcmp(logpath, g_bdrplog_path)) {
            g_bdrplog_level = loglevel;
            return BDRP_OK;
        }else{
            free(g_bdrplog_path);
            g_bdrplog_path = NULL;
        }
    }
    /* malloc for new path */
    g_bdrplog_path = (char*)malloc(strlen(logpath) + 1);
    if (NULL == g_bdrplog_path) {
        return BDRP_ERR;
    }

    memcpy(g_bdrplog_path, logpath, strlen(logpath) + 1);
    g_bdrplog_level = loglevel;
    return BDRP_OK;
}

void bdrplog(int log_level, const char *fmt, ...) {
    if (NULL == g_bdrplog_path || log_level < g_bdrplog_level) {
        return;
    }
    va_list ap;
    char msg[BDRP_MAX_LOGMSG_LEN] = {0};
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    FILE *fp = NULL;
    fp = fopen(g_bdrplog_path, "a");
    if (!fp) { 
        return;
    }
    struct timeval tv;
    char buf[64] = {0};
    int off = 0;
    gettimeofday(&tv, NULL);
    off = strftime(buf, sizeof(buf), "%d %b %H:%M:%S.", localtime(&tv.tv_sec));
    snprintf(buf + off, sizeof(buf) - off, "%03d", (int)tv.tv_usec / 1000);
    fprintf(fp, "%s %s\n", buf, msg);
    
    fflush(fp);
    if (fp) { 
        fclose(fp);
    }
    return;
}

/* get random host and port from bns */
int getrandomredisaddr(const char *bnsname, redisAddr *addr) {
    int ret = 0;
    std::list<webfoot::InstanceInfo> bnslist;
    if (NULL == bnsname) {
        /* error log */
        bdrplog(LOG_ERROR, "[bns_error] BnsName is empty");
        return BDRP_ERR;
    }
    if (strlen(bnsname) > MAX_BNS_LEN) {
        bdrplog(LOG_ERROR, "[bns_error] BnsName %s out of limit lens of %d", bnsname, MAX_BNS_LEN);
        return BDRP_ERR;
    }
    ret = webfoot::get_instance_by_service(bnsname, &bnslist, TIMEOUT_GETBNS);
    if (0 != ret) {
        /* error log */
        bdrplog(LOG_ERROR, "[bns_error] %s bns error code:%d %s\n", bnsname, ret, webfoot::error_to_string(ret));
        return BDRP_ERR;
    }
    /* record original host number of bns*/
    int total_host = bnslist.size();
    for (std::list<webfoot::InstanceInfo>::iterator it = bnslist.begin();it != bnslist.end();){
        if (0 != it->status()) {
            it = bnslist.erase(it); //list erase() return the next iterator
        }else{
            ++it;
        }
    }
    if (0 == bnslist.size()) {
        /* error log */
        bdrplog(LOG_ERROR, "[bns_error] Can't get valid host from %s and total host num %d", bnsname, total_host);
        return BDRP_ERR;
    }
    
    std::list<webfoot::InstanceInfo>::iterator it = bnslist.begin();
    srand((unsigned)time(NULL));
    std::advance(it, rand() % bnslist.size());
    
    int host_len = it->host_ip_str().length() + 1;
    if (host_len > MAX_IP_LEN) {
        bdrplog(LOG_ERROR, "[sys_error] ip length over max limit %d", MAX_IP_LEN);
        return BDRP_ERR;
    }
    if (0 > snprintf(addr->redis_ip, host_len, "%s", it->host_ip_str().c_str())) {
        bdrplog(LOG_ERROR, "[sys_error] Copy bdrp host failed from %s", bnsname);
        return BDRP_ERR;
    }
    addr->redis_ip[host_len] = '\0';
    addr->redis_port = it->port();
    
    return BDRP_OK;
}

/* Use bns get random host and return redisContext */
redisContext *bdrpconnect(const char *bnsname, int max_retry, int timeout) {
    redisContext *context = NULL; 
    redisAddr addr;
    /* out of limit with bad timeout */
    if (timeout > MAX_CONNECT_TIMEOUT || timeout < 0) {
        bdrplog(LOG_ERROR, "[bdrp_error] Timeout not range of 0 to %d us", MAX_CONNECT_TIMEOUT);
        return NULL;
    }
    if (max_retry > MAX_RETRY_TIMES) {
        bdrplog(LOG_ERROR, "[bdrp_error] max retry times out of limit of %d", MAX_RETRY_TIMES);
        return NULL;
    }
    /* get random host ip and port */
    if (BDRP_ERR == getrandomredisaddr(bnsname, &addr)) {
        bdrplog(LOG_ERROR, "[bdrp_error] Can't get bdrp host from %s", bnsname);
        return NULL;
    }
    if (timeout <= 0) {
        context = redisConnect(addr.redis_ip, addr.redis_port);
    }else{
        struct timeval tv;
        tv.tv_sec = timeout/1000000;
        tv.tv_usec = timeout%1000000;
        context = redisConnectWithTimeout(addr.redis_ip, addr.redis_port, tv);
    }
    if (NULL == context) {
        bdrplog(LOG_ERROR, "[sys_error] Connection error: can't allocate redis context\n");
        return NULL;
    }
    if (context->err) {
        bdrplog(LOG_ERROR, "[bdrp_error] Connection %s:%d error: %s\n", addr.redis_ip, addr.redis_port, context->errstr);
        redisFree(context);
        return NULL;
    }
    return context;
}

/* Use bns get random host and return redisAsyncContext */
redisAsyncContext *bdrpasyncconnect(const char *bnsname, int max_retry) {
    redisAsyncContext *context = NULL;
    redisAddr addr;

    if (BDRP_ERR == getrandomredisaddr(bnsname, &addr)) {
        bdrplog(LOG_ERROR, "[bdrp_error] Can't get bdrp host from %s", bnsname);
        return NULL;
    }
    if (max_retry > MAX_RETRY_TIMES) {
        bdrplog(LOG_ERROR, "[bdrp_error] max retry times out of limit of %d", MAX_RETRY_TIMES);
        return NULL;
    }
    
    context = redisAsyncConnect(addr.redis_ip, addr.redis_port);

    if (NULL == context) {
        bdrplog(LOG_ERROR, "[sys_error] Connection error: can't allocate redis context\n");
        return NULL;
    }
    if (context->err) {
        bdrplog(LOG_ERROR, "[bdrp_error] Connection %s:%d error: %s\n", addr.redis_ip, addr.redis_port, context->errstr);
        redisAsyncFree(context);
        return NULL;
    }
    return context;
}
