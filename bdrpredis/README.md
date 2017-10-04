### 关于bdrpredis
这是基于hiredis和BNS（Baidu Naming Service）封装了访问Redis的客户端。BNS是公司内部对机器资源进行管理的工具。我们在这里用到的是其资源定位方面的能力。
类似于DNS，BNS可以提供名字到一个或多个内网IP（机器）的映射。因此，我们封装出的新客户端，不仅继承了hiredis原有的与服务端交互的方式（IP+Port），还新增了几点功能：
* 使用BNS与服务端Redis进行交互
* 故障剔除，利用BNS的能力屏蔽故障实例（机器）
* 负载均衡，随机访问BNS中的实例，以平衡服务端负载

注：由于BNS属于公司内部所有，不做介绍，也不包含其代码。这里仅仅用于分享一些思路及记录部分代码。无法在外部真正使用。

### 使用说明
头文件说明
```C
/* 状态宏定义 */
#define BDRP_OK (0)
#define BDRP_ERR (-1)
 
/* 日志等级 */
enum bdrpLogLevel{
    LOG_DEBUG = 0,
    LOG_WARNING,
    LOG_ERROR //bns解析错误、连接错误、请求错误
};
 
/* BNS进程缓存时间，单位是ms，默认是100ms */
void setbnscachetime(int time_ms);
 
/* 设置日志路径及日志等级，设置成功返回BDRP_OK（0）,错误返回BDRP_ERR(-1) */
/* 不调用该函数或者传入的日志等级为空，视为关闭该客户端打日志功能 */
/* 等级参照上述日志等级，等级向上兼容，例如选择LOG_WARNING,则LOG_ERROR的错误也会打印log */
int setlogconf(char *logpath, int loglevel);
 
/* bns连接接口函数，返回均为hiredis原生context */
 
/* 同步连接获取函数，bnsname为bdrp同学提供的proxy bns，maxRetry为io重试次数，timeout为连接超时时间（单位为微秒） */
redisContext *bdrpconnect(const char *bnsname, int maxRetry, int timeout);
 
/* 异步连接获取函数，参数含义同上 */
redisAsyncContext *bdrpasyncconnect(const char *bnsname, int maxRetry);
 
/* 基于hiredis原生command方法封装出io失败重试专用接口*/
/* 新增了max_retry参数，用户可传入重试次数值（0到默认值=10），io失败时将进行重试，最多重试max_retry次 */
void *rediscommandwithretry(redisContext *c, int max_retry, const char *format, ...);
void *rediscommandargvwithretry(redisContext *c, int max_retry, int argc, const char **argv, const size_t *argvlen);
```
注：其他原生hiredis方法，依然兼容。
