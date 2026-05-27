#pragma once
#include "const.h"
#include "Singleton.h"
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <queue>
#include "data.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::GetChatServerRsp;
using message::LoginRsp;
using message::LoginReq;
using message::ChatService;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;

// ��֤���Ӱ�ȫ��
class ChatConPool {
public:
	ChatConPool(size_t poolSize, std::string host, std::string port) :
		poolSize_(poolSize), host_(host), port_(port), b_stop_(false) {
		for (size_t i = 0; i < poolSize_; ++i) {
			// GBK造成乱码
			// what's wrong?
			std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
			// ���� Channel ���� ChatService �Ŀͻ��� Stub
			// Stub ����ʵ�ʷ��� RPC ���ã��� SendMessage �ȣ�
			// �˴�push���Ĳ������ӣ����ǿ��Է���RPC���õ�Stub
			connections_.push(ChatService::NewStub(channel));
		}
	}

	~ChatConPool() {
		std::lock_guard<std::mutex> lock(mutex_);
		Close();
		while (!connections_.empty()) {
			connections_.pop();
		}
	}

	std::unique_ptr<ChatService::Stub> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock, [this] {
			if (b_stop_) {
				return true;
			}
			return !connections_.empty();
			});
		// ���ֹͣ��ֱ�ӷ��ؿ�ָ��
		if (b_stop_) {
			return  nullptr;
		}
		// ȡ�������е�һ������
		auto context = std::move(connections_.front());
		connections_.pop();
		return context;
	}

	void returnConnection(std::unique_ptr<ChatService::Stub> context) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (b_stop_) {
			return;
		}
		// context��Ϊunique_ptr���޷�ֱ�Ӹ��ƣ�ʹ��std::moveת������Ȩ����Ϊһ����ֵ
		connections_.push(std::move(context));
		cond_.notify_one();
	}

	void Close() {
		b_stop_ = true;
		cond_.notify_all();
	}

private:
	// �Ƿ�رճ���
	atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	// ʹ��unique_ptr��֤����Ψһ��
	std::queue<std::unique_ptr<ChatService::Stub>> connections_;
	// ��������֤�̰߳�ȫ
	std::mutex mutex_;
	std::condition_variable cond_;
};

// ��Ϊ�ͻ��ˣ�����ChatService��Grpc�ӿ�
class ChatGrpcClient : public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;

public:
	~ChatGrpcClient() {

	}

	AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq& req);
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req);
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue);
private:
	ChatGrpcClient();
	// ���ӳؼ��ϣ�ÿ��ip��Ӧһ�����ӳ�
	unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;
};
