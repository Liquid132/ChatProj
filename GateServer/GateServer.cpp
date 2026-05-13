#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "CServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"

void TestRedis() {
    //连接redis 需要启动才可以进行连接
//redis默认监听端口为6387 可以再配置文件中修改
    redisContext* c = redisConnect("127.0.0.1", 6380);
    if (c->err)
    {
        printf("Connect to redisServer faile:%s\n", c->errstr);
        redisFree(c);        return;
    }
    printf("Connect to redisServer Success\n");
    std::string redis_password = "123456";
    redisReply* r = (redisReply*)redisCommand(c, "AUTH %s", redis_password.c_str());
    if (r->type == REDIS_REPLY_ERROR) {
        printf("Redis认证失败！\n");
    }
    else {
        printf("Redis认证成功！\n");
    }
    //为redis设置key
    const char* command1 = "set stest1 value1";
    //执行redis命令行
    r = (redisReply*)redisCommand(c, command1);
    //如果返回NULL则说明执行失败
    if (NULL == r)
    {
        printf("Execut command1 failure\n");
        redisFree(c);        return;
    }
    //如果执行失败则释放连接
    if (!(r->type == REDIS_REPLY_STATUS && (strcmp(r->str, "OK") == 0 || strcmp(r->str, "ok") == 0)))
    {
        printf("Failed to execute command[%s]\n", command1);
        freeReplyObject(r);
        redisFree(c);        return;
    }
    //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
    freeReplyObject(r);
    printf("Succeed to execute command[%s]\n", command1);
    const char* command2 = "strlen stest1";
    r = (redisReply*)redisCommand(c, command2);
    //如果返回类型不是整形 则释放连接
    if (r->type != REDIS_REPLY_INTEGER)
    {
        printf("Failed to execute command[%s]\n", command2);
        freeReplyObject(r);
        redisFree(c);        return;
    }
    //获取字符串长度
    int length = r->integer;
    freeReplyObject(r);
    printf("The length of 'stest1' is %d.\n", length);
    printf("Succeed to execute command[%s]\n", command2);
    //获取redis键值对信息
    const char* command3 = "get stest1";
    r = (redisReply*)redisCommand(c, command3);
    if (r->type != REDIS_REPLY_STRING)
    {
        printf("Failed to execute command[%s]\n", command3);
        freeReplyObject(r);
        redisFree(c);        return;
    }
    printf("The value of 'stest1' is %s\n", r->str);
    freeReplyObject(r);
    printf("Succeed to execute command[%s]\n", command3);
    const char* command4 = "get stest2";
    r = (redisReply*)redisCommand(c, command4);
    if (r->type != REDIS_REPLY_NIL)
    {
        printf("Failed to execute command[%s]\n", command4);
        freeReplyObject(r);
        redisFree(c);        return;
    }
    freeReplyObject(r);
    printf("Succeed to execute command[%s]\n", command4);
    //释放连接资源
    redisFree(c);
}

void TestRedisMgr() {

    assert(RedisMgr::GetInstance()->Set("blogwebsite", "llfc.club"));
    std::string value = "";
    // 由于value被设置为空，下一行理应返回failed
    assert(RedisMgr::GetInstance()->Get("blogwebsite", value));
    assert(RedisMgr::GetInstance()->Get("nonekey", value) == false);
    assert(RedisMgr::GetInstance()->HSet("bloginfo", "blogwebsite", "llfc.club"));
    assert(RedisMgr::GetInstance()->HGet("bloginfo", "blogwebsite") != "");
    assert(RedisMgr::GetInstance()->ExistsKey("bloginfo"));
    assert(RedisMgr::GetInstance()->Del("bloginfo"));
    assert(RedisMgr::GetInstance()->Del("bloginfo"));
    assert(RedisMgr::GetInstance()->ExistsKey("bloginfo") == false);
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue1"));
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue2"));
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue3"));
    assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->LPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->LPop("lpushkey2", value) == false);
}

int main()
{
	// 测试redis
    //TestRedis();
	//TestRedisMgr();
    
    //ConfigMgr gCfgMgr;
    ConfigMgr& gCfgMgr= ConfigMgr::Inst();
    std::string gate_port_str = gCfgMgr["GateServer"]["Port"];
    unsigned short gate_port = atoi(gate_port_str.c_str());
    try {
        // 明确指定服务器监听的端口号。static_cast 确保端口号被正确地转换为 unsigned short 类型。
        unsigned short port = static_cast<unsigned short>(8080);
        // Boot.Asio核心调度器io_connect，使用一个线程来处理IO事件
        net::io_context ioc{ 1 };
        // 生成信号集，捕获操作系统发送给进程的信号
        // SIGINT(ctrl+C触发）,SIGTERM（系统终止）
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        // 异步等待。收到错误信号后触发一个回调（lambda表达式）
        signals.async_wait([&ioc](const boost::system::error_code& error, int signal_number) {
            if (error) {
                return;
            }

            ioc.stop();
            });
        
        /*
        创建服务器实例
        make_sahred在堆上创建一个CServer对象，使用shared_ptr管理其生命周期
        构造函数传入ioc和port，服务器绑定指定端口和调度器
        调用Start方法，异步接受客户端连接
        */
        std::make_shared<CServer>(ioc, port)->Start();
        std::cout << "GateServer listen on port: " << port << std::endl;
        // 启动 io_context 的事件循环。这个调用会阻塞当前线程（通常是主线程）。
        // 它会持续运行，处理所有的异步操作（如接受连接、读写数据、等待信号），直到以下情况发生：
        //   a) 所有工作都完成（没有未完成的异步操作）
        //   b) 显式调用了 ioc.stop()（如在信号处理程序中）
        ioc.run();
    }
    catch(std::exception const& e){
        // 将异常的错误信息打印到标准错误流
        std::cerr << "Error: " << e.what() << std::endl;
        // 返回一个非零的退出码（EXIT_FAILURE），表明程序是因错误而退出的
        return EXIT_FAILURE;
    }
}