#pragma once

#include "mutex.h"
#include "utils/singleton.h"
#include <map>
#include <string>

namespace azzato {

class Env {
  public:
	using RWMutexType = RWMutex;
	bool			init(int argc, char** argv);

	void		add(const std::string& key, const std::string& val);
	bool		has(const std::string& key);
	void		del(const std::string& key);
	std::string get(const std::string& key, const std::string& defaultValue = "");

	void addHelp(const std::string& key, const std::string& desc);
	void removeHelp(const std::string& key);
	void printHelp();

	const std::string& getExe() const { return _exe; }
	const std::string& getCwd() const { return _cwd; }

	bool		setEnv(const std::string& key, const std::string& val);
	std::string getEnv(const std::string& key, const std::string& defaultValue = "");

	std::string getAbsolutePath(const std::string& path) const;
	std::string getAbsoluteWorkPath(const std::string& path) const;
	std::string getConfigPath();

  private:
	RWMutexType										 _mutex;
	std::map<std::string, std::string>				 _args;
	std::vector<std::pair<std::string, std::string>> _helps;

	std::string _program;
	std::string _exe;
	std::string _cwd;
};

using EnvMgr = azzato::Singleton<Env>;

}  // namespace azzato