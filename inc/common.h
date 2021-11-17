#ifndef __COMMON_H__
#define __COMMON_H__
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

int base64_encode(unsigned char *in_str, int in_len, char *out_str);
int sendWholeFile(int outfd, int infd, int size);
int getGmtTime(char* szGmtTime, int len);
char* trim(char* str);
int readline(const char* allbuf,int level, char* linebuf);
int sockSetReuseAddr(int sockfd);
int fdSetNonBlock(int fd);

/* 64 bit 本地字节序转网络字节序 */
static inline unsigned long long htonll(unsigned long long host)
{
    return ((unsigned long long)htonl(host & 0xffffffff) << 32) + htonl(host >> 32);
}
/* 64 bit 网络字节序转本地字节序 */
static inline unsigned long long ntohll(unsigned long long net)
{
    return ((unsigned long long)ntohl(net & 0xffffffff) << 32) + ntohl(net >> 32);
}

#endif