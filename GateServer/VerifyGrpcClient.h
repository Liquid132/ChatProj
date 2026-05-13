#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "const.h"
#include "Singleton.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVarifyReq;
using message::GetVarifyRsp;
using message::VarifyService;

// RPConPool：gRPC 连接池（RPC Connection Pool）
// -------------------------------------------
// 用于统一管理与服务器的 gRPC 连接（VarifyService::Stub），
// 通过维护一个线程安全的 Stub 队列，实现连接复用，
// 避免每次 RPC 调用时重新建立 TCP/HTTP2 通道的开销。
class RPConPool {
public:
	// 构造函数
	// poolSize：连接池中Stub（客户端连接）的数量
	// host、port：服务器地址和端口
	RPConPool(size_t poolSize, const std::string& host, std::string port)
		:poolSize_(poolSize), host_(host), port_(port), b_stop_(false) {
		// 初始化连接池
		for (size_t i = 0; i < poolSize_; ++i) {
			// 创建与服务器通道
			std::shared_ptr<Channel> channel = grpc::CreateChannel(host +
				":" + port, grpc::InsecureChannelCredentials());
			// 基于该通道生成一个VarifyService客户端Stub
			connections_.push(VarifyService::NewStub(channel));
		}
	}

	// 析构函数
	// 释放连接池资源，确保在销毁前所有线程退出等待
	~RPConPool() {
		std::lock_guard<std::mutex> lock(mutex_);
		Close(); // 通知所有等待线程停止
		// 清空连接队列
		while (!connections_.empty()) {
			connections_.pop();
		}
	}

	// 从连接池获取一个可用的 Stub
	// 若池为空，则阻塞等待直到有可用连接或被停止
	std::unique_ptr<VarifyService::Stub> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);

		// 条件变量等待：当池非空或停止标志被置位时唤醒
		cond_.wait(lock, [this] {
			if (b_stop_) return true;
			return !connections_.empty();
			});

		// 若连接池已关闭，则返回空指针
		if (b_stop_) {
			return nullptr;
		}

		// 从队列取出一个 Stub（转移所有权）
		auto context = std::move(connections_.front());
		connections_.pop();
		return context;
	}

	// 将使用完的 Stub 放回连接池
	// 允许其他线程继续复用该连接
	void returnConnection(std::unique_ptr<VarifyService::Stub> context) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (b_stop_) {
			// 若连接池已关闭，直接丢弃
			return;
		}
		// 将 Stub 放回池中，并唤醒一个等待线程
		connections_.push(std::move(context));
		cond_.notify_one();
	}

	// 关闭连接池
	// 将停止标志设为 true，并唤醒所有等待线程
	void Close() {
		b_stop_ = true;
		cond_.notify_all();
	}

private:
	// 停止标志，用于安全关闭连接池
	std::atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	// 连接池队列，存储可用的Stub
	std::queue<std::unique_ptr<VarifyService::Stub>> connections_;

	// 互斥锁，保护连接池队列的线程安全
	std::mutex mutex_;
	// 条件变量，实现等待、唤醒
	std::condition_variable cond_;
};

class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
	friend class Singleton<VerifyGrpcClient>;
public:
	GetVarifyRsp GetVarifyCode(std::string email) {
		// 客户端上下文context
		ClientContext context;
		GetVarifyRsp reply;
		GetVarifyReq request;
		// 将email设置进请求
		request.set_email(email);
		auto stub = pool_->getConnection();
		Status status = stub->GetVarifyCode(&context, request, &reply);
		if (status.ok()) {
			// 用完后将stub归还连接池
			pool_->returnConnection(std::move(stub));
			return reply;
		}
		else {
			pool_->returnConnection(std::move(stub));
			reply.set_error(ErrorCodes::RPCFailed);
			return reply;
		}
	}
private:
	// 智能指针会将channel加入stub_中，只要stub_纯在就不会释放
	VerifyGrpcClient();
	//{
		//std::shared_ptr<Channel> channel = grpc::CreateChannel("127.0.0.1:50051",
		//	grpc::InsecureChannelCredentials());
		//// 信使
		//stub_ = VarifyService::NewStub(channel);
	//}
	/*std::unique_ptr<VarifyService::Stub> stub_;*/
	std::unique_ptr<RPConPool> pool_;
};



