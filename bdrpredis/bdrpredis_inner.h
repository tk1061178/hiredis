#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>

/* The max length of bnsname */
const int MAX_BNS_LEN = 32;
/* Default maximum length of bdrplog messages */
const int BDRP_MAX_LOGMSG_LEN = 1024;
/* Max ip len  */
const int MAX_IP_LEN = 32 + 2;

/* Record response time for all requests */
extern struct timeval g_total_rsp_time;
extern int g_bdrplog_level;
extern char* g_bdrplog_path;

typedef struct redisAddr {
    char redis_ip[MAX_IP_LEN];
    int redis_port;
} redisAddr;

/* bdrp log function */
#ifdef __GNUC__
void bdrplog(int log_level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
void bdrplog(int log_level, const char *fmt, ...);
#endif

/* get random host and port from bns */
int getrandomredisaddr(const char *bnsname, char *redisAddr);
