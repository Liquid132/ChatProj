#include "CServer.h"
#include "HttpConnection.h"
#include "AsioIOServicePool.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port):_ioc(ioc),
_acceptor(ioc, tcp::endpoint(tcp::v4(), port))/*, _socket(ioc)*/{
	// tcp::v4服务器本地地址，port端口号
}

void CServer::Start() {
    auto self = shared_from_this();
    auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
	// 将io_context传给HttpConnect，后者将生成一个socket与其绑定，之后使用GetSocket获取该socket
	std::shared_ptr<HttpConnection> new_con = std::make_shared<HttpConnection>(io_context);
    // async_accept异步等待并接受一个传入的TCP连接请求
    _acceptor.async_accept(new_con->GetSocket(), [self, new_con](beast::error_code ec) {
        try {
            // 出错：放弃该链接，继续监听其他链接
            if (ec) {
                self->Start();
                return;
            }

            //// 创建新链接，并使用HttpConnection管理
            //std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
            // 直接启动
			new_con->Start();

            // 继续监听
            self->Start();
        }
        catch (std::exception& exp) {

        }
        });
}  