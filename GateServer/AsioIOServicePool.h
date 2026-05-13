#pragma once
#include <vector>
#include <boost/asio.hpp>
#include "Singleton.h"

class AsioIOServicePool : public Singleton<AsioIOServicePool>
{
	friend class Singleton<AsioIOServicePool>;
public:
	// 旧版本中的io_service现在被io_context取代
	using IOService = boost::asio::io_context;
	// Work绑定io_context确保在没有任务时不会退出run循环
	using Work = boost::asio::io_context::work;
	using WorkPtr = std::unique_ptr<Work>;
	~AsioIOServicePool();
	AsioIOServicePool(const AsioIOServicePool&) = delete;
	AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;
	// 使用round-robin算法返回一个io_service
	boost::asio::io_context& GetIOService();
	// Stop回收资源并令挂起的线程唤醒
	void Stop();
private:
	// 使用2个线程
	AsioIOServicePool(std::size_t size = 2/*std::thread::hardware_concurrency()*/);
	// WorkPtr与IOService一一对应
	std::vector<IOService> _ioServices;
	std::vector<WorkPtr> _works;
	// 线程池
	std::vector<std::thread> _threads;
	// 下一个io_service索引
	std::size_t _nextIOService;
};