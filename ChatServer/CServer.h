#pragma once
#include <boost/asio.hpp>
#include "CSession.h"
#include <memory.h>
#include <map>
#include <mutex>
#include <boost/asio/steady_timer.hpp>

using boost::asio::ip::tcp;
class CServer:public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& io_context, short port);
	~CServer();
	// session无效时将其从map中清除
	void ClearSession(std::string);
	//根据uid获取session
	shared_ptr<CSession> GetSession(std::string);
	bool CheckValid(std::string);
	void on_timer(const boost::system::error_code& ec);
	void StartTimer();
	void StopTimer();
private:
	// 接收连接后开始处理回调
	void HandleAccept(shared_ptr<CSession>, const boost::system::error_code & error);
	// 开始接收连接
	void StartAccept();
	boost::asio::io_context &_io_context;
	short _port;
	tcp::acceptor _acceptor;
	std::map<std::string, shared_ptr<CSession>> _sessions;
	std::mutex _mutex;
	boost::asio::steady_timer _timer;
};

