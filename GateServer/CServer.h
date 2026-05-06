#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>#pragma once

class CServer :public std::enable_shared_from_this<CServer> {
public:
	// 提供io服务访问接口，给予上下文
	CServer(boost::asio::io_context& ioc, unsigned short& port);
	// 服务器启动
	void Start();
private:
	// 接收器_acceptor
	tcp::acceptor _acceptor;
	net::io_context& _ioc;
	//tcp::socket _socket;
};

//class CServer:public std::enable_shared_from_this<CServer>
//{
//public:
//    //提供io服务访问接口，给予上下文
//    CServer(boost::asio::io_context& ioc, unsigned short& port);
//    //服务器启动
//    void Start();
//private:
//    //接收器_acceptor
//    tcp::acceptor  _acceptor;
//    net::io_context& _ioc;
//    tcp::socket _socket;
//};
//
//void CServer::Start() {
//    auto self = shared_from_this();
//    //async_accept异步等待并接受一个传入的TCP连接请求
//    _acceptor.async_accept(_socket, [self](beast::error_code _ec) {
//        try {
//            //出错：放弃该链接，继续监听其他链接
//            if (_ec) {
//                self->Start();
//                return;
//            }
//
//            //没出错：创建新链接，并使用HttpConnection管理
//            std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
//
//            //继续监听
//            self->Start();
//        }
//        catch (std::exception& exp) {
//
//        }
//        });
//}
