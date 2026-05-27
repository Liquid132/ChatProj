#include "UserMgr.h"
#include "CSession.h"
#include "RedisMgr.h"

UserMgr:: ~UserMgr() {
	_uid_to_session.clear();
}

// 根据uid获取session（连接）
std::shared_ptr<CSession> UserMgr::GetSession(int uid)
{
	// 不同于连接池通过线程管理，需要效率，session管理通过加锁保证线程安全
	std::lock_guard<std::mutex> lock(_session_mtx);
	auto iter = _uid_to_session.find(uid);
	if (iter == _uid_to_session.end()) {
		return nullptr;
	}

	return iter->second;
}

void UserMgr::SetUserSession(int uid, std::shared_ptr<CSession> session)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	_uid_to_session[uid] = session;
}

void UserMgr::RmvUserSession(int uid)
{
	auto uid_str = std::to_string(uid);
	// 再次登录有可能在其他服务器，会造成本服务器删除key，其他服务器注册key
	// 有可能其他服务器登录本服务器删除key，找不到key
	{
		std::lock_guard<std::mutex> lock(_session_mtx);
		_uid_to_session.erase(uid);
	}
}

UserMgr::UserMgr()
{

}
