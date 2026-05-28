# StatusServer 模块分析

## 1. 模块职责

`StatusServer` 是系统里的状态与调度服务，核心职责集中在两件事：

1. 给客户端侧或其他服务分配一个可用的 `ChatServer`。
2. 为登录流程生成并校验临时 `token`。

从当前代码看，它本质上是一个独立的 gRPC 服务，不直接承载聊天业务，也不直接处理 TCP 长连接。它更像一个“登录接入层的状态中心”。

对应代码：

- 服务入口：[StatusServer.cpp](/D:/Github/ChatProj/StatusServer/StatusServer.cpp)
- gRPC 实现：[StatusServiceImpl.cpp](/D:/Github/ChatProj/StatusServer/StatusServiceImpl.cpp)
- 协议定义：[message.proto](/D:/Github/ChatProj/StatusServer/message.proto)

---

## 2. 程序入口

程序入口在 [StatusServer.cpp](/D:/Github/ChatProj/StatusServer/StatusServer.cpp) 的 `main -> RunServer()`。

启动流程如下：

1. `ConfigMgr::Inst()` 读取当前目录下的 `config.ini`。
2. 根据 `[StatusServer]` 下的 `Host`、`Port` 拼出监听地址。
3. 构造 `StatusServiceImpl`，把它注册到 gRPC `ServerBuilder`。
4. `builder.BuildAndStart()` 启动 gRPC 服务。
5. 用一个独立的 `boost::asio::io_context` + `signal_set` 监听 `SIGINT` / `SIGTERM`。
6. 收到退出信号后调用 `server->Shutdown()`，主线程上的 `server->Wait()` 返回，服务退出。

这个入口非常单纯，没有额外的 HTTP 层，也没有自己维护 acceptor 或 socket。

---

## 3. 核心类与核心文件

### 3.1 `StatusServiceImpl`

文件：

- [StatusServiceImpl.h](/D:/Github/ChatProj/StatusServer/StatusServiceImpl.h)
- [StatusServiceImpl.cpp](/D:/Github/ChatProj/StatusServer/StatusServiceImpl.cpp)

这是当前模块最核心的业务类，继承自 `message::StatusService::Service`，对外暴露两个 RPC：

1. `GetChatServer`
2. `Login`

内部关键成员：

- `_servers`
  作用：保存配置文件里声明的所有 `ChatServer` 节点。
- `_server_mtx`
  作用：保护 `_servers` 读取和负载选择过程。

内部关键方法：

- `StatusServiceImpl()`
  启动时从 `config.ini` 的 `[chatservers]`、`[chatserver1]`、`[chatserver2]` 等 section 加载聊天节点列表。
- `getChatServer()`
  从 `_servers` 中选出“负载最小”的聊天节点。
- `insertToken(int uid, std::string token)`
  把 `uid -> token` 写入 Redis。

### 3.2 `ConfigMgr`

文件：

- [ConfigMgr.h](/D:/Github/ChatProj/StatusServer/ConfigMgr.h)
- [ConfigMgr.cpp](/D:/Github/ChatProj/StatusServer/ConfigMgr.cpp)

职责是读取 INI 配置并缓存到内存里。  
特点：

- 单例：`ConfigMgr::Inst()`
- 使用 `boost::property_tree::read_ini`
- 配置访问方式是 `cfg["Section"]["Key"]`

### 3.3 `RedisMgr` / `RedisConPool`

文件：

- [RedisMgr.h](/D:/Github/ChatProj/StatusServer/RedisMgr.h)
- [RedisMgr.cpp](/D:/Github/ChatProj/StatusServer/RedisMgr.cpp)

这是 `StatusServer` 当前真正依赖的基础设施层。

主要用途：

1. 保存用户 token：`utoken_<uid>`
2. 保存各个聊天节点登录人数：`logincount` 哈希表

`RedisConPool` 维护一个 hiredis 连接池，并有后台线程定期 `PING` 检查连接健康。

### 3.4 `MysqlMgr` / `MysqlDao`

文件：

- [MysqlMgr.h](/D:/Github/ChatProj/StatusServer/MysqlMgr.h)
- [MysqlDao.h](/D:/Github/ChatProj/StatusServer/MysqlDao.h)
- [MysqlDao.cpp](/D:/Github/ChatProj/StatusServer/MysqlDao.cpp)

这一层提供用户注册、邮箱检查、密码更新、密码校验能力。

但结合当前 `StatusServiceImpl` 来看：

