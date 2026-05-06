#pragma once
#include "const.h"
#include "MysqlDao.h"

class MysqlMgr : public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	// 检验邮箱
	bool CheckEmail(const std::string& name, const std::string& email);
	// 更新密码
	bool UpdatePwd(const std::string& name, const std::string& pwd);
	// 检查用户密码
	bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);
private:
	MysqlMgr();
	// 数据库操作对象
	MysqlDao _dao;
};