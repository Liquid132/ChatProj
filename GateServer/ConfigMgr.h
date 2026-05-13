#pragma once
#include "const.h"   

// 对应.ini文件中的端口、端口号
struct SectionInfo {
	SectionInfo(){}
	~SectionInfo() { _section_datas.clear(); }

	// 重载拷贝
	SectionInfo(const SectionInfo& src) {
		_section_datas = src._section_datas;
	}
	// 重载赋值
	SectionInfo& operator = (const SectionInfo& src) {
		if (&src == this) {
			// 不允许自己拷贝自己
			return *this;
		}

		this->_section_datas = src._section_datas;
		return *this;
	}

	std::map<std::string, std::string> _section_datas;
	// 使用map管理section的key值和对应value
	// 重载[]。对运算符重构需要使用“operator”
	std::string operator[](const std::string& key) {
		// 当key不存在时，返回空字符串
		if (_section_datas.find(key) == _section_datas.end()) {
			return "";
		}

		return _section_datas[key];
	}
};


// 对应.ini文件中的服务器名
class ConfigMgr
{
public:
	~ConfigMgr() {
		_config_map.clear();
	}

	SectionInfo operator[](const std::string& section) {
		if (_config_map.find(section) == _config_map.end()) {
			// 返回一个空的SectionInfo
			return SectionInfo();
		}

		return _config_map[section];
	}

	// 获取单例，多线程访问Inst函数时生成一个局部静态变量，确保线程安全
	// static使得Inst成为类的静态成员函数，可以不依赖类实例调用
	// (如果 Inst() 不是静态的，就必须先有一个 ConfigMgr 对象才能调用，
	// 但单例模式的本意正是“我们还没有对象，要靠它来生成第一个对象”，所以
	// 函数必须是 static)
	static ConfigMgr& Inst() {
		// cfg_mgr只在第一次调用时创建；
		// 即使多个线程同时调用Inst函数它也只会创建一个实例；
		// static确保该变量的生命周期贯穿程序始终
		static ConfigMgr cfg_mgr;
		return cfg_mgr;
	}

	ConfigMgr(const ConfigMgr& src) {
		_config_map = src._config_map;
	}
	
	ConfigMgr& operator = (const ConfigMgr& src) {
		if (&src == this) {
			return *this;
		}
		_config_map = src._config_map;
		return *this;
	}

private:
	std::map<std::string, SectionInfo> _config_map;
	ConfigMgr();
};