- `StatusServer` 当前实际对外暴露的两个 RPC 并没有调用 MySQL。
- 这些类更像是复用代码，或者为后续扩展登录/账号能力做准备。

### 3.5 `message.proto`

文件：

- [message.proto](/D:/Github/ChatProj/StatusServer/message.proto)

当前模块真正用到的是 `StatusService`：

- `rpc GetChatServer(GetChatServerReq) returns (GetChatServerRsp)`
- `rpc Login(LoginReq) returns (LoginRsp)`

同一个 proto 里还定义了 `VarifyService`、`ChatService` 等其他服务，说明这个仓库采用了共享 proto 的方式。

### 3.6 其他辅助类

- [const.h](/D:/Github/ChatProj/StatusServer/const.h)
  定义错误码、Redis key 前缀、`Defer` 等通用设施。
- [DistLock.cpp](/D:/Github/ChatProj/StatusServer/DistLock.cpp)
  实现 Redis 分布式锁，但当前模块主流程未启用，相关调用在 `RedisMgr.cpp` 中被注释。
- [ChatGrpcClient.cpp](/D:/Github/ChatProj/StatusServer/ChatGrpcClient.cpp)
  预留了调用 ChatServer 的 gRPC 客户端，但当前 `NotifyAddFriend` 还是空实现，说明暂未接入主业务链路。
- [AsioIOServicePool.cpp](/D:/Github/ChatProj/StatusServer/AsioIOServicePool.cpp)
  提供 io_context 池，但当前 `StatusServer.cpp` 并未实际使用。

---

## 4. 网络通信结构

当前模块的网络结构非常清晰：

1. 外部调用方通过 gRPC 请求 `StatusServer`
2. `StatusServer` 内部访问 Redis 获取负载状态、写入 token
3. `StatusServer` 返回一个可连接的 `ChatServer` 地址

可以简化为：

```text
Client / GateServer / ChatServer
            |
            | gRPC: GetChatServer / Login
            v
       StatusServer
            |
            | hiredis
            v
           Redis
```

从代码层面看：

- gRPC 服务端：`StatusServer.cpp`
- gRPC 服务实现：`StatusServiceImpl.cpp`
- Redis 访问层：`RedisMgr.cpp`

当前没有直接调用 MySQL 的在线链路，也没有真正调用 `ChatGrpcClient` 访问其他聊天服务。

---

## 5. 消息处理流程

### 5.1 `GetChatServer` 流程

入口：`StatusServiceImpl::GetChatServer`

处理步骤：

1. 收到 `GetChatServerReq`，请求体里只有 `uid`。
2. 调用 `getChatServer()`。
3. `getChatServer()` 遍历 `_servers`。
4. 对每个 server，通过 Redis `HGET logincount <server_name>` 查询当前连接数。
5. 选出连接数最小的节点。
6. 生成 UUID 字符串作为 token。
7. 调用 `insertToken(uid, token)`，写入 Redis：`SET utoken_<uid> <token>`
8. 把 `host`、`port`、`token`、`error=Success` 填入响应返回。

这里的业务含义是：

- `StatusServer` 不直接建立聊天连接；
- 它只负责告诉调用方“你应该去连哪个 ChatServer”，并发放一个后续登录校验用的 token。

### 5.2 `Login` 流程

入口：`StatusServiceImpl::Login`

处理步骤按当前代码是：

1. 取出请求里的 `uid` 和 `token`。
2. Redis 查询 `GET utoken_<uid>`。
3. 如果 `Get` 返回 `success == true`，直接返回 `UidInvalid`。
4. 如果 `token_value != token`，返回 `TokenInvalid`。
5. 否则返回 `Success`。

从“代码字面逻辑”看，这里存在明显问题：

- `RedisMgr::Get` 成功时，理论上表示 token 查到了；
- 但 `Login` 却在查到后直接返回 `UidInvalid`；
- 这会导致正常 token 反而被判失败。

所以当前实现大概率应该是条件写反了，合理逻辑应更接近：

1. `Get` 失败才说明 `uid` 无效或 token 不存在；
2. `Get` 成功后再比对 token 内容。

这点在阅读该模块时需要特别留意。

---

## 6. 线程模型

### 6.1 gRPC 服务线程

`StatusServer.cpp` 使用的是 gRPC 同步服务模型：

- `server->Wait()` 阻塞主线程；
- RPC 处理线程由 gRPC 框架内部管理。

