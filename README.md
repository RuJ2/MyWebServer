# MyWebServer
## v-1.0 搭建基本框架
- 使用 **线程池 + 非阻塞socket + epoll + 事件处理(Reactor模拟)** 的并发模型
- 在Main中使用epoll同时监听新的客户连接和客户请求
  - 每新增一个新连接，将connfd注册到内核事件表中
  - 每检测到读写事件时，通知主线程有事件发生，并将事件放入请求队列，交给工作线程进行处理
- 使用线程池并发处理用户请求，主线程负责读写，线程池中的工作线程负责处理业务逻辑（HTTP报文解析等）
- 线程池的实现依靠 **锁机制** 和 **信号量** 实现线程同步，保证操作的原子性 <lock.h>


#### 只实现了框架，能够读取连接请求并进行工作线程调用，具体报文处理部分还未实现

--------------------------

## v-1.1 补充HTTP报文处理部分
#### 流程
- 当浏览器发出http连接时，主线程创建http类对象并接受请求将数据读入对应buffer，将该对象插入任务队列
- 工作线程取出任务，调用process_read函数，通过主从状态机对请求报文解析
- 解析完通过do_request函数生成响应报文，再通过process_write写入buffer，返回给浏览器端

##### 主状态机
  三种状态，标识解析位置
  - CHECK_STATE_REQUESTLINE，解析请求行
  - CHECK_STATE_HEADER，解析请求头
  - CHECK_STATE_CONTENT，解析消息体，将用于解析POST请求

##### 从状态机
  三种状态，标识解析一行的读取状态
  - LINE_OK，完整读取一行
  - LINE_BAD，报文语法有误
  - LINE_OPEN，读取的行不完整

##### 解析
- 获取请求报文，并解析请求首行、请求头和请求体
- 获取目标请求方法（目前只实现了GET）、URL、HTTP版本号及头部信息
- 根据URL查看本地资源，判断是否存在、访问权限、是否目录等
- 若均满足条件，则打开文件并mmap创建内存映射，将文件映射到内存逻辑地址
- 依据do_request状态，调用precess_write向m_write_buffer中写入响应报文
- 通过io向量机制iovec，声明两个iovec，分别指向m_write_buffer和mmap的地址m_file_address
- 完成响应报文后注册epollout事件，主线程将调用write将报文发送给浏览器端

## v-1.2 补充log部分
- 使用单例模式创建日志系统，对服务器运行状态、错误信息和访问数据进行记录
- 根据实际情况分别使用同步和异步写入两种方式
- 异步写入方式，将生产者-消费者模型封装为阻塞队列，工作线程将要写内容push进队列，创建写线程读取内容，写入日志文件

## v-1.3 添加数据库连接部分
#### 能够通过post请求完成注册和登录的校验工作
**数据库连接池：**
使用单例模式创建，实现外部访问接口，并通过RAII机制释放数据库连接，使用信号量实现多线程争夺连接的同步机制，初始化为数据库连接总数

**流程图：**
- 载入数据库表，将数据库中的数据载入到服务器中
- 服务器从报文中取出用户名和密码
- 根据数据库中数据查询结果，完成注册和登录校验
- 进行进行页面跳转
