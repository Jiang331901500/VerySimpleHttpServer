#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "http.h"
#define MAX_EVENTS_NUM 512

typedef struct _sockitem
{
	int sockfd;
    int epfd;
	int (*callback)(int fd, int events, void *arg);

    // http and ws param
    http_service_interface http;
} sockitem;

int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
int accept_cb(int fd, int events, void *arg);

#endif
