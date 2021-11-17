#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>

#include "httpserver.h"

struct reactor
{
    int epfd;
    struct epoll_event events[MAX_EVENTS_NUM];
};


int close_connection(sockitem *si)
{
    if(si == NULL)
        return -1;
    struct epoll_event ev;
    ev.data.ptr = si;
    epoll_ctl(si->epfd, EPOLL_CTL_DEL, si->sockfd, &ev);  
    http_resetInterface(&si->http);
    close(si->sockfd);
    free(si);
    return 0;
}

/* 写IO回调函数 */
int send_cb(int fd, int events, void *arg)
{
    sockitem *si = arg;
    struct epoll_event ev;
    int ret  = 0;

    switch (si->http.status)
    {
        case WS_DATATRANSFER:   // websocket传输数据中
            ret = ws_response(&si->http);
            break;
        
        case WS_END:    // websocket结束状态，关闭连接
            ret = ws_response(&si->http);
            printf("# websocket client request to close.\n");
            close_connection(si);
            return ret; // 直接结束
            break;

        case HTTP_RESPONSE:
            http_response(&si->http);
            break;
        
        default:
            printf("# status error: %d\n", si->http.status);
            close_connection(si);
            return -1;
            break;
    }

    memset(si->http.buffers.sendBuffer, 0, MAX_BUFFER_SIZE);
    si->http.buffers.sendLength = 0;

    si->callback = recv_cb;
    ev.events = EPOLLIN;
    ev.data.ptr = si;
    epoll_ctl(si->epfd, EPOLL_CTL_MOD, si->sockfd, &ev);

    return ret;
}

/* 读IO回调函数 */
int recv_cb(int fd, int events, void *arg)
{
    sockitem *si = arg;

    int clientfd = si->sockfd;
    memset(si->http.buffers.recvBuffer, 0, MAX_BUFFER_SIZE);
    int ret = recv(clientfd, si->http.buffers.recvBuffer, MAX_BUFFER_SIZE, 0);
    if(ret <= 0)
    {
        if(ret < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {   // 被打断直接返回的情况
                return ret;
            }
        }
        /* 将当前客户端socket从epoll中删除 */
        printf("# [%d]client disconnected...\n", ret);
        close_connection(si);
    }
    else
    {
        /* status machine */
        si->http.buffers.recvLength = ret;
        switch(si->http.status)
        {
            case HTTP_REQUEST:
            case HTTP_REQUEST_BODY_CONTINUE:
                ret = http_request(&si->http);
                break;

            case WS_DATATRANSFER:
                ret = ws_datatransfer(&si->http);
                break;

            default:
                assert(0);
        }
        if(ret <= 0)
        {
            si->callback = send_cb;
            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = si;
            epoll_ctl(si->epfd, EPOLL_CTL_MOD, si->sockfd, &ev);
        }
    }

    return ret;
}

/* accept也属于读IO操作的回调 */
int accept_cb(int fd, int events, void *arg)
{
    sockitem *si = arg;
    struct epoll_event ev;

    struct sockaddr_in client;
    memset(&client, 0, sizeof(struct sockaddr_in));
    socklen_t caddr_len = sizeof(struct sockaddr_in);

    int clientfd = accept(si->sockfd, (struct sockaddr*)&client, &caddr_len);
    if(clientfd < 0)
    {
        printf("# accept error\n");
        return clientfd;
    }

    char str[INET_ADDRSTRLEN] = {0};
    printf("recv from %s at port %d\n", inet_ntop(AF_INET, &client.sin_addr, str, sizeof(str)),
        ntohs(client.sin_port));

    fdSetNonBlock(clientfd);

    sockitem *client_si = (sockitem*)malloc(sizeof(sockitem));
    if(client_si == NULL)
    {
        perror("client_si malloc");
        close(clientfd);
        return -1;
    }
    memset(client_si, 0, sizeof(sockitem));
    client_si->sockfd = clientfd;
    client_si->callback = recv_cb;
    client_si->epfd = si->epfd;
    http_initInterface(&client_si->http, clientfd);

    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN;
    ev.data.ptr = client_si;
    epoll_ctl(si->epfd, EPOLL_CTL_ADD, clientfd, &ev);  // 把客户端socket增加到epoll中监听

    return clientfd;
}

int main(int argc, char* argv[])
{
    int port = 80;
    if(argc == 2)
    {
        port = atoi(argv[1]);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket ");
		return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    sockSetReuseAddr(sockfd);
    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind ");
		return -1;
    }

    if(listen(sockfd, 5) < 0)
    {
        perror("listen ");
		return -1;
    }

    /* go epoll */
    struct reactor ra;
    ra.epfd = epoll_create(1);

    sockitem *si = (sockitem*)malloc(sizeof(sockitem));    // 自定义数据，用于传递给回调函数
    si->sockfd = sockfd;
    si->callback = accept_cb;
    si->epfd = ra.epfd;
    http_initInterface(&si->http, sockfd);

    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN;    // 默认LT
    ev.data.ptr = si;

    epoll_ctl(ra.epfd, EPOLL_CTL_ADD, sockfd, &ev);    // 添加到事件到epoll

    while(1)
    {
        int nready = epoll_wait(ra.epfd, ra.events, MAX_EVENTS_NUM, -1);
        if(nready < 0)
        {
            printf("epoll_wait error.\n");
            break;
        }

        int i;
        for(i = 0; i < nready; i++)
        {
            si = ra.events[i].data.ptr;
            if(ra.events[i].events & (EPOLLIN | EPOLLOUT))
            {
                if(si->callback != NULL)
                    si->callback(si->sockfd, ra.events[i].events, si);  // 调用回调函数
            }
        }
    }
}
