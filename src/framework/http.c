#include "http.h"
#include "service.h"


struct http_err_msg
{
    int code;
    char* msg;
};
static struct http_err_msg http_errmsg[] = 
{
    {HTTP_CODE_OK, "OK"},
	{HTTP_CODE_NOTFOUND, "Not Found"},
	{HTTP_CODE_INTERNAL_SERVER_ERROR, "Internal Server Error"},
	{HTTP_CODE_METHOD_NOT_ALLOWED, "Method Not Allowed"},
    {HTTP_CODE_NOT_ACCEPTABLE, "Not Acceptable"},
};
#define HTTP_ERR_TYPE_NUM (sizeof(http_errmsg) / sizeof(struct http_err_msg))

static struct filetype_pair filetypes[] =   // 此处增加新的类型后，要记得按顺序增加 HTTP_CONTENT_TYPE 枚举类的成员
{
	{HTTP_CONTENT_TYPE_HTML, ".html", "text/html;charset="HTTP_CHAR_SET},
	{HTTP_CONTENT_TYPE_JPG, ".jpg", "image/jpeg"},
	{HTTP_CONTENT_TYPE_PDF, ".pdf", "application/pdf"},
    {HTTP_CONTENT_TYPE_ICO, ".ico", "image/x-icon"}
};
#define HTTP_FILE_TYPE_NUM (sizeof(filetypes) / sizeof(struct filetype_pair))

enum HTTP_METHOD
{
    HTTP_METHOD_NOTALLOWED = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_PUT,
    HTTP_METHOD_POST,
};

/* 把文件名拼接到目录后面 */
static inline int http_getFilePath(char* path, int maxLen, char* resourcename)
{
    if(path == NULL || resourcename == NULL)
        return -1;
    return snprintf(path, maxLen, "%s%s", HTTP_RESRC_ROOTDIR, resourcename);
}

static inline char* http_getContentTypeByEnum(const HTTP_CONTENT_TYPE typenum)
{
    return (0 <= typenum && typenum < HTTP_FILE_TYPE_NUM) ? filetypes[typenum].content_type : NULL;
}

static char* http_getContentTypeBySuffix(const char* suffix)
{
    int i;
    for(i = 0; i < HTTP_FILE_TYPE_NUM; i++)
    {
        if(strcmp(suffix, filetypes[i].file_suffix) == 0)
            return filetypes[i].content_type;
    }
    return NULL;    // 没有匹配的类型则返回NULL
}
/* 从请求文件名末尾提取其后缀，再根据后缀去匹配相应的 Content-Type */
static char* http_getContentTypeByResourceName(const char* _resource)
{
    char* resource = strrchr(_resource, '.');
    if(!resource)
        return NULL;
    return http_getContentTypeBySuffix(resource);
}

/* 提取从'/'开始之后资源名，以空格作为结束 */
static int http_getResourceName(char* buffer, int buflen, char* input)
{
    if(buffer == NULL || input == NULL)
        return -1;

    char* p = input;
    int bufIdx = 0;

    while(*p && *p != ' ' && bufIdx < buflen)
    {
        buffer[bufIdx] = *p;
        p++;
        bufIdx++;
    }
    if(*p == '\0' || bufIdx == buflen)
    {
        buffer[0] = '\0';
        return -1;
    }

    buffer[bufIdx] = '\0';

    return bufIdx;
}

/* 判断HTTP头部是都都接收完了 */
static inline char* http_isHeaderOver(http_service_interface *http)
{
    if(http == NULL) return NULL;
    return strstr(http->req.header, HTTP_HEADER_END);
}

/* 获取 string 型头部字段 */
static int http_getHeaderValueString(const char* buffer, const char* name, char* value)
{
    char* tmp;
    char linebuf[256];
    int level = 0;

    do
    {
        memset(linebuf, 0, 256);
        level = readline(buffer, level, linebuf);
        if(strlen(linebuf) == 0)    // 读到空行则http头部全部解析完毕
            break;

        if((tmp = strstr(linebuf, name)))
        {
            if((tmp = strchr(tmp, ':')))
            {
                tmp++;   // 跳过':'
                tmp = trim(tmp);  // 消除首尾空格
                strcpy(value, tmp);
                return strlen(value); // 找到返回值的长度
            }
        }
    } while (level != -1);
    
    return -1; // 没找到则返回-1
}

/* 获取 int 型头部字段 */
static int http_getHeaderValueInt(const char* buffer, const char* name, int* value)
{
    if(value == NULL) return -1;

    char valueString[128] = {0};
    int ret = http_getHeaderValueString(buffer, name, valueString);
    if(ret < 0) return -1;

    *value = atoi(valueString);
    return 0;
}

