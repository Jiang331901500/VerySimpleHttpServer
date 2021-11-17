#ifndef __SERVICE_USER_H__
#define __SERVICE_USER_H__
#include "http.h"

/** 所有的 service 函数在此声明，然后再 service_user.c中实现 **/
HTTP_CODE service_putInfo(const struct http_req* req, struct http_resp* resp);

#endif