也就是说，`GetChatServer` / `Login` 并不是主线程串行处理，而是由 gRPC runtime 并发调用 `StatusServiceImpl`。

### 6.2 信号监听线程

程序额外创建了一个线程执行：

```cpp
std::thread([&io_context]() { io_context.run(); }).detach();
```

这个线程只负责跑 `boost::asio::signal_set`，用于优雅退出。

### 6.3 Redis 连接池后台线程

`RedisConPool` 在构造时启动一个检查线程：

- 每秒轮询一次计数器
- 每 60 秒做一轮连接健康检查
- 通过 `PING` 验证连接是否有效
- 失效连接尝试重连

### 6.4 MySQL 连接池后台线程

`MySqlPool` 也会起一个后台线程做保活检查，但当前在线主流程未走 MySQL。

### 6.5 共享数据并发保护

`StatusServiceImpl::_servers` 通过 `_server_mtx` 保护。  
不过当前锁保护的主要是遍历和更新 `con_count`，并不涉及复杂写入。

---

## 7. 配置系统

配置文件： [config.ini](/D:/Github/ChatProj/StatusServer/config.ini)

当前配置包含几类信息：

### 7.1 服务自身配置

```ini
[StatusServer]
Port = 50052
Host = 0.0.0.0
```

决定 gRPC 服务监听地址。

### 7.2 MySQL 配置

```ini
[Mysql]
Host = 127.0.0.1
Port = 3308
User = root
Passwd = 123456
Schema = roottest
```

供 `MysqlDao` 初始化连接池使用。

### 7.3 Redis 配置

```ini
[Redis]
Host = 81.68.86.146
Port = 6380
Passwd = 123456
```

供 `RedisMgr` 初始化连接池使用。

### 7.4 ChatServer 列表

```ini
[chatservers]
Name = chatserver1,chatserver2

[chatserver1]
Name = chatserver1
Host = 127.0.0.1
Port = 8090

[chatserver2]
Name = chatserver2
Host = 127.0.0.1
Port = 8091
```

`StatusServiceImpl` 构造时会读取这些 section，建立 `_servers` 映射。

配置系统的特点：

- 实现简单直接；
- 所有配置在进程启动时一次性读入；
- 没有热更新机制；
- 配置读取依赖当前工作目录下存在 `config.ini`。

---

## 8. 与其他服务的交互方式

### 8.1 与调用方

通过 gRPC 暴露 `StatusService`。

已有其他模块在引用这个服务：

- `GateServer/StatusGrpcClient.cpp`
- `ChatServer/StatusGrpcClient.cpp`

说明 `StatusServer` 是一个共享基础服务，不只给单一模块用。

### 8.2 与 Redis

通过 hiredis 直连 Redis。

核心交互键：

- `logincount`
  类型：Hash  
  作用：记录每个聊天节点当前连接数。
- `utoken_<uid>`
  类型：String  
  作用：保存用户登录 token。

### 8.3 与 MySQL

通过 MySQL Connector/C++ 连接数据库。  
当前代码已具备用户数据操作能力，但在本模块主 RPC 中未使用。

### 8.4 与 ChatServer

代码里预留了 `ChatGrpcClient`，但从当前实现看：

- 连接池已能初始化；
- `NotifyAddFriend` 还没有真正发送 RPC；
- 所以 StatusServer 当前并未承担聊天消息转发角色。

---

## 9. 使用到的设计模式

### 9.1 单例模式

大量基础组件都采用单例：

- `ConfigMgr::Inst()`
- `RedisMgr` 继承 `Singleton<RedisMgr>`
- `MysqlMgr` 继承 `Singleton<MysqlMgr>`
- `AsioIOServicePool` 继承 `Singleton<AsioIOServicePool>`

用途是让配置、连接池、基础设施在进程内全局复用。

### 9.2 对象池模式

连接池本质上是对象池：

- `RedisConPool`
- `MySqlPool`
- `ChatConPool`

通过队列缓存连接对象，避免每次请求重新建立连接。

### 9.3 RAII

`Defer` 类利用析构回调实现资源回收，主要用于：

- Redis 连接归还
- MySQL 连接归还
- 分布式锁解锁

### 9.4 门面/管理器模式

`RedisMgr`、`MysqlMgr` 把底层连接池和命令细节封装起来，对上层只暴露较少的业务化接口。

---

## 10. 核心调用链

### 10.1 服务启动调用链

```text
main
  -> RunServer
     -> ConfigMgr::Inst
     -> StatusServiceImpl::StatusServiceImpl
     -> grpc::ServerBuilder::BuildAndStart
     -> server->Wait
```

### 10.2 获取聊天节点调用链

```text
RPC GetChatServer
  -> StatusServiceImpl::GetChatServer
     -> StatusServiceImpl::getChatServer
        -> RedisMgr::HGet(LOGIN_COUNT, serverName)
     -> generate_unique_string
     -> StatusServiceImpl::insertToken
        -> RedisMgr::Set(utoken_<uid>, token)
```

### 10.3 登录校验调用链

```text
RPC Login
  -> StatusServiceImpl::Login
     -> RedisMgr::Get(utoken_<uid>, token_value)
     -> token 比较
     -> 返回 LoginRsp
```

---

## 11. 当前模块的业务重点

如果只抓主业务逻辑，建议把注意力集中在下面三处：

1. `StatusServiceImpl::GetChatServer`
   这是“登录前分配聊天节点”的核心入口。
2. `StatusServiceImpl::getChatServer`
   这是“按 Redis 负载做节点选择”的核心策略。
3. `StatusServiceImpl::Login`
   这是“token 校验”的核心逻辑，但当前实现疑似有 bug。

其中最关键的状态数据并不保存在内存里，而是落在 Redis 中：

- 节点负载状态：`logincount`
- 用户登录令牌：`utoken_<uid>`

所以这个模块的业务本质不是复杂计算，而是：

- 读取配置
- 查询 Redis 状态
- 做轻量调度
- 发放和校验 token

---

## 12. 值得注意的问题

### 12.1 `Login` 逻辑疑似写反

`StatusServiceImpl::Login` 中：

```cpp
bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
if (success) {
    reply->set_error(ErrorCodes::UidInvalid);
    return Status::OK;
}
```

这里按字面理解会把“查到 token”当成失败处理，明显不合理。

### 12.2 `getChatServer()` 里有重复比较

它先做了一轮带 Redis 查询的遍历，后面又做了一轮单纯比较 `_servers` 的遍历：

- 第一轮已经在更新 `con_count` 并比较最小值；
- 第二轮没有重新查 Redis，更多像冗余代码。

这不一定出错，但会增加阅读成本。

### 12.3 `_servers.begin()` 默认假设服务器列表非空

`getChatServer()` 开头直接取：

```cpp
auto minServer = _servers.begin()->second;
```

如果配置缺失或没有加载到任何聊天节点，这里会直接崩溃。

### 12.4 基础设施代码多于当前业务实际使用量

当前模块里存在：

- MySQL 连接池
- Asio 线程池
- Chat gRPC 客户端
- Redis 分布式锁

但主链路真正使用到的，主要只有：

- `ConfigMgr`
- `StatusServiceImpl`
- `RedisMgr`

阅读时不要被未启用代码分散注意力。

---

## 13. 推荐阅读顺序

建议按下面顺序阅读，效率最高：

1. [message.proto](/D:/Github/ChatProj/StatusServer/message.proto)
   先看对外暴露了什么 RPC 和消息结构。
2. [StatusServer.cpp](/D:/Github/ChatProj/StatusServer/StatusServer.cpp)
   了解服务如何启动、监听、退出。
3. [StatusServiceImpl.h](/D:/Github/ChatProj/StatusServer/StatusServiceImpl.h)
   看清核心服务类的接口与成员。
4. [StatusServiceImpl.cpp](/D:/Github/ChatProj/StatusServer/StatusServiceImpl.cpp)
   阅读主业务流程。
5. [config.ini](/D:/Github/ChatProj/StatusServer/config.ini)
   对照配置理解节点列表和依赖组件。
6. [RedisMgr.h](/D:/Github/ChatProj/StatusServer/RedisMgr.h)
7. [RedisMgr.cpp](/D:/Github/ChatProj/StatusServer/RedisMgr.cpp)
   看清 token 和负载数据是怎么存取的。
8. [const.h](/D:/Github/ChatProj/StatusServer/const.h)
   补齐错误码和 Redis key 约定。
9. [MysqlDao.cpp](/D:/Github/ChatProj/StatusServer/MysqlDao.cpp)
   最后再看未进入当前主链路的账号数据库能力。

---

## 14. 一句话总结

`StatusServer` 当前是一个轻量级 gRPC 状态服务，负责“为用户分配负载较低的 ChatServer，并通过 Redis 发放/校验登录 token”，真正的核心代码集中在 `StatusServiceImpl + RedisMgr + config.ini` 这一条链上。
