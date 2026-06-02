## 如何利用asio实现tcp服务
### asio的Proactor模式


### 使用Session类管理连接
    底层绑定用户id和session关联
    回调函数可以根据session反向查找用户进行消息推送


### 客户端和服务器使用json通信
    使用tlv方式封装消息防止粘包
    tlv方式：消息id、消息长度、消息内容，其中id和长度组成消息头

### 心跳机制检测连接可用性


## 为何需要封装连接池
        多个线程使用同一个连接不安全，需要为每个县城分配独立链接，连接数不能随着线程数无限增加
    每个线程需要操作时才从连接池中取出连接进行访问
        MySQL连接池封装包含Mgr管理层（api调用）和Dao数据访问层（底层调用），前者是单例模式，后者包含一个号连接池，
        #描述连接池工作方法#，采用生产者消费者模式管理可用连接，通过心跳定时访问mysql保活连接 

# GateServer梳理
### 层次
GateServer
│
├── 启动层
│   └── GateServer.cpp
│
├── 网络层 (Asio)
│   ├── CServer
│   ├── HttpConnection
│   └── AsioIOServicePool
│
├── 逻辑层
│   └── LogicSystem
│
├── RPC通信层
│   ├── StatusGrpcClient
│   └── VerifyGrpcClient
│
├── 数据访问层
│   ├── MysqlMgr / MysqlDao
│   └── RedisMgr
│
├── 工具层
│   ├── ConfigMgr
│   ├── Singleton
│   └── const
│
└── 通信协议
    └── message.proto

### asio网络库

一个跨平台C++库，用于实现异步I/O。其采用Proactor模式，通过异步操作和时间循环，使用少量线程处理大量链接，减少阻塞和线程切换成本，从而实现高并发高性能
- 异步操作：用户发起网络请求后，asio不阻塞进程程等待数据返回，而是继续处理其他任务
- 事件循环：一个或几个 `io_context` 线程会负责监听操作系统（Linux下用 epoll）的通知。当内核汇报“数据准备好了”，ASIO 就会回头调用用户的回调函数来处理

**组件**
1. `io_context` 与操作系统交互，负责调度所有异步任务。使用`io_context.run()`启动事件循环
2. `socket` 网络通信接口，对应TCP、UDP等协议的通信端点
3. `timer` 定时器，用于处理超市或执行周期性任务
4. `strand` 线程安全绳。多线程环境下调用同一个`socket`的方法，`strand`保证回调函数不并发执行，避免数据竞争