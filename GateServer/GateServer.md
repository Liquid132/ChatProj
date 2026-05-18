# GateServer 模块分析

## 1. 模块职责

`GateServer` 是系统的 HTTP 网关层，负责承接客户端的注册、验证码、登录、找回密码等请求，并将请求分发到后端依赖：

- `VerifyGrpcClient`：向验证码服务请求验证码，定义见 [message.proto](/D:/Github/ChatProj/GateServer/message.proto:5)
- `MysqlMgr` / `MysqlDao`：读写用户账号数据，核心实现见 [MysqlDao.cpp](/D:/Github/ChatProj/GateServer/MysqlDao.cpp:27)
- `RedisMgr`：读取验证码等缓存数据，核心实现见 [RedisMgr.cpp](/D:/Github/ChatProj/GateServer/RedisMgr.cpp:19)
- `StatusGrpcClient`：登录后向状态服务申请可用 ChatServer 与 token，核心实现见 [StatusGrpcClient.cpp](/D:/Github/ChatProj/GateServer/StatusGrpcClient.cpp:3)

从业务边界看，它不是聊天长连接服务器，而是“账号入口 + 服务发现入口”。

## 2. 程序入口

程序入口在 [GateServer.cpp](/D:/Github/ChatProj/GateServer/GateServer.cpp:109)。

主流程：

1. 初始化 `ConfigMgr::Inst()` 读取 `config.ini`，实现见 [ConfigMgr.cpp](/D:/Github/ChatProj/GateServer/ConfigMgr.cpp:3)
2. 创建主 `net::io_context ioc{1}`
3. 注册 `SIGINT` / `SIGTERM` 信号处理
4. 构造 `CServer` 并调用 `Start()`，见 [CServer.cpp](/D:/Github/ChatProj/GateServer/CServer.cpp:10)
5. `ioc.run()` 进入事件循环

注意：

- 代码先读取了 `GateServer.Port`，但实际监听端口写死为 `8080`，没有使用配置值，见 [GateServer.cpp](/D:/Github/ChatProj/GateServer/GateServer.cpp:116)

## 3. 核心类与核心文件

### 3.1 网络入口

- [CServer.h](/D:/Github/ChatProj/GateServer/CServer.h:9)
- [CServer.cpp](/D:/Github/ChatProj/GateServer/CServer.cpp:10)

职责：

- 持有 `tcp::acceptor`
- 异步接受 TCP 连接
- 为每个连接创建 `HttpConnection`

### 3.2 单连接处理

- [HttpConnection.h](/D:/Github/ChatProj/GateServer/HttpConnection.h:3)
- [HttpConnection.cpp](/D:/Github/ChatProj/GateServer/HttpConnection.cpp:9)

职责：

- 执行 `http::async_read`
- 解析 GET 参数
- 根据 HTTP 方法分发到 `LogicSystem`
- 组装 HTTP 响应并 `async_write`
- 使用 `steady_timer` 做 60 秒超时关闭

这是实际 HTTP 请求生命周期的中心。

### 3.3 业务路由层

- [LogicSystem.h](/D:/Github/ChatProj/GateServer/LogicSystem.h:7)
- [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:22)

职责：

- 维护 `_get_handlers` 和 `_post_handlers`
- 在构造函数中注册所有路由
- 把 URL 映射到具体业务 lambda

当前核心路由：

- `/get_varifycode`，见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:37)
- `/user_register`，见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:80)
- `/reset_pwd`，见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:162)
- `/user_login`，见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:238)

### 3.4 配置与基础设施

- [ConfigMgr.h](/D:/Github/ChatProj/GateServer/ConfigMgr.h:39)
- [config.ini](/D:/Github/ChatProj/GateServer/config.ini:1)
- [const.h](/D:/Github/ChatProj/GateServer/const.h:22)
- [Singleton.h](/D:/Github/ChatProj/GateServer/Singleton.h:1)

职责：

- 读取 INI 配置
- 定义错误码、常量、延迟释放工具 `Defer`
- 为 Redis / MySQL / gRPC 客户端提供单例能力

### 3.5 外部依赖封装

- MySQL： [MysqlMgr.h](/D:/Github/ChatProj/GateServer/MysqlMgr.h:5), [MysqlDao.h](/D:/Github/ChatProj/GateServer/MysqlDao.h:19), [MysqlDao.cpp](/D:/Github/ChatProj/GateServer/MysqlDao.cpp:4)
- Redis： [RedisMgr.h](/D:/Github/ChatProj/GateServer/RedisMgr.h:10), [RedisMgr.cpp](/D:/Github/ChatProj/GateServer/RedisMgr.cpp:5)
- 验证码 RPC： [VerifyGrpcClient.h](/D:/Github/ChatProj/GateServer/VerifyGrpcClient.h:17), [VerifyGrpcClient.cpp](/D:/Github/ChatProj/GateServer/VerifyGrpcClient.cpp:4)
- 状态服务 RPC： [StatusGrpcClient.h](/D:/Github/ChatProj/GateServer/StatusGrpcClient.h:18), [StatusGrpcClient.cpp](/D:/Github/ChatProj/GateServer/StatusGrpcClient.cpp:25)

## 4. 网络通信结构

### 4.1 HTTP 接入层

客户端通过 HTTP 访问 GateServer。

链路如下：

1. `main()` 创建 `CServer`
2. `CServer::Start()` 从 `AsioIOServicePool` 取一个 `io_context`
3. 为新连接创建 `HttpConnection`
4. `acceptor.async_accept()` 接收到连接后调用 `HttpConnection::Start()`
5. `HttpConnection::Start()` 执行 `http::async_read`
6. `HttpConnection::HandleReq()` 根据 `GET/POST` 调用 `LogicSystem`
7. 业务处理完成后 `WriteResponse()` 回写 HTTP 响应

其中连接读写线程来自 `AsioIOServicePool`，实现见 [AsioIOServicePool.cpp](/D:/Github/ChatProj/GateServer/AsioIOServicePool.cpp:6)。

### 4.2 gRPC 出站调用

GateServer 自身是 HTTP 服务器，但它向其他服务发起 gRPC 调用：

- `VerifyGrpcClient::GetVarifyCode()` 调用 `VarifyService.GetVarifyCode`，定义见 [message.proto](/D:/Github/ChatProj/GateServer/message.proto:5)
- `StatusGrpcClient::GetChatServer()` 调用 `StatusService.GetChatServer`，定义见 [message.proto](/D:/Github/ChatProj/GateServer/message.proto:41)

这说明 GateServer 是典型的“协议转换层”：外部 HTTP，内部 gRPC + Redis + MySQL。

### 4.3 Redis / MySQL 访问

- Redis 通过 `hiredis` 直连，连接池在 `RedisConPool`
- MySQL 通过 `mysql-connector-c++` 访问，连接池在 `MySqlPool`

这两类访问都是同步阻塞式调用，只是通过连接池减少建连成本。

## 5. 消息处理流程

### 5.1 `/get_varifycode`

入口见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:37)。

流程：

1. 读取 POST body
2. 解析 JSON，要求包含 `email`
3. 调用 `VerifyGrpcClient::GetVarifyCode(email)`
4. 将 RPC 返回的 `error` 和 `email` 封装回 JSON

特点：

- GateServer 本身不生成验证码
- 验证码生成职责完全在验证码服务

### 5.2 `/user_register`

入口见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:80)。

流程：

1. 解析 `user/email/passwd/confirm/varifycode`
2. 校验 `passwd == confirm`
3. 从 Redis 读取 `code_<email>` 验证码
4. 校验验证码是否过期、是否一致
5. 调用 `MysqlMgr::RegUser()` 注册用户
6. 返回 `uid`、用户信息和错误码

关键依赖：

- Redis 校验验证码，见 [RedisMgr.cpp](/D:/Github/ChatProj/GateServer/RedisMgr.cpp:19)
- MySQL 调用存储过程 `reg_user`，见 [MysqlDao.cpp](/D:/Github/ChatProj/GateServer/MysqlDao.cpp:27)

业务重点：

- Redis 只负责“短期校验状态”
- 用户主数据最终以 MySQL 为准

### 5.3 `/reset_pwd`

入口见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:162)。

流程：

1. 解析 `email/user/passwd/varifycode`
2. 从 Redis 读取验证码并校验
3. 用 `MysqlMgr::CheckEmail(name, email)` 校验用户名和邮箱是否匹配
4. 用 `MysqlMgr::UpdatePwd(name, pwd)` 更新密码
5. 返回结果

关键点：

- 先验证码，后账号归属校验，最后改密码

### 5.4 `/user_login`

入口见 [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:238)。

流程：

1. 解析 `email/passwd`
2. `MysqlMgr::CheckPwd(email, pwd, userInfo)` 校验账号密码
3. `StatusGrpcClient::GetChatServer(userInfo.uid)` 向状态服务申请可用聊天节点
4. 将 `uid/token/host/port` 返回给客户端

这是当前最关键的业务闭环，因为它串起了账号校验和后续聊天服务发现。

## 6. 线程模型

### 6.1 主线程

主线程运行 `main()` 中的 `ioc.run()`，负责：

- 监听退出信号
- 维护 `acceptor` 所在的主 `io_context`

### 6.2 I/O 线程池

`AsioIOServicePool` 默认创建 2 个 `io_context` 和 2 个线程，见 [AsioIOServicePool.h](/D:/Github/ChatProj/GateServer/AsioIOServicePool.h:24) 与 [AsioIOServicePool.cpp](/D:/Github/ChatProj/GateServer/AsioIOServicePool.cpp:6)。

用途：

- 每个新连接会绑定到池中的某个 `io_context`
- `HttpConnection` 的异步读写在这些线程上执行

结构上是：

- `acceptor` 在主 `ioc`
- socket I/O 在池内 `io_context`

这是“接入线程与连接处理线程分离”的模型。

### 6.3 后台维护线程

- `MySqlPool` 启动一个后台线程，每 60 秒执行连接保活，见 [MysqlDao.h](/D:/Github/ChatProj/GateServer/MysqlDao.h:45)
- `RedisConPool` 启动一个后台线程，周期性 `PING` 检查并尝试重连，见 [RedisMgr.h](/D:/Github/ChatProj/GateServer/RedisMgr.h:32)

### 6.4 阻塞点

虽然 HTTP 收发使用了异步模型，但业务处理阶段内部是同步阻塞的：

- JSON 解析同步执行
- Redis 命令同步执行
- MySQL 查询同步执行
- gRPC 调用同步执行

所以本质上它是“异步网络接入 + 同步业务处理”。

## 7. 配置系统

配置文件是 [config.ini](/D:/Github/ChatProj/GateServer/config.ini:1)，由 [ConfigMgr.cpp](/D:/Github/ChatProj/GateServer/ConfigMgr.cpp:3) 读取。

已定义配置：

- `GateServer.Port`
- `VarifyServer.Host` / `Port`
- `StatusServer.Host` / `Port`
- `MySQL.Host` / `Port` / `User` / `Passwd` / `Schema`
- `Redis.Host` / `Port` / `Passwd`

使用方式：

- `ConfigMgr::Inst()["Section"]["Key"]`

设计上比较轻量，适合当前项目规模，但有两个特点：

- 没有配置校验和默认值体系
- 返回值是字符串，业务层自行 `atoi` 或直接使用

## 8. 与其他服务器的交互方式

### 8.1 VerifyServer

通过 gRPC 调用 `VarifyService.GetVarifyCode`。

用途：

- 请求验证码生成或发送

代码位置：

- [VerifyGrpcClient.h](/D:/Github/ChatProj/GateServer/VerifyGrpcClient.h:109)
- [VerifyGrpcClient.cpp](/D:/Github/ChatProj/GateServer/VerifyGrpcClient.cpp:4)

### 8.2 StatusServer

通过 gRPC 调用 `StatusService.GetChatServer`。

用途：

- 登录成功后申请一个可用聊天节点
- 获取连接 `host/port` 和 `token`

代码位置：

- [StatusGrpcClient.cpp](/D:/Github/ChatProj/GateServer/StatusGrpcClient.cpp:3)

### 8.3 Redis

用途：

- 暂存验证码
- 提供短期状态查询

当前在 GateServer 已实际使用的主要是 `GET`

### 8.4 MySQL

用途：

- 用户注册
- 邮箱归属校验
- 密码更新
- 登录鉴权

MySQL 承担账号主存储角色。

## 9. 使用到的设计模式

### 9.1 单例模式

大量基础组件使用 `Singleton<T>`：

- `LogicSystem`
- `RedisMgr`
- `MysqlMgr`
- `VerifyGrpcClient`
- `StatusGrpcClient`
- `AsioIOServicePool`

优点是访问简单，缺点是全局状态较多，测试替换困难。

### 9.2 Reactor / Proactor 风格异步网络模型

基于 Boost.Asio / Beast：

- `async_accept`
- `async_read`
- `async_write`
- `steady_timer.async_wait`

这是整个 HTTP 网关的并发基础。

### 9.3 对象池 / 连接池

- `AsioIOServicePool`
- `RedisConPool`
- `MySqlPool`
- `RPConPool`
- `StatusConPool`

核心目标都是复用昂贵资源，减少重复创建。

### 9.4 路由表模式

`LogicSystem` 使用 `std::map<std::string, HttpHandler>` 保存 URL 到处理器的映射，本质上是轻量路由注册表。

### 9.5 RAII

`Defer` 用于在作用域结束时归还连接或执行清理，见 [const.h](/D:/Github/ChatProj/GateServer/const.h:26)。

## 10. 核心调用链

### 10.1 HTTP 总调用链

`main()`
-> `CServer::Start()`
-> `acceptor.async_accept()`
-> `HttpConnection::Start()`
-> `http::async_read()`
-> `HttpConnection::HandleReq()`
-> `LogicSystem::HandleGet()/HandlePost()`
-> 具体业务 lambda
-> `HttpConnection::WriteResponse()`
-> `http::async_write()`

### 10.2 登录调用链

`POST /user_login`
-> `LogicSystem` 登录处理器
-> `MysqlMgr::CheckPwd()`
-> `MysqlDao::CheckPwd()`
-> MySQL 查询 `user`
-> `StatusGrpcClient::GetChatServer(uid)`
-> `StatusService.GetChatServer`
-> 返回 `host/port/token`
-> 回包给客户端

### 10.3 注册调用链

`POST /user_register`
-> `LogicSystem` 注册处理器
-> `RedisMgr::Get("code_<email>")`
-> 校验验证码
-> `MysqlMgr::RegUser()`
-> `MysqlDao::RegUser()`
-> 存储过程 `reg_user`
-> 返回 `uid`
-> 回包给客户端

### 10.4 找回密码调用链

`POST /reset_pwd`
-> `LogicSystem` 重置密码处理器
-> `RedisMgr::Get("code_<email>")`
-> `MysqlMgr::CheckEmail()`
-> `MysqlMgr::UpdatePwd()`
-> 回包给客户端

## 11. 当前代码中的实现特征与注意点

### 11.1 业务核心集中在 `LogicSystem.cpp`

虽然文件名像“逻辑系统”，但实际上它承载了当前模块绝大多数核心业务流程。分析这个模块时，应优先读它，而不是先读连接池代码。

### 11.2 `GateServer` 是薄网关，不做复杂领域建模

业务代码基本是：

- 解析请求
- 做校验
- 调 Redis / MySQL / gRPC
- 组装响应

因此它的复杂度主要来自“外部依赖编排”，而不是领域对象设计。

### 11.3 配置读取与实际监听端口存在偏差

`main()` 读取了配置中的 `GateServer.Port`，但最终监听端口仍固定为 `8080`。这会让配置文件对监听端口失效。

### 11.4 业务处理在线程池线程中同步阻塞

如果登录、注册流量上来，阻塞式 MySQL / Redis / gRPC 调用会直接占住 `HttpConnection` 所在线程，吞吐上限主要受线程池大小和后端响应时间影响。

## 12. 推荐阅读顺序

1. [GateServer.cpp](/D:/Github/ChatProj/GateServer/GateServer.cpp:109)：先看入口和服务器启动方式
2. [CServer.cpp](/D:/Github/ChatProj/GateServer/CServer.cpp:10)：理解连接如何被 accept
3. [HttpConnection.cpp](/D:/Github/ChatProj/GateServer/HttpConnection.cpp:133)：看 HTTP 请求如何被解析和回包
4. [LogicSystem.cpp](/D:/Github/ChatProj/GateServer/LogicSystem.cpp:22)：这是业务主战场
5. [StatusGrpcClient.cpp](/D:/Github/ChatProj/GateServer/StatusGrpcClient.cpp:3) 和 [VerifyGrpcClient.h](/D:/Github/ChatProj/GateServer/VerifyGrpcClient.h:109)：看外部 RPC 依赖
6. [MysqlDao.cpp](/D:/Github/ChatProj/GateServer/MysqlDao.cpp:27)：看账号数据落库逻辑
7. [RedisMgr.cpp](/D:/Github/ChatProj/GateServer/RedisMgr.cpp:19)：看验证码与缓存读取
8. [AsioIOServicePool.cpp](/D:/Github/ChatProj/GateServer/AsioIOServicePool.cpp:6)：最后补并发模型
9. [ConfigMgr.cpp](/D:/Github/ChatProj/GateServer/ConfigMgr.cpp:3) 和 [config.ini](/D:/Github/ChatProj/GateServer/config.ini:1)：最后补配置来源

这个顺序更贴近真实调用链，不会一开始陷入工具类细节。
