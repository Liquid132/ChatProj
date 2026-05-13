#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <grpcpp/grpcpp.h>
#include <atomic>

// grpc插件，连接通道
using grpc::Channel;
// 状态
using grpc::Status;
// 上下文
using grpc::ClientContext;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

class StatusConPool {
public:
	StatusConPool(size_t poolSize, std::string host, std::string port)
		: poolSize_(poolSize), host_(host), port_(port), b_stop_(false) {
		// 创建 poolSize_ 个 gRPC Channel
		// grpc::InsecureChannelCredentials()提供无加密、无认证的凭证对象
		for (size_t i = 0; i < poolSize_; ++i) {
			// 于端口地址创建一个不加密、无认证的grpc连接
			std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,
				grpc::InsecureChannelCredentials());

			connections_.push(StatusService::NewStub(channel));
		 }
	}

	~StatusConPool() {
		std::lock_guard<std::mutex> lock(mutex_);
		Close();
		while (!connections_.empty()) {
			connections_.pop();
		}
	}

	// 从连接池中取连接
	std::unique_ptr<StatusService::Stub> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock, [this] {
			if (b_stop_) {
				return true;
			}
			// connetsions_空，挂起
			return !connections_.empty();
			});
		// 遇上停止标记，返回空指针
		if (b_stop_) {
			return nullptr;
		}
		// 取指针
		auto context = std::move(connections_.front());
		connections_.pop();
		return context;
	}

	// 还连接
	void returnConnection(std::unique_ptr<StatusService::Stub> context) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (b_stop_) {
			return;
		}
		connections_.push(std::move(context));
		cond_.notify_one();
	}

	void Close() {
		b_stop_ = true;
		// 唤醒一个等待线程
		cond_.notify_all();
	}

private:
	std::atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	std::queue<std::unique_ptr<StatusService::Stub>> connections_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

// GRPC服务器
class StatusGrpcClient :public Singleton<StatusGrpcClient>
{
	friend class Singleton<StatusGrpcClient>;
public:
	~StatusGrpcClient() {

	}
	// 从状态服务器中获取聊天服务器的ip和token
	GetChatServerRsp GetChatServer(int uid);
	// LoginRsp Login(int uid, std::string token);

private:
	StatusGrpcClient();
	std::unique_ptr<StatusConPool> pool_;
};