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