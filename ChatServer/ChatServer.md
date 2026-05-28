# ChatServer 模块分析

## 1. 模块职责

`ChatServer` 是系统里的聊天接入与在线转发节点，主要职责有四类：

1. 接收客户端 TCP 长连接。
2. 解析自定义二进制包头 + JSON 包体协议。
3. 处理登录后聊天相关业务，包括搜索用户、好友申请、好友认证、文本聊天。
4. 在用户不在本机时，通过 gRPC 把通知转发到对端 `ChatServer`。

它不是登录入口。登录前的节点分配和 token 发放由 `StatusServer` 完成；`ChatServer` 负责“用户已经连进来之后”的在线业务。

对应核心文件：

- 入口：[ChatServer.cpp](/D:/Github/ChatProj/ChatServer/ChatServer.cpp)
- TCP 接入：[CServer.cpp](/D:/Github/ChatProj/ChatServer/CServer.cpp)
- 会话层：[CSession.cpp](/D:/Github/ChatProj/ChatServer/CSession.cpp)
- 业务分发：[LogicSystem.cpp](/D:/Github/ChatProj/ChatServer/LogicSystem.cpp)
- 跨服 gRPC 服务端：[ChatServiceImpl.cpp](/D:/Github/ChatProj/ChatServer/ChatServiceImpl.cpp)
- 跨服 gRPC 客户端：[ChatGrpcClient.cpp](/D:/Github/ChatProj/ChatServer/ChatGrpcClient.cpp)

---

## 2. 程序入口

程序入口在 [ChatServer.cpp](/D:/Github/ChatProj/ChatServer/ChatServer.cpp) 的 `main()`。

启动流程：

1. `ConfigMgr::Inst()` 读取 `config.ini`。
2. 读取 `[SelfServer]` 里的 `Name`，并在 Redis `logincount` 哈希中把当前节点登录数初始化为 `0`。
3. 启动 `AsioIOServicePool`，为每个会话分配异步读写线程。
4. 启动本机 gRPC 服务 `ChatServiceImpl`，监听 `[SelfServer].RPCPort`。
5. 创建主 `boost::asio::io_context`，注册 `SIGINT` / `SIGTERM`。
6. 创建 `CServer`，监听 `[SelfServer].Port`，开始接收 TCP 连接。
7. 主线程执行 `io_context.run()`。
8. 退出时删除 Redis 里的本节点 `logincount` 记录，关闭 Redis，并等待 gRPC 线程结束。

这个入口同时启动了两套网络面：

1. 面向客户端的 TCP 长连接。
2. 面向其他 `ChatServer` 的 gRPC 服务。

---

## 3. 核心类与核心文件

### 3.1 `CServer`

文件：

- [CServer.h](/D:/Github/ChatProj/ChatServer/CServer.h)
- [CServer.cpp](/D:/Github/ChatProj/ChatServer/CServer.cpp)

职责：

1. 监听 TCP 端口。
2. 接收新连接。
3. 管理当前节点上的所有 `CSession`。

关键成员：

- `_acceptor`
  TCP 监听器。
- `_sessions`
  保存 `session_id -> CSession` 的映射。
- `_mutex`
  保护 `_sessions`。

关键行为：

- `StartAccept()`
  从 `AsioIOServicePool` 取一个 `io_context`，创建 `CSession` 并异步接收连接。
- `HandleAccept()`
  接收成功后启动 session，并写入 `_sessions`。
- `ClearSession()`
  断线时移除 session，并通知 `UserMgr` 解除 `uid -> session` 绑定。

### 3.2 `CSession`

文件：

- [CSession.h](/D:/Github/ChatProj/ChatServer/CSession.h)
- [CSession.cpp](/D:/Github/ChatProj/ChatServer/CSession.cpp)

这是客户端连接的封装层，负责：

1. 保存 socket 和 session id。
2. 异步读取消息头和消息体。
3. 把完整消息投递给 `LogicSystem`。
4. 顺序发送响应和通知消息。

协议解析方式：

- 固定 4 字节头：
  - 前 2 字节：`msg_id`
  - 后 2 字节：`msg_len`
- 头部定义见 [const.h](/D:/Github/ChatProj/ChatServer/const.h)
- 头节点和数据节点定义见 [MsgNode.h](/D:/Github/ChatProj/ChatServer/MsgNode.h)

关键方法：

- `Start()`
  从读包头开始。
- `AsyncReadHead()`
  读取 4 字节头部，解析消息类型和包体长度。
- `AsyncReadBody()`
  读取完整包体，封装成 `LogicNode` 投递给逻辑线程。
- `Send()`
  把响应包装成 `SendNode` 放入发送队列，串行异步写回客户端。

### 3.3 `LogicSystem`

文件：

- [LogicSystem.h](/D:/Github/ChatProj/ChatServer/LogicSystem.h)
- [LogicSystem.cpp](/D:/Github/ChatProj/ChatServer/LogicSystem.cpp)

这是本模块最核心的业务调度中心。

职责：

1. 维护逻辑消息队列。
2. 将不同 `msg_id` 分发给对应业务处理函数。
3. 协调 Redis、MySQL、本机在线用户、跨服 gRPC。

关键成员：

- `_msg_que`
  业务消息队列。
- `_worker_thread`
  单独的逻辑线程。
- `_fun_callbacks`
  `msg_id -> handler` 的回调表。

当前注册的业务回调：

- `MSG_CHAT_LOGIN`
- `ID_SEARCH_USER_REQ`
- `ID_ADD_FRIEND_REQ`
- `ID_AUTH_FRIEND_REQ`
- `ID_TEXT_CHAT_MSG_REQ`

### 3.4 `ChatServiceImpl`

文件：

- [ChatServiceImpl.h](/D:/Github/ChatProj/ChatServer/ChatServiceImpl.h)
- [ChatServiceImpl.cpp](/D:/Github/ChatProj/ChatServer/ChatServiceImpl.cpp)

这是给其他 `ChatServer` 调用的 gRPC 服务端实现。

当前实现了三类跨服通知：

1. `NotifyAddFriend`
2. `NotifyAuthFriend`
3. `NotifyTextChatMsg`

它本身不处理复杂业务，只做“找到本机在线用户，然后直接推给其 TCP session”。

### 3.5 `ChatGrpcClient`

文件：

- [ChatGrpcClient.h](/D:/Github/ChatProj/ChatServer/ChatGrpcClient.h)
- [ChatGrpcClient.cpp](/D:/Github/ChatProj/ChatServer/ChatGrpcClient.cpp)

这是调用其他 `ChatServer` 的 gRPC 客户端。

职责：

1. 按 `[PeerServer]` 配置初始化对端连接池。
2. 当目标用户不在本机时，把好友申请、好友认证、文本消息转发给对端节点。

### 3.6 `UserMgr`

文件：

- [UserMgr.h](/D:/Github/ChatProj/ChatServer/UserMgr.h)
- [UserMgr.cpp](/D:/Github/ChatProj/ChatServer/UserMgr.cpp)

职责很单一：

1. 保存 `uid -> session`
2. 查询用户是否在线于本机
3. 断线时删除映射

它是“本机在线用户表”，不是全局在线状态中心。全局在线位置放在 Redis。

### 3.7 `MysqlMgr` / `MysqlDao`

文件：

- [MysqlMgr.h](/D:/Github/ChatProj/ChatServer/MysqlMgr.h)
- [MysqlDao.h](/D:/Github/ChatProj/ChatServer/MysqlDao.h)
- [MysqlDao.cpp](/D:/Github/ChatProj/ChatServer/MysqlDao.cpp)

当前主链路中实际使用到的数据库能力：

1. 查询用户资料
2. 保存好友申请
3. 把申请状态改为已认证
4. 查询申请列表
5. 查询好友列表

`AddFriend()` 虽然在 DAO 层实现了双向好友插入事务，但 `MysqlMgr.cpp` 里该转发函数被注释掉了，而 `LogicSystem::AuthFriendApply()` 仍调用 `MysqlMgr::GetInstance()->AddFriend(...)`。这会导致代码层面存在明显不一致，需要特别注意。

### 3.8 `RedisMgr`

文件：

- [RedisMgr.h](/D:/Github/ChatProj/ChatServer/RedisMgr.h)
- [RedisMgr.cpp](/D:/Github/ChatProj/ChatServer/RedisMgr.cpp)

这是在线状态和缓存的核心依赖。

当前主链路主要用到的 Redis key：

- `utoken_<uid>`
  登录 token。
- `uip_<uid>`
  用户当前所在 `ChatServer` 名称。
- `ubaseinfo_<uid>`
  用户基础资料缓存。
- `nameinfo_<name>`
  按用户名检索时的缓存。
- `logincount`
  各聊天节点当前登录人数。

---

## 4. 网络通信结构

`ChatServer` 同时扮演两个网络角色。

### 4.1 面向客户端

客户端通过 TCP 连接 `SelfServer.Port`。

通信协议不是 HTTP，也不是 protobuf，而是：

1. 自定义 4 字节头
2. JSON 字符串消息体

### 4.2 面向其他聊天节点

其他 `ChatServer` 通过 gRPC 调用本机 `ChatServiceImpl`。

用途：

1. 转发好友申请通知
2. 转发好友认证通知
3. 转发文本聊天通知

### 4.3 面向状态服务

当前模块提供了 [StatusGrpcClient.cpp](/D:/Github/ChatProj/ChatServer/StatusGrpcClient.cpp)，可调用 `StatusServer` 的：

1. `GetChatServer`
2. `Login`

但从当前 `ChatServer` 主链路代码看，这个客户端没有直接被 `LogicSystem` 使用，说明登录前置分配/校验可能由别的模块调用，或者是预留能力。

### 4.4 面向存储

- Redis：在线状态、token、资料缓存、节点计数
- MySQL：用户、好友申请、好友关系

可简化为：

```text
Client
  |
  | TCP + 自定义头 + JSON
  v
ChatServer
  | \
  |  \ gRPC
  |   -> Peer ChatServer
  |
  +--> Redis
  |
  +--> MySQL
```

---

## 5. 消息处理流程

## 5.1 TCP 收包总流程

入口在 `CSession`：

1. `Start()` 调用 `AsyncReadHead()`
2. 读取 4 字节头，解析 `msg_id` 和 `msg_len`
3. `AsyncReadBody()` 读取完整 JSON 包体
4. 封装成 `LogicNode(session, recvNode)`
5. 投递给 `LogicSystem::_msg_que`
6. `LogicSystem::DealMsg()` 在单线程中分发到具体 handler

所以网络 I/O 和业务逻辑是分离的：

- I/O 在线程池里并发执行
- 业务在单逻辑线程里串行处理

### 5.2 登录流程

处理函数：`LogicSystem::LoginHandler`

流程：

1. 从客户端 JSON 里读取 `uid` 和 `token`。
2. Redis `GET utoken_<uid>` 校验 token 是否存在。
3. token 不存在则返回 `UidInvalid`。
4. token 不匹配则返回 `TokenInvalid`。
5. 读取 `ubaseinfo_<uid>`，没有则查询 MySQL 并回填缓存。
6. 查询好友申请列表。
7. 查询好友列表。
8. Redis `HGET logincount <self_server>` 取当前登录数并加一。
9. 记录 `uip_<uid> = <self_server>`。
10. 调用 `UserMgr::SetUserSession(uid, session)` 绑定本机在线会话。
11. 返回完整登录响应给客户端。

这里能看出 `ChatServer` 的登录含义不是账号密码登录，而是“拿着 `StatusServer` 发的 token 进入聊天节点，并同步在线状态和好友数据”。

### 5.3 搜索用户流程

处理函数：`LogicSystem::SearchInfo`

流程：

1. 读取 `uid` 字段。
2. 如果全是数字，走 `GetUserByUid()`。
3. 否则走 `GetUserByName()`。
4. 优先 Redis 缓存，缓存没有再查 MySQL。
5. 返回用户基础资料。

### 5.4 添加好友申请流程

处理函数：`LogicSystem::AddFriendApply`

流程：

1. 解析 `uid`、`applyname`、`touid` 等字段。
2. 先写 MySQL：`AddFriendApply(uid, touid)`。
3. Redis 查询目标用户位置：`GET uip_<touid>`。
4. 如果对方不在线，流程结束，只保留数据库申请。
5. 如果对方在线且就在本机，直接从 `UserMgr` 取 session，发 `ID_NOTIFY_ADD_FRIEND_REQ`。
6. 如果对方在线但在其他节点，构造 `AddFriendReq`，通过 `ChatGrpcClient::NotifyAddFriend()` 跨服转发。

### 5.5 好友认证流程

处理函数：`LogicSystem::AuthFriendApply`

流程：

1. 解析 `fromuid`、`touid`、`back`。
2. 先回客户端一个成功响应。
3. 更新申请状态：`MysqlMgr::AuthFriendApply(uid, touid)`。
4. 继续尝试建立双向好友关系：`MysqlMgr::AddFriend(uid, touid, back_name)`。
5. Redis 查询 `uip_<touid>` 定位对方所在服务器。
6. 如果在本机，直接给对方 session 发送 `ID_NOTIFY_AUTH_FRIEND_REQ`。
7. 如果在其他节点，调用 `ChatGrpcClient::NotifyAuthFriend()`。

这里有一个重要现实问题：

- DAO 层实现了 `AddFriend`
- `LogicSystem` 也在调用
- 但 `MysqlMgr.cpp` 里 `AddFriend` 转发函数被注释掉了

说明当前工程代码很可能无法正常编译，或者处于半改动状态。

### 5.6 文本聊天流程

处理函数：`LogicSystem::DealChatTextMsg`

流程：

1. 读取 `fromuid`、`touid` 和 `text_array`。
2. 先给发送方回一个 `ID_TEXT_CHAT_MSG_RSP`。
3. Redis 查询 `uip_<touid>`。
4. 如果接收方不在线，当前代码直接返回，没有离线消息存储。
5. 如果接收方在线且在本机，直接发 `ID_NOTIFY_TEXT_CHAT_MSG_REQ`。
6. 如果接收方在线但在其他节点，封装 `TextChatMsgReq`，通过 `ChatGrpcClient::NotifyTextChatMsg()` 转发到对端节点。

当前文本聊天是纯在线转发模型，没有消息持久化队列。

---

## 6. 线程模型

`ChatServer` 的线程模型比 `StatusServer` 更复杂，分成四层。

### 6.1 主线程

主线程运行：

- `boost::asio::io_context`
- `tcp::acceptor`
- 进程信号处理

### 6.2 Asio I/O 线程池

[AsioIOServicePool.cpp](/D:/Github/ChatProj/ChatServer/AsioIOServicePool.cpp) 启动多个 `io_context.run()` 线程。

用途：

1. 承载各个 `CSession` 的异步读写
2. 把网络 I/O 从主 accept 线程中拆开

`CServer::StartAccept()` 每次新连接都会从池里 round-robin 取一个 `io_context`。

### 6.3 单逻辑线程

`LogicSystem` 构造时启动一个 `_worker_thread`，串行消费 `_msg_que`。

这是整个聊天业务逻辑的核心串行化点，优点是：

1. 业务代码简单
2. 状态竞争少

代价是：

1. 所有业务消息共用一个逻辑线程
2. 高并发下可能成为瓶颈

### 6.4 gRPC 服务线程

`ChatServer.cpp` 单独起一个线程执行 `server->Wait()`，承载跨服 RPC 服务。

真正的 RPC handler 并发由 gRPC runtime 自己管理。

### 6.5 Redis / MySQL 连接池保活线程

Redis 和 MySQL 连接池都各自有后台检查线程，周期性 `PING` 或 `SELECT 1` 保活并重连。

---

## 7. 配置系统

配置文件： [config.ini](/D:/Github/ChatProj/ChatServer/config.ini)

### 7.1 自身服务配置

```ini
[SelfServer]
Name = chatserver1
Host = 0.0.0.0
Port  = 8090
RPCPort = 50055
```

含义：

- `Port`：客户端 TCP 接入端口
- `RPCPort`：供其他 `ChatServer` 调用的 gRPC 端口
- `Name`：写入 Redis 的节点标识

### 7.2 状态服务配置

```ini
[StatusServer]
Host = 127.0.0.1
Port = 50052
```

用于 `StatusGrpcClient` 初始化连接池。

### 7.3 MySQL / Redis 配置

分别供 `MysqlDao` 和 `RedisMgr` 初始化连接池使用。

### 7.4 对端聊天节点配置

```ini
[PeerServer]
Servers = chatserver2

[chatserver2]
Name = chatserver2
Host = 127.0.0.1
Port = 50056
```

这里的 `Port` 实际对应的是对方 `ChatServer` 的 `RPCPort`，因为 `ChatGrpcClient` 是按 gRPC 访问对端的。

配置系统本身由 [ConfigMgr.cpp](/D:/Github/ChatProj/ChatServer/ConfigMgr.cpp) 实现，特点与其他模块一致：

1. 进程启动时一次性读入
2. 无热更新
3. 依赖当前工作目录存在 `config.ini`

---

## 8. 与其他服务的交互方式

### 8.1 与客户端

通过 TCP 长连接交互，自定义头 + JSON 包体。

### 8.2 与 `StatusServer`

通过 gRPC 交互，协议在 [message.proto](/D:/Github/ChatProj/ChatServer/message.proto) 的 `StatusService` 里定义。

当前客户端代码已具备：

1. `GetChatServer(int uid)`
2. `Login(int uid, std::string token)`

### 8.3 与其他 `ChatServer`

通过 gRPC 交互，协议在 `ChatService` 里定义。

当前实际使用的 RPC：

1. `NotifyAddFriend`
2. `NotifyAuthFriend`
3. `NotifyTextChatMsg`

### 8.4 与 Redis

Redis 是这个模块的在线状态中心和缓存层。

主要承担：

1. token 校验
2. 用户所在节点定位
3. 用户资料缓存
4. 节点登录数统计

### 8.5 与 MySQL

MySQL 是持久化事实来源。

主要承担：

1. 用户资料
2. 好友申请记录
3. 好友关系

---

## 9. 使用到的设计模式

### 9.1 单例模式

主要体现在：

- `LogicSystem`
- `UserMgr`
- `RedisMgr`
- `MysqlMgr`
- `ChatGrpcClient`
- `StatusGrpcClient`
- `AsioIOServicePool`
- `ConfigMgr`

### 9.2 Reactor / Proactor 风格异步 I/O

`boost::asio` 负责：

1. 异步 accept
2. 异步 read
3. 异步 write

`CSession` 用回调链完成收包和发包。

### 9.3 生产者-消费者模式

`CSession` 是消息生产者，`LogicSystem::_worker_thread` 是消费者。

### 9.4 对象池模式

连接池包括：

- `AsioIOServicePool`
- `RedisConPool`
- `MySqlPool`
- `ChatConPool`
- `StatusConPool`

### 9.5 RAII

`Defer` 被广泛用于：

1. Redis 连接归还
2. MySQL 连接归还
3. 响应自动发送

---

## 10. 核心调用链

### 10.1 启动链

```text
main
  -> ConfigMgr::Inst
  -> AsioIOServicePool::GetInstance
  -> RedisMgr::HSet(LOGIN_COUNT, self_name, "0")
  -> 启动 gRPC ChatServiceImpl
  -> 创建 CServer
  -> io_context.run
```

### 10.2 客户端消息处理链

```text
TCP accept
  -> CServer::HandleAccept
  -> CSession::Start
  -> CSession::AsyncReadHead
  -> CSession::AsyncReadBody
  -> LogicSystem::PostMsgToQue
  -> LogicSystem::DealMsg
  -> 具体 handler
  -> CSession::Send
```

### 10.3 登录链

```text
MSG_CHAT_LOGIN
  -> LogicSystem::LoginHandler
  -> RedisMgr::Get(utoken_<uid>)
  -> GetBaseInfo
     -> Redis 缓存 / MysqlMgr::GetUser
  -> MysqlMgr::GetApplyList
  -> MysqlMgr::GetFriendList
  -> RedisMgr::HGet/HSet(logincount)
  -> RedisMgr::Set(uip_<uid>, self_name)
  -> UserMgr::SetUserSession
```

### 10.4 跨服好友申请链

```text
ID_ADD_FRIEND_REQ
  -> LogicSystem::AddFriendApply
  -> MysqlMgr::AddFriendApply
  -> RedisMgr::Get(uip_<touid>)
  -> 本机: UserMgr::GetSession -> CSession::Send
  -> 跨机: ChatGrpcClient::NotifyAddFriend
         -> 对端 ChatServiceImpl::NotifyAddFriend
         -> 对端 UserMgr::GetSession -> CSession::Send
```

### 10.5 跨服文本消息链

```text
ID_TEXT_CHAT_MSG_REQ
  -> LogicSystem::DealChatTextMsg
  -> RedisMgr::Get(uip_<touid>)
  -> 本机: CSession::Send
  -> 跨机: ChatGrpcClient::NotifyTextChatMsg
         -> 对端 ChatServiceImpl::NotifyTextChatMsg
         -> 对端 CSession::Send
```

---

## 11. 当前模块的业务重点

如果只抓当前真正重要的业务逻辑，建议优先关注这三条线：

1. `LoginHandler`
   负责 token 校验、在线状态登记、好友列表加载。
2. `AddFriendApply` / `AuthFriendApply`
   负责好友关系申请与通知。
3. `DealChatTextMsg`
   负责文本消息的本机转发与跨服转发。

这个模块的核心不是复杂协议，而是“在线状态定位”：

1. 通过 Redis 知道某个用户在哪个 `ChatServer`
2. 如果在本机，直接通过 `UserMgr` 找到 session
3. 如果不在本机，通过 gRPC 通知对应节点再投递

也就是说，`ChatServer` 的业务模型本质上是：

- TCP 接入层
- 单线程逻辑分发层
- Redis 在线定位层
- 跨服 RPC 转发层

---

## 12. 值得注意的问题

### 12.1 `MysqlMgr::AddFriend` 声明/调用与实现不一致

`LogicSystem::AuthFriendApply()` 调用了：

```cpp
MysqlMgr::GetInstance()->AddFriend(uid, touid, back_name);
```

但 [MysqlMgr.cpp](/D:/Github/ChatProj/ChatServer/MysqlMgr.cpp) 中对应实现被注释掉了。

这会导致：

1. 编译失败，或者
2. 当前目录代码处于不完整状态

这是最明显的问题。

### 12.2 登录数只增不减

`LoginHandler()` 里会：

1. 读取 `logincount[self_name]`
2. `count++`
3. 回写 Redis

但当前启用代码里没有对断线进行对称 `count--`。  
`CServer` 里原本有定时器和断线清理相关逻辑，但已经整体注释。

结果是：

1. 节点负载统计可能持续偏大
2. `StatusServer` 的节点选择会越来越失真

### 12.3 离线消息未落库

文本消息如果目标用户不在线：

```cpp
bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
if (!b_ip) {
    return;
}
```

当前就直接返回，没有离线消息持久化。

### 12.4 异地登录踢人逻辑被整体注释

`LoginHandler()` 中原本有：

1. 分布式锁
2. 检查用户是否在其他服务器登录
3. 跨服踢下线

这些代码目前都被注释掉了。  
所以当前版本更接近“允许覆盖在线标记，但不真正清退旧连接”的状态。

### 12.5 心跳与 session 过期清理被注释

`CServer` 的定时器、`CSession` 的心跳时间、异常 session 回收都被注释。

这意味着当前连接生命周期主要依赖 socket 出错回调，缺少主动超时治理。

### 12.6 `StatusGrpcClient` 在当前主链路中未实际接入

代码具备调用 `StatusServer` 的能力，但当前 `ChatServer` 的核心业务路径没有直接用到它，说明模块边界可能仍在调整中。

---

## 13. 推荐阅读顺序

建议按下面顺序阅读：

1. [const.h](/D:/Github/ChatProj/ChatServer/const.h)
   先看消息 ID、头部结构、Redis key 约定。
2. [ChatServer.cpp](/D:/Github/ChatProj/ChatServer/ChatServer.cpp)
   了解 TCP 服务和 gRPC 服务是如何同时启动的。
3. [CServer.h](/D:/Github/ChatProj/ChatServer/CServer.h)
4. [CServer.cpp](/D:/Github/ChatProj/ChatServer/CServer.cpp)
   看连接接入与 session 生命周期。
5. [CSession.h](/D:/Github/ChatProj/ChatServer/CSession.h)
6. [CSession.cpp](/D:/Github/ChatProj/ChatServer/CSession.cpp)
   看协议解析与消息投递。
7. [LogicSystem.h](/D:/Github/ChatProj/ChatServer/LogicSystem.h)
8. [LogicSystem.cpp](/D:/Github/ChatProj/ChatServer/LogicSystem.cpp)
   这是主业务逻辑核心。
9. [ChatServiceImpl.cpp](/D:/Github/ChatProj/ChatServer/ChatServiceImpl.cpp)
   看跨服通知如何落到本机在线用户。
10. [ChatGrpcClient.cpp](/D:/Github/ChatProj/ChatServer/ChatGrpcClient.cpp)
    看跨服转发如何发出去。
11. [UserMgr.cpp](/D:/Github/ChatProj/ChatServer/UserMgr.cpp)
    看本机在线用户表。
12. [RedisMgr.cpp](/D:/Github/ChatProj/ChatServer/RedisMgr.cpp)
    看在线状态和缓存是怎么存的。
13. [MysqlDao.cpp](/D:/Github/ChatProj/ChatServer/MysqlDao.cpp)
    最后再看数据库事实来源。
14. [config.ini](/D:/Github/ChatProj/ChatServer/config.ini)
    对照理解多节点部署关系。

---

## 14. 一句话总结

`ChatServer` 当前是一个“TCP 长连接接入 + 单线程业务分发 + Redis 在线定位 + gRPC 跨服通知”的聊天节点，核心业务集中在 `CSession -> LogicSystem -> UserMgr/Redis/Mysql/ChatGrpcClient` 这条链上。
