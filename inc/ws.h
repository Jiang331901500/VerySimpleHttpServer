#ifndef __WS_H__
#define __WS_H__

#include "common.h"

struct ws_opHeader
{
    unsigned char opcode : 4,
                  RSV123 : 3,
                  FIN    : 1;

    unsigned char payloadLen : 7,
                  MASK       : 1;
}__attribute__ ((packed));  // __attribute__ ((packed)) 的作用就是告诉编译器取消结构在编译过程中的优化对齐,按照实际占用字节数进行对齐

struct ws_dataHeader126
{
    unsigned short payloadLen;
}__attribute__ ((packed));

struct ws_dataHeader127
{
    unsigned long long payloadLen;
}__attribute__ ((packed));


#define GUID ("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")

typedef struct
{
    char ws_key[512];
} ws_param;


#endif