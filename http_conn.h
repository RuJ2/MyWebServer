#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "locker.h"
#include <assert.h>

class http_conn
{
public:
    static int m_epollfd;    // 所有的socket上的事件都被注册到同一个epoll事件上
    static int m_user_count; // 统计用户的数量

public:
    http_conn() {}
    ~http_conn() {}

    void init(int sockfd, const sockaddr_in &addr); // 初始化新接收
    void close_conn();                              // 关闭连接
    void process();                                 // 处理客户端请求
    bool read();                                    // 非阻塞的读
    bool write();                                   // 非阻塞的写

private:
    int m_sockfd;          // 该http连接的socket
    sockaddr_in m_address; // 通信的socket地址
};

#endif
