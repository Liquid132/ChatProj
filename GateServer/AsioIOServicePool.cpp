#include "AsioIOServicePool.h"
#include <iostream>
using namespace std;

// io_context没有默认构造函数，需要在初始化列表中初始化
AsioIOServicePool::AsioIOServicePool(std::size_t size) : _ioServices(size),
_works(size), _nextIOService(0) {
	for (std::size_t i = 0; i < size; ++i) {
		// 为每个io_service创建一个work对象，防止io_service.run()提前退出
		_works[i] = std::unique_ptr<Work>(new Work(_ioServices[i]));
	}

	// 创建线程池，启动线程运行io_service的事件循环
	for (std::size_t i = 0; i < _ioServices.size(); ++i) {
		_threads.emplace_back([this, i]() {
			_ioServices[i].run();
			});
	}
}

AsioIOServicePool::~AsioIOServicePool()
{
	Stop();
	std::cout << "this is AsioIOServicePool destruct" << std::endl;
}

boost::asio::io_context& AsioIOServicePool::GetIOService() {
	// 让每个io_service轮流工作
	auto& service = _ioServices[_nextIOService++];
	if (_nextIOService == _ioServices.size()) {
		_nextIOService = 0;
	}
	return service;
}

void AsioIOServicePool::Stop() {
	//因为仅仅执行work.reset并不能让iocontext从run的状态中退出
	//当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。
	for (auto& work : _works) {
		//把服务先停止
		work->get_io_context().stop();
		work.reset();
	}

	// 等待所有线程退出
	for (auto& t : _threads) {
		t.join();
	}
}