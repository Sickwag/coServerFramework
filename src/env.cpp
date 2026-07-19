#include "env.h"
#include "utils/config.h"
#include "log.h"
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

namespace azzato {

static azzato::Logger::ptr gLogger = AZZATO_LOG_NAME("system");

bool Env::init(int argc, char** argv) {
	std::string link	  = "/proc/self/exe";
	fs::path	fsExePath = fs::read_symlink(link);
	_exe				  = fsExePath.string();
	_cwd				  = fsExePath.parent_path().string() + "/";
	_program			  = argv[0];
	// -config /path/to/config -file xxxx -d
	const char* nowKey	  = nullptr;
	for(int i = 1; i < argc; ++i) {
		if(argv[i][0] == '-') {
			if(strlen(argv[i]) > 1) {
				if(nowKey) {
					add(nowKey, "");
				}
				nowKey = argv[i] + 1;
			} else {
				AZZATO_LOG_ERROR(systemLogger) << "invalid arg idx=" << i << " val=" << argv[i];
				return false;
			}
		} else {
			if(nowKey) {
				add(nowKey, argv[i]);
				nowKey = nullptr;
			} else {
				AZZATO_LOG_ERROR(systemLogger) << "invalid arg idx=" << i << " val=" << argv[i];
				return false;
			}
		}
	}
	if(nowKey) {
		add(nowKey, "");
	}
	return true;
}

void Env::add(const std::string& key, const std::string& val) {
	RWMutexType::WriteLock lock(_mutex);
	_args[key] = val;
}

bool Env::has(const std::string& key) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _args.find(key);
	return it != _args.end();
}

void Env::del(const std::string& key) {
	RWMutexType::WriteLock lock(_mutex);
	_args.erase(key);
}

std::string Env::get(const std::string& key, const std::string& defaultValue) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _args.find(key);
	return it != _args.end() ? it->second : defaultValue;
}

void Env::addHelp(const std::string& key, const std::string& desc) {
	removeHelp(key);
	RWMutexType::WriteLock lock(_mutex);
	_helps.push_back(std::make_pair(key, desc));
}

void Env::removeHelp(const std::string& key) {
	RWMutexType::WriteLock lock(_mutex);
	for(auto it = _helps.begin(); it != _helps.end();) {
		if(it->first == key) {
			it = _helps.erase(it);
		} else {
			++it;
		}
	}
}

void Env::printHelp() {
	RWMutexType::ReadLock lock(_mutex);
	std::cout << "Usage: " << _program << " [options]" << std::endl;
	for(auto& i : _helps) {
		std::cout << std::setw(5) << "-" << i.first << " : " << i.second << std::endl;
	}
}

bool Env::setEnv(const std::string& key, const std::string& val) {
	return !setenv(key.c_str(), val.c_str(), 1);
}

std::string Env::getEnv(const std::string& key, const std::string& default_value) {
	const char* v = getenv(key.c_str());
	if(v == nullptr) {
		return default_value;
	}
	return v;
}

std::string Env::getAbsolutePath(const std::string& path) const {
	if(path.empty()) {
		return "/";
	}
	if(path[0] == '/') {
		return path;
	}
	return _cwd + path;
}

std::string Env::getAbsoluteWorkPath(const std::string& path) const {
	if(path.empty()) {
		return "/";
	}
	if(path[0] == '/') {
		return path;
	}

	ConfigVar<std::string>::ptr gServerWorkPath = Config::lookup<std::string>("server.workPath");
	return gServerWorkPath->getValue() + "/" + path;
}

std::string Env::getConfigPath() { return getAbsolutePath(get("c", "conf")); }
}  // namespace azzato