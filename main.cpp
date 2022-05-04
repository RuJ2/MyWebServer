#include "threadpool.h"
#include "locker.h"
#include "http/http_conn.h"
#include "log/log.h"
#include "mySQL/sql_connection_pool.h"

#define SYNLOG  //同步写日志
//#define ASYNLOG //异步写日志

const int MAX_FD = 65535;        // 最大的文件描述符个数
const int MAX_EVENT_NUM = 10000; // 监听的最大事件数量

extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);

// 添加信号捕捉
void addsig(int sig, void(handler)(int))
{ // 函数指针,信号处理函数
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    // 将信号添加到信号集中
    sigfillset(&sa.sa_mask);
    // 执行sigaction
    sigaction(sig, &sa, NULL);
}

int main(int argc, char *argv[])
{
    // 初始化日志
    #ifdef ASYNLOG
    Log::get_instance()->init("./ServerLog/Log", 2000, 800000, 8); //异步日志模型
    #endif
    #ifdef SYNLOG
    Log::get_instance()->init("./ServerLog/Log", 2000, 800000, 0); //同步日志模型
    #endif  

    if (argc <= 1)
    {
        printf("按照如下格式运行: %s port_num\n", basename(argv[0]));
        LOG_ERROR("%s", "Input argc failure");
        exit(-1);
    }

    // 获取端口号
    int port = atoi(argv[1]);

    // 对SIGPIPE信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    //初始化数据库连接池
    connection_pool *m_connPool = connection_pool::GetInstance();
    string locolhost = "localhost";
    string m_user = "root";
    string m_passWord = "";
    string m_databaseName = "webdb";
    m_connPool->init("localhost", "root", "Ruj123456", "webdb", 3306, 8);

    // 创建线程池，初始化
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>(m_connPool);
    }
    catch (...)
    {
        LOG_ERROR("%s", "Create threadpoll failure");
        exit(-1);
    }

    // 创建数组用于保存客户端信息
    http_conn *users = new http_conn[MAX_FD];

    // //初始化数据库读取表
    users->initmysql_result(m_connPool);


    // 创建socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    // 端口复用,绑定前设置
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // 绑定
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    listen(listenfd, 5); // 监听

    epoll_event events[MAX_EVENT_NUM]; // 创建epoll对象，事件数组
    int epollfd = epoll_create(1);

    // 将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while (true)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if (num < 0 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }

        // 轮询文件描述符
        for (int i = 0; i < num; ++i)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                // 有客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlen);

                char clientIP[16];
                inet_ntop(AF_INET, &client_address.sin_addr.s_addr, clientIP, sizeof(clientIP));
                printf("------------------------------------\n");
                printf("User Ip: %s, | Port: %d \n", clientIP, ntohs(client_address.sin_port));

                if (http_conn::m_user_count >= MAX_FD)
                {
                    // 目前连接已满
                    // 给客户端写信息：服务器内部正忙
                    printf("Internal server busy\n");
                    close(connfd);
                    continue;
                }
                // 将新的客户数据初始化，放到数组中
                users[connfd].init(connfd, client_address);
                LOG_INFO("Client(%s) is connected", clientIP);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 对方异常断开或错误等事件,关闭连接
                printf("异常断开连接\n");
                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                if (users[sockfd].read())
                {
                    // 一次性把所有数据读完
                    pool->append(users + sockfd);
                }
                else
                {
                    printf("read 断开\n");
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!users[sockfd].write())
                {
                    // 一次性把所有数据写完
                    printf("write 断开\n");
                    users[sockfd].close_conn();
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;

    return 0;
}