/* 解析消息头，获取方法、URI以及需要的头部字段 */
static int http_resolveReqHeader(http_service_interface *http)
{
    if(http == NULL) return -1;

    char linebuf[256];
    char* value;
    int level = 0;
    int ret;

    /* 1 - 从第1行提取出http方法*/
    memset(linebuf, 0, 256);
    level = readline(http->req.header, level, linebuf);
    if(strstr(linebuf, "GET"))
        http->req.method = HTTP_METHOD_GET;
    else if(strstr(linebuf, "POST"))
        http->req.method = HTTP_METHOD_POST;
    else if(strstr(linebuf, "PUT"))
        http->req.method = HTTP_METHOD_PUT;
    else
        http->req.method = HTTP_METHOD_NOTALLOWED;    // 从第一行提取不到方法或不支持的方法

    /* 2 - 尝试提取请求的资源路径 */
    if((value = strchr(linebuf, '/')))
    {
        memset(http->req.resource, 0, HTTP_MAX_RESOURCE_NAME);
        ret = http_getResourceName(http->req.resource, HTTP_MAX_RESOURCE_NAME, value + 1);
        if(ret >= 0) printf("# resource requested[%d]: %s\n", ret, http->req.resource);
    }

    /******* 3 - 提取可能有用的头部字段 *******/ // this piece of code is awful, but I will improve it
    /* 都需要提取哪些头部字段目前通过手动加到这里 */
    // 3.1 尝试提取 ws 的key字段，后面依次判断是否进行 ws 握手
    ret = http_getHeaderValueString(http->req.header, "Sec-WebSocket-Key", http->ws.ws_key);
    if(ret >= 0) printf("# WS key[%d]: %s\n\n", ret, http->ws.ws_key);

    // 3.2 尝试提取 Content-Length 字段
    if(http->req.method != HTTP_METHOD_GET)
    {
        ret = http_getHeaderValueInt(http->req.header, "Content-Length", &http->req.contentlength);
        if(ret >= 0 && http->req.contentlength > 0) 
        {   // 根据消息体长度申请一块堆内存来单独存放消息体，此处暂不管申请是否成功
            http->req.body = (char*)malloc(http->req.contentlength + 1); // 加1若需要的话作为结束符
            if(http->req.body) 
                memset(http->req.body, 0, http->req.contentlength + 1);

            printf("# Content Length[%d]: %d\n\n", ret, http->req.contentlength);
        }
    }
    /****************************************/
    return 0;
}

/* 确保将所有头部信息读出，不允许超出最大范围 
 * 返回值: 0 - 已成功读到所有头部；1 - 未完全读完头部，还需再继续读；-1 - 头部长度超出了允许范围了，出错
*/
static int http_readAllHeader(http_service_interface *http)
{
    if(http == NULL) return -1;
    char* endOfHeader = NULL;
    int resLen = 0;

    if(http->helper.recvHeaderLen + http->buffers.recvLength < HTTP_MAX_HEADER_LEN) 
    {   // 未超出范围则拷贝已读到的数据
        resLen = http->buffers.recvLength;
        memcpy(http->req.header + http->helper.recvHeaderLen, http->buffers.recvBuffer, resLen);
        http->helper.recvHeaderLen += resLen;
        /* 判断消息头是否都已读完*/
        endOfHeader = http_isHeaderOver(http);
        if(!endOfHeader)
            return 1;   // 当前消息头还未读完，下一次触发读完后再做后续处理
    }
    else
    {   // 超出范围则拷贝剩余空间还能填入数据
        resLen = HTTP_MAX_HEADER_LEN - 1 - http->helper.recvHeaderLen;
        memcpy(http->req.header + http->helper.recvHeaderLen, http->buffers.recvBuffer, resLen);
        http->helper.recvHeaderLen += resLen;
        /* 判断消息头是否都已读完*/
        endOfHeader = http_isHeaderOver(http);
        if(!endOfHeader)
            return -1;  // 都已经读到允许的最大头部长度了还没读完头部，不允许再读了，返回错误
    }
    int offset = endOfHeader - (http->req.header + http->helper.recvHeaderLen - resLen);
    http->helper.startOfBody = http->buffers.recvBuffer + offset + strlen(HTTP_HEADER_END);
    return 0;
}

static int http_packErrorResponse(char* buffer, int maxLen, int retcode)
{
    int ret = 0;
    int i;
    char* msg = NULL;
    char gmttime[64] = {0};
    char body[256] = {0};

    getGmtTime(gmttime, sizeof(gmttime) - 1);

    for(i = 0; i < HTTP_ERR_TYPE_NUM; i++)
        if(http_errmsg[i].code == retcode) msg = http_errmsg[i].msg;

    ret = sprintf(body, "<html><head><title>%d %s</title></head><body><p style=\"text-align: center;font-size:100px\">%d</p></body></html>\r\n\r\n", 
                        retcode, (msg ? msg : ""), retcode);

    ret = snprintf(buffer, maxLen,
			"HTTP/1.1 %d %s\r\n"
		 	"Date: %s\r\n"
		 	"Content-Type: text/html;charset=%s\r\n"
			"Content-Length: %d\r\n\r\n"
		 	"%s",

             retcode, (msg ? msg : ""), 
             gmttime, 
             HTTP_CHAR_SET, 
             ret,
             body );

    return ret;
}

static int http_packOKRespHead(char* buffer, const char* contentType, int contentLength)
{
    int ret = 0;
    char gmttime[64] = {0};

    getGmtTime(gmttime, sizeof(gmttime) - 1);
    ret = sprintf(buffer, 
				"HTTP/1.1 200 OK\r\n"
			 	"Date: %s\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: %d\r\n\r\n", 
			 	gmttime,
                contentType ? contentType : "application/octet-stream", 
                contentLength );

    return ret;
}

static int http_request_get(http_service_interface *http)
{
    // 这里暂时什么也不用做
    return 0;
}

static HTTP_CODE http_response_post(http_service_interface *http)
{
    HTTP_CODE code;
    int ret = 0;

    const struct http_service* service = http_service_findServiceEntry(http->req.resource);
    if(service)
        code = service->serve(&http->req, &http->resp);
    else
        return HTTP_CODE_NOTFOUND;  // 错误报文在外面统一处理

    ret = http_packOKRespHead(http->buffers.sendBuffer, http_getContentTypeByEnum(http->resp.contentType), http->resp.contentLength);
    send(http->sockfd, http->buffers.sendBuffer, ret, 0);
    if(http->resp.body && http->resp.contentLength > 0)
    {
        ret = send(http->sockfd, http->resp.body, http->resp.contentLength, 0);
        printf("# resp body[%d]: %s\n", ret, http->resp.body);
    }

    return code;
}

static HTTP_CODE http_response_get(http_service_interface *http)
{
    int ret = 0;
    int fd = -1;

    if(strlen(http->req.resource) == 0)   // 资源为空表示请求根目录，则返回 index.html
        strcpy(http->req.resource, HTTP_INDEX_PAGE);

    char filepath[HTTP_MAX_RESOURCE_NAME] = {0};
    http_getFilePath(filepath, HTTP_MAX_RESOURCE_NAME - 1, http->req.resource); // 将资源名加上目录构成本地的文件路径
    fd = open(filepath, O_RDONLY);
    if(fd < 0)
    {   // 没有这个文件或目录，注意open以只读方式可以打开目录的
        return HTTP_CODE_NOTFOUND;
    }

    struct stat filestat;
    fstat(fd, &filestat);
    if(S_ISREG(filestat.st_mode))   // 如果是普通文件，则返回 200 并发送文件
    {
        char* contentType = http_getContentTypeByResourceName(http->req.resource);  // 获取文件类型
        ret = http_packOKRespHead(http->buffers.sendBuffer, contentType, filestat.st_size);
        ret = send(http->sockfd, http->buffers.sendBuffer, ret, 0);                 // 先发头部
        ret = sendWholeFile(http->sockfd, fd, filestat.st_size);        // 再发文件内容
    }
    else
    {   // 其他类型的资源不允许访问，返回 404
        return HTTP_CODE_NOTFOUND;
    }

    if(fd > 0)
        close(fd);

    return HTTP_CODE_OK;
}

static int http_request_post_put(http_service_interface *http)
{
    if(http->req.contentlength >= HTTP_MAX_BODY_LEN)
    {
        http->resp.code = HTTP_CODE_NOT_ACCEPTABLE;
        return -1;
    }
    else if(http->req.contentlength > 0)  // POST 有消息体
    {
        if(http->req.body != NULL && http->helper.startOfBody != NULL)
        {
            int resBodyLen = http->buffers.recvBuffer + http->buffers.recvLength - http->helper.startOfBody; // 剩下可读的消息体长度
            memcpy(http->req.body + http->helper.recvBodyLen, http->helper.startOfBody, resBodyLen);
            http->helper.recvBodyLen += resBodyLen;
            if(http->helper.recvBodyLen < http->req.contentlength)
            {   
                http->helper.startOfBody = http->buffers.recvBuffer;
                return 1;
            }
            http->req.body[http->req.contentlength] = '\0';
            http->helper.startOfBody = NULL;
        }
        else
        {   // 有消息体长度但之前却没有成功申请到足够的内存，返回内部错误
            http->resp.code = HTTP_CODE_INTERNAL_SERVER_ERROR;
        }
    }
    // 若 POST 没有消息体则不需要处理
    return 0;
}

/***************************************** 以上为模块内部函数 *************************************************/

/* http_request 负责提取资源，响应什么则由 http_response 决定 
 * 返回 0 - 请求解析正常并可进入下一个状态；
 * 返回 -1 - 请求解析异常但可进入下一个状态；
 * 返回 1 - 请求解析正常但还有数据未接收，保持当前状态不变继续接收
*/
int http_request(http_service_interface *http)
{
    int ret = 0;

    printf("-------------------\nrequest[%d]:\n%s\n", http->buffers.recvLength, http->buffers.recvBuffer);
    
    if(http->status == HTTP_REQUEST)
    {
        ret = http_readAllHeader(http); /* 读出全部消息头 */
        if(ret == 1)
            return 1;   // 下一次触发继续读剩下的头部，所以状态不变
        else if(ret == -1)  // 返回错误响应
            http->resp.code = HTTP_CODE_NOT_ACCEPTABLE;
        else    // 消息头已全部读完，可以进行解析
            http_resolveReqHeader(http);  // 解析头部
    }

    /* 根据方法进行处理并设置下一个状态 */
    switch(http->req.method)
    {
        case HTTP_METHOD_GET:
            if(strlen(http->ws.ws_key) > 0)  // 获取到了websocket的key，进行 websocket 握手
            {
                ret = ws_handshake(http);
                http->status = WS_DATATRANSFER;   // 握手完成后则进入通信状态
            }
            else    // 否则按普通 http 请求处理
            {
                ret = http_request_get(http);
                http->status = HTTP_RESPONSE;
            }
            break;

        case HTTP_METHOD_POST:
        case HTTP_METHOD_PUT:
            ret = http_request_post_put(http);
            if(ret <= 0)
            {   // 正常接收完消息体则可以进入 HTTP_RESPONSE 进行响应
                http->status = HTTP_RESPONSE;
            }
            else
            {   // 如果还有消息体的数据没有全部接收到缓冲区中，则需要继续接收
                http->status = HTTP_REQUEST_BODY_CONTINUE;
            }
            break;

        default:
            ret = -1;
            http->status = HTTP_RESPONSE;
            break;
    }

    return ret;
}

int http_response(http_service_interface *http)
{
    int ret = 0;
    if(http->resp.code == HTTP_CODE_NONE)    // 此处为 HTTP_CODE_NONE 说明接收阶段没有出错，因此没有指定错误的状态码
    {
        /* 根据方法执行响应，并确定返回的响应码 */
        switch(http->req.method)
        {
            case HTTP_METHOD_GET:
                http->resp.code = http_response_get(http);
                break;

            case HTTP_METHOD_POST:
                http->resp.code = http_response_post(http);
                break;

            case HTTP_METHOD_PUT:
                http->resp.code = HTTP_CODE_OK;
                break;

            default:    // 不支持的方法
                http->resp.code = HTTP_CODE_METHOD_NOT_ALLOWED;
                break;
        }
    }

    /* 返回错误放在这里统一处理 */
    if(http->resp.code != HTTP_CODE_OK)
    {
        ret = http_packErrorResponse(http->buffers.sendBuffer, MAX_BUFFER_SIZE - 1, http->resp.code);
        ret = send(http->sockfd, http->buffers.sendBuffer, ret, 0);
    }
    printf("# http response:%s\n",  http->buffers.sendBuffer);

    /* 完成响应后，在返回并进入 HTTP_REQUEST 状态前要清理资源 */
    http_resetInterface(http);
    http->status = HTTP_REQUEST;

    return ret;
}

void http_initInterface(http_service_interface *http, int sockfd)
{
    if(http == NULL)
        return ;

    memset(http, 0, sizeof(http_service_interface));
    http->sockfd = sockfd;
    http->status = HTTP_REQUEST;
}

/* 释放当前HTTP连接的所有资源 */
void http_resetInterface(http_service_interface *http)
{
    if(http == NULL) return;

    if(http->resp.body)
        free(http->resp.body);
    if(http->req.body)
        free(http->req.body);
    memset(&http->buffers, 0, sizeof(struct http_buffers));
    memset(&http->helper, 0, sizeof(struct http_helper));
    memset(&http->req, 0, sizeof(struct http_req));
    memset(&http->resp, 0, sizeof(struct http_resp));
    memset(&http->ws, 0, sizeof(ws_param));
}

/* 从消息体中提取某个变量的值 */
int http_getValueFromReqBody(char* buf, int buflen, const char* key, const char* body)
{
    if(buf == NULL || key == NULL || body == NULL) return -1;

    char* p = strstr(body, key);
    if(!p) return -1;

    int i = 0;
    p += strlen(key) + 1;
    while (*p && *p != '&' && i < buflen - 1)
    {
        buf[i++] = *p;
        p++;
    }
    buf[i] = '\0';

    return 0;
}
