#ifndef __HTTP__H__
#define __HTTP__H__

#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "ws.h"
#include "common.h"

#define MAX_BUFFER_SIZE 2048

#define HTTP_CHAR_SET "utf-8"
#define HTTP_RESRC_ROOTDIR ("public/")
#define HTTP_INDEX_PAGE ("index.html")
#define HTTP_HEADER_END ("\r\n\r\n")
#define HTTP_MAX_RESOURCE_NAME 512
#define HTTP_MAX_HEADER_LEN 1024 // 最大允许头部长度
#define HTTP_MAX_BODY_LEN (10 * 1024 * 1024) // 最大消息体暂定为10MB

typedef enum
{
    HTTP_CODE_NONE = 0, // 默认值
    HTTP_CODE_OK = 200,
    HTTP_CODE_NOTFOUND = 404,
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
    HTTP_CODE_METHOD_NOT_ALLOWED = 405,
    HTTP_CODE_NOT_ACCEPTABLE = 406,
} HTTP_CODE;

struct filetype_pair
{
    int content_type_enum;
	char *file_suffix;
	char *content_type;
};

typedef enum
{
    HTTP_CONTENT_TYPE_HTML = 0,
    HTTP_CONTENT_TYPE_JPG,
    HTTP_CONTENT_TYPE_PDF,
    HTTP_CONTENT_TYPE_ICO,
} HTTP_CONTENT_TYPE;

struct http_req
{
    char header[HTTP_MAX_HEADER_LEN];
    char* body;
    int method;     // http 方法
    char resource[HTTP_MAX_RESOURCE_NAME]; // 请求资源的名称
    int contentlength;
};
struct http_resp
{
    char* body;
    int contentLength;
    HTTP_CONTENT_TYPE contentType; // 使用 enum HTTP_CONTENT_TYPE
    HTTP_CODE code; // 响应码
};

struct http_helper
{
    char* startOfBody;  // 当前缓冲区中，从哪个地址开始都是消息体的数据，用于作为头部和消息体的分界线
    int recvHeaderLen;  // 已接收到的消息头长度
    int recvBodyLen;    // 已接收到的消息体长度
};

struct http_buffers
{
    char recvBuffer[MAX_BUFFER_SIZE];   // 接收缓冲首地址
	char sendBuffer[MAX_BUFFER_SIZE];   // 发送缓冲首地址
    int recvLength;     // 接收缓冲区当前数据长度
    int sendLength;     // 发送缓冲区当前数据长度
};

typedef struct _http_service_interface
{
    struct http_req req;
    struct http_resp resp;
    struct http_helper helper;
    struct http_buffers buffers;
    int status;
    int sockfd;
    // websocket param
    ws_param ws;
} http_service_interface;

enum HTTP_STATUS
{
    UNKNOWN_STATUS,
    WS_DATATRANSFER,            // WS正常通信
    WS_END,                     // WS结束
    HTTP_REQUEST,               // HTTP请求，包括 WS握手
    HTTP_REQUEST_BODY_CONTINUE, // 上一次 HTTP_REQUEST 由于报文体的数据量超出了缓冲区而没有接收完，此次继续
    HTTP_RESPONSE,              // HTTP响应
};

int http_request(http_service_interface *http);
int http_response(http_service_interface *http);
void http_resetInterface(http_service_interface *http);
void http_initInterface(http_service_interface *http, int sockfd);
int http_getValueFromReqBody(char* buf, int buflen, const char* key, const char* body);


int ws_datatransfer(http_service_interface *http);
int ws_handshake(http_service_interface *http);
int ws_response(http_service_interface *http);

#endif