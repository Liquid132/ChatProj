#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "const.h"
#include "ConfigMgr.h"
#include "hiredis.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "AsioIOServicePool.h"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include "StatusServiceImpl.h"

void RunServer() {
    auto& cfg = ConfigMgr::Inst();

    std::string server_address(cfg["StatusServer"]["Host"] + ":" + cfg["StatusServer"]["Port"]);
    StatusServiceImpl service;

    // 创建服务器入口
    grpc::ServerBuilder builder;
    // AddListeningPort监听一个TCP端口、添加服务，RegisterService表明不加密、不验证
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    // 构建、启动gRPC服务器
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    // 创建Boost.Asio的io_context
    boost::asio::io_context io_context;
    // 创建signal_set用于捕获SIGINT，其中
    // #define SIGINT          2   // interrupt
    // #define SIGTERM         15  // Software termination signal from kill
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);

    // 设置异步等待SIGINT信号，SIGINT使用ctrl+C触发
    // SIGTERM杀进程触发
    signals.async_wait([&server](const boost::system::error_code& error, int signal_number) {
        if (!error) {
            std::cout << "Shutting down server ... " << std::endl;
            server->Shutdown(); //Shutdown 会让 server->Wait() 返回，从而停止服务
        }
        });

    // 在单独的线程中运行io_context,监听系统信号
    std::thread([&io_context]() { io_context.run(); }).detach();
    // 阻塞主线程，等待shutdown（）被调用，服务器关闭
    server->Wait();
    io_context.stop(); // 停止io_context
}

int main(int argc, char** argv) {
    try {
        RunServer();
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
