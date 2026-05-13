#pragma once
#include "const.h"
#include <thread>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>

class SqlConnection {
public:
	SqlConnection(sql::Connection* con, int64_t lasttime) : _con(con), _last_oper_time(lasttime) {}
	// 通过智能指针管理连接资源
	std::unique_ptr<sql::Connection> _con;
	int64_t _last_oper_time;	// 记录最后一次操作时间
};

class MySqlPool {
public:
	MySqlPool(const std::string& url, const std::string& user,
		const std::string& pass, const std::string& schema, int poolSize) :
		url_(url), user_(user), pass_(pass), schema_(schema), poolSize_(poolSize), b_stop_(false) {
		try {
			for (int i = 0; i < poolSize_; ++i) {
				// 获取mysql驱动实例
				// MySQL_Driver是连接MySQL数据库的入口点，负责管理底层连接和协议
				sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
				// connect接受三个参数：hostname、username、password
				// connect方法建立到MySQL服务器的TCP连接，并进行身份验证
				auto* con = driver->connect(url_, user_, pass_);
				// 设置默认schema
				// 后续的SQL查询将默认在此schema上执行
				con->setSchema(schema_);
				// 获取当前时间戳，用于管理连接生命周期
				auto currentTime = std::chrono::system_clock::now().time_since_epoch();
				// 转换为毫秒数
				long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
				// 将连接包装为SqlConnection对象并放入连接池
				pool_.push(std::make_unique<SqlConnection>(con, timestamp));
			}

			// 启动定时检查连接线程
			_check_thread = std::thread([this]() {
				while (!b_stop_) {
					checkConnection();
					std::this_thread::sleep_for(std::chrono::seconds(60));
				}
				});

			// 分离线程，使其在后台运行
			_check_thread.detach();
		}
		catch (sql::SQLException& e) {
			// 处理异常，例如记录日志或重新抛出异常
			std::cout << "mysql pool init failed, error:" << e.what() << std::endl;
		}
	}

	void checkConnection() {
		// 上锁
		std::lock_guard<std::mutex> lock(mutex_);
		// 提前存储连接池大小
		int poolsize = pool_.size();
		// 获取当前时间戳,转换为毫秒数
		auto currentTime = std::chrono::system_clock::now().time_since_epoch();
		long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
		for (int i = 0; i < poolsize; i++) {
			// front返回队列头部元素的引用，但不移除它
			// move将其转换为右值引用，以便转移所有权
			// pop移除队列头部元素,此前已经通过move将其所有权转移到con变量
			auto con = std::move(pool_.front());
			pool_.pop();
			Defer defer([this, &con]() {
				pool_.push(std::move(con));
				});
			// 当前时间戳减去最后操作时间小于5秒，说明连接刚被使用过，跳过检查
			if (timestamp - con->_last_oper_time < 5) {
				continue;
			}

			try {
				std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
				stmt->executeQuery("SELECT 1");
				con->_last_oper_time = timestamp; // 更新最后操作时间
				//std::cout << "execute timier alive query, cur is " << timestamp << std::endl;
			}
			catch (sql::SQLException& e) {
				std::cout << "Error keeping connection alive: " << e.what() << std::endl;
				// 连接失效，重新建立连接替代旧连接
				sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
				auto* newcon = driver->connect(url_, user_, pass_);
				newcon->setSchema(schema_);
				con->_con.reset(newcon);
				con->_last_oper_time = timestamp;
			}
		}
	}

	std::unique_ptr<SqlConnection> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);
		// 等待直到有可用连接或池被关闭
		cond_.wait(lock, [this] {
			if (b_stop_) {
				return true;
			}
			return !pool_.empty();
			});
		// 如果池已关闭，不再取连接，返回空指针
		if (b_stop_) {
			return nullptr;
		}
		// 取头部连接并返回
		std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
		pool_.pop();
		return con;
	}

	void returnConnection(std::unique_ptr<SqlConnection> con){
		std::unique_lock<std::mutex> lock(mutex_);
		if(b_stop_) {
			return;
		}
		// 使用move将连接变为右值引用，转移所有权到连接池
		pool_.push(std::move(con));
		// 通知等待线程有可用连接
		cond_.notify_one();
	}

	void Close() {
		b_stop_ = true;
		// 唤醒所有等待线程，防止死锁
		cond_.notify_all();
	}

	~MySqlPool() {
		std::unique_lock<std::mutex> lock(mutex_);
		while(!pool_.empty()) {
			pool_.pop();
		}
	}

private:
	std::string url_;	// 数据库地址
	std::string user_;	// 认证用户名
	std::string pass_;	// 认证密码
	std::string schema_;// 默认数据库
	int poolSize_;
	std::queue<std::unique_ptr<SqlConnection>> pool_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::atomic<bool> b_stop_;
	std::thread _check_thread;	// 定时检查连接线程
};

struct UserInfo {
	std::string name;
	std::string pwd;
	int uid;
	std::string email;
};

// 对数据库进行直接读写，增删改查
class MysqlDao
{
public:
	MysqlDao();
	~MysqlDao();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);
	bool CheckPwd(const std::string& email, const std::string& pwd, UserInfo& userInfo);
private:
	std::unique_ptr<MySqlPool> pool_;
};

