#include "common.h"

/* 设置fd为非阻塞 */
int fdSetNonBlock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if(flags < 0) return flags;

    flags |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags) < 0) return -1;

    return 0;
}

/* 设置socket SO_REUSEADDR*/
int sockSetReuseAddr(int sockfd)
{
    int reuse = 1;
    return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}


/* 读一行数据，不包括末尾的 \r\n */
int readline(const char* allbuf,int level, char* linebuf)
{
    if(allbuf == NULL || linebuf == NULL) return -1;    
    
	int len = strlen(allbuf);    

	for (;level < len; ++level)    {        
		if(allbuf[level]=='\r' && allbuf[level+1]=='\n')            
			return level+2;        
		else            
			*(linebuf++) = allbuf[level];    
	}    
    linebuf[0] = '\0';
	return -1;
}

/* 去除字符串首尾的空格 */
char* trim(char* str)
{
    char* start;
    if(str == NULL)
        return NULL;

    while(*str == ' ') str++;
    start = str;
    
    str += strlen(str) - 1;
    while(*str == ' ') 
    {
        *str = '\0';
        str--;
    }

    return start;
}

int getGmtTime(char* szGmtTime, int len)
{
    if (szGmtTime == NULL)
        return -1;
    
    time_t rawTime;
    struct tm* timeInfo;

    time(&rawTime);
    timeInfo = gmtime(&rawTime);
    strftime(szGmtTime, len, "%a, %d %b %Y %H:%M:%S GMT", timeInfo);
 
    return strlen(szGmtTime);
}

int sendWholeFile(int outfd, int infd, int size)
{
    int left = size;
    int ret = 0;

    while(left > 0)
    {
        ret = sendfile(outfd, infd, NULL, left);
        if(ret < 0)
        {
            if(errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
            {   
                usleep(200);
                continue;
            }
            return ret; // error
        }
        left -= ret;
    }

    return size;
}

int base64_encode(unsigned char *in_str, int in_len, char *out_str)
{    
	BIO *b64, *bio;    
	BUF_MEM *bptr = NULL;    
	size_t size = 0;    

	if (in_str == NULL || out_str == NULL)        
		return -1;    

	b64 = BIO_new(BIO_f_base64());	
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio);

	BIO_write(bio, in_str, in_len);    
	BIO_flush(bio);    

	BIO_get_mem_ptr(bio, &bptr);    
	memcpy(out_str, bptr->data, bptr->length);    
	out_str[bptr->length-1] = '\0';    
	size = bptr->length;    

	BIO_free_all(bio);    
	return size;
}