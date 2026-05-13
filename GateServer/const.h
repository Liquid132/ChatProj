#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <functional>
#include <map>
#include <unordered_map>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <memory>
#include "Singleton.h"
#include "hiredis.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>#pragma once

// 定义错误，方便处理和返回给客户端
enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,		// JSON解析失败
	RPCFailed = 1002,		// RPC调用失败
	VarifyExpired = 1003,	// 验证码过期
	VarifyCodeErr = 1004,	// 验证码错误
	UserExist = 1005,		// 用户已存在
	PasswdErr = 1006,		// 密码错误
	EmailNotMatch = 1007,	// 邮箱不匹配
	PasswdUpFailed = 1008,	// 密码更新失败
	PasswdInvalid = 1009,	// 密码不符合规范
};

//class ConfigMgr;
//extern ConfigMgr gCfgMgr;

// Defer类，类似go的defer，延迟执行函数操作
class Defer {
public:
	// 构造时传入一个无参数、无返回值的函数对象，或者lambda表达式
	Defer(std::function<void()> func) : func_(func) {}

	//  析构时执行之前传入的函数
	~Defer() {
		func_();
	}

private:
	// 保存传入的函数
	std::function<void()> func_;
};

// 常量前缀,匹配key中的code字段
#define CODEPREFIX "code_"