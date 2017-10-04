#include "hiredis.h"
#include "async.h"

/* Compatible with C */
#ifdef __cplusplus
extern "C" {
#endif
/* define max timeout as 10 sec*/
#define MAX_CONNECT_TIMEOUT (10000000)
/* Error code */
#define BDRP_OK (0)
#define BDRP_ERR (-1)

/* Log level */
enum bdrpLogLevel{
    LOG_DEBUG = 0,
    LOG_WARNING,
    LOG_ERROR
};

/* Local Cache BNS interval time, the default is 100ms */
void setbnscachetime (int time_ms);

/* Set Log info */
int setlogconf(char *logpath, int loglevel);

/* Function to get connection from bdrp */
redisContext *bdrpconnect(const char *bnsname, int max_retry, int timeout);

/* Function to get Asynchronous connection */
redisAsyncContext *bdrpasyncconnect(const char *bnsname, int max_retry);

#ifdef __cplusplus
};
#endif
