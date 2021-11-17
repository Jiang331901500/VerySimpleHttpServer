#include "ws.h"
#include "http.h"

// 既可以加掩码也可以消除掩码
static void ws_maskProcess(char *data,int len,char *mask) 
{    
	int i;    
	for (i = 0;i < len;i ++)        
		*(data+i) ^= *(mask+(i%4));
}

/* websocket 数据帧解包 */
static char* ws_depack(char* frame, char* maskbuf, unsigned char* opcode)
{
    unsigned long long payloadLen = 0;
    char* payload;
    char* mask;

    if(frame == NULL || maskbuf == NULL)
        return NULL;

    struct ws_opHeader* op = (struct ws_opHeader*)frame;    // 取出op相关头部
    if(op->payloadLen == 126)
    {
        struct ws_dataHeader126* header126 = (struct ws_dataHeader126*)(frame + sizeof(struct ws_opHeader));
        payloadLen = ntohs(header126->payloadLen);
        mask = (char*)header126 + sizeof(struct ws_dataHeader126);
    }
    else if(op->payloadLen == 127)
    {
        struct ws_dataHeader127* header127 = (struct ws_dataHeader127*)(frame + sizeof(struct ws_opHeader));
        payloadLen = ntohll(header127->payloadLen);
        mask = (char*)header127 + sizeof(struct ws_dataHeader127);
    }
    else
    {
        payloadLen = op->payloadLen;
        mask = frame + sizeof(struct ws_opHeader);
    }

    payload = mask;
    if(op->MASK)
    {
        payload = mask + 4;
        memcpy(maskbuf, mask, 4);
        ws_maskProcess(payload, payloadLen, maskbuf);
    }

    *opcode = op->opcode;
    return payload;
}

/* websocket 数据帧封包，如果maskbuf为NULL则不加掩码处理 */
static int ws_enpack(char* frame, char* payload, unsigned long long payloadLen, char* maskbuf, char opcode)
{
    char* mask;

    if(frame == NULL)
        return -1;

    struct ws_opHeader op = {0};
    op.FIN = 1;     // 一次发完
    op.opcode = opcode;
    op.MASK = maskbuf ? 1 : 0;

    if(payloadLen < 126)
    {
        op.payloadLen = payloadLen;
        memcpy(frame, &op, sizeof(struct ws_opHeader));
        mask = frame + sizeof(struct ws_opHeader);
    }
    else if(payloadLen < 0xffff)
    {
        op.payloadLen = 126;
        struct ws_dataHeader126 header = {0};
        header.payloadLen = htons(payloadLen);
        memcpy(frame, &op, sizeof(struct ws_opHeader));
        memcpy(frame + sizeof(struct ws_opHeader), &header, sizeof(struct ws_dataHeader126));
        mask = frame + sizeof(struct ws_opHeader) + sizeof(struct ws_dataHeader126);
    }
    else
    {
        op.payloadLen = 127;
        struct ws_dataHeader127 header = {0};
        header.payloadLen = htonll(payloadLen);
        memcpy(frame, &op, sizeof(struct ws_opHeader));
        memcpy(frame + sizeof(struct ws_opHeader), &header, sizeof(struct ws_dataHeader126));
        mask = frame + sizeof(struct ws_opHeader) + sizeof(struct ws_dataHeader127);
    }

    // 是否需要做掩码处理
    if(maskbuf)
    {
        ws_maskProcess(payload, payloadLen, maskbuf);
        memcpy(mask, maskbuf, 4);
        mask += 4;
    }
    memcpy(mask, payload, payloadLen);

    return mask - frame + payloadLen;
}

int ws_response(http_service_interface *http)
{
    int ret = send(http->sockfd, http->buffers.sendBuffer, http->buffers.sendLength, 0);
    return ret;
}

/* ws 握手过程处理 */
int ws_handshake(http_service_interface *http)
{
    unsigned char sha1_data[SHA_DIGEST_LENGTH+1] = {0};
    char base64_result[32] = {0};    // 最终base64的结果

    strcat(http->ws.ws_key, GUID);   // 拼接GUID
    SHA1((unsigned char*)http->ws.ws_key, strlen(http->ws.ws_key), sha1_data);
    base64_encode(sha1_data, strlen((char*)sha1_data), base64_result);

    sprintf(http->buffers.sendBuffer, "HTTP/1.1 101 Switching Protocols\r\n" \
    "Upgrade: websocket\r\n" \
    "Connection: Upgrade\r\n" \
    "Sec-WebSocket-Accept: %s\r\n" \
    "\r\n", base64_result);             

    http->buffers.sendLength = strlen(http->buffers.sendBuffer);
    printf("# do handshake and get into transfer status.\n");

    return 0;
}

/* 通信过程处理 */
int ws_datatransfer(http_service_interface *http)
{
    char* payload;
    char mask[4];
    unsigned long long payloadLen = 0;
    unsigned char opcode;

    // 解包
    payload = ws_depack(http->buffers.recvBuffer, mask, &opcode);
    payloadLen = http->buffers.recvLength - (payload - http->buffers.recvBuffer);

    printf("opcode:%d, recv len:%llu, payload: %s\n", opcode, payloadLen, payload);

    if(opcode == 8)     // 如果是结束帧，则回复中不带payload
        payloadLen = 0;

    // 封包
    http->buffers.sendLength = ws_enpack(http->buffers.sendBuffer, payload, payloadLen, NULL, opcode);

    memset(http->buffers.recvBuffer, 0, MAX_BUFFER_SIZE);
    http->buffers.recvLength = 0;

    if(opcode == 8)
        http->status = WS_END;
    else
        http->status = WS_DATATRANSFER;

    return 0;
}