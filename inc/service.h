#ifndef __SERVICE_H__
#define __SERVICE_H__
#include "http.h"
#include "service_user.h"

typedef HTTP_CODE (*SERVICE_FUNC)(const struct http_req* req, struct http_resp* resp); // 标准服务函数定义
struct http_service
{
    const char* service_path;
    const char* next_path;
    const SERVICE_FUNC serve;
};
/* 将服务函数与对应的服务 URI 成对地添加到 services[] 数组中，然后实现服务函数即可 */
static const struct http_service services[] = 
{
    {"service/putInfo", NULL, service_putInfo},
};
#define HTTP_SERVICE_NUM (sizeof(services) / sizeof(struct http_service))

const struct http_service* http_service_findServiceEntry(const char* resource);

#endif