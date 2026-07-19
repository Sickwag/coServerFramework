#include "utils/config.h"
#include "env.h"
#include "log.h"
#include <filesystem>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace {
static std::map<std::string, uint64_t> sFile2modifytime;
static azzato::Mutex				   sMutex;
}  // namespace

namespace azzato {

void listAllMember(const std::string&									prefix,
				   const YAML::Node&									node,
				   std::list<std::pair<std::string, const YAML::Node>>& output) {
	if(prefix.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678") != std::string::npos) {
		AZZATO_LOG_ERROR(systemLogger) << "Config invalid name: " << prefix << " : " << node;
		return;
	}
	output.push_back(std::make_pair(prefix, node));
	if(node.IsMap()) {
		for(auto it = node.begin(); it != node.end(); ++it) {
			listAllMember(
				prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
		}
	}
}

ConfigVarBase::ConfigVarBase(const std::string& name, const std::string& description)
	: _name(name)
	, _description(description) {}

void Config::loadFromYaml(const YAML::Node& root) {
	std::list<std::pair<std::string, const YAML::Node>> allNodes;
	listAllMember("", root, allNodes);

	for(auto& i : allNodes) {
		std::string key = i.first;
		if(key.empty()) {
			continue;
		}

		std::transform(key.begin(), key.end(), key.begin(), ::tolower);
		ConfigVarBase::ptr var = lookupBase(key);

		if(var) {
			if(i.second.IsScalar()) {
				var->fromString(i.second.Scalar());
			} else {
				std::stringstream ss;
				ss << i.second;
				var->fromString(ss.str());
			}
		}
	}
}

void Config::loadFromConfigDir(const std::string& path, bool force) {
	std::string				 absolutePath = azzato::EnvMgr::getInstance()->getAbsolutePath(path);
	std::vector<std::string> files;
	FSUtil::listAllFile(files, absolutePath, ".yml");

	for(auto& i : files) {
		{
			struct stat st;
			lstat(i.c_str(), &st);
			azzato::Mutex::Lock lock(sMutex);
			if(!force && sFile2modifytime[i] == (uint64_t)st.st_mtime) {
				continue;
			}
			sFile2modifytime[i] = st.st_mtime;
		}
		try {
			YAML::Node root = YAML::LoadFile(i);
			loadFromYaml(root);
			AZZATO_LOG_INFO(systemLogger) << "LoadConfFile file=" << i << " ok";
		} catch(...) {
			AZZATO_LOG_ERROR(systemLogger) << "LoadConfFile file=" << i << " failed";
		}
	}
}

ConfigVarBase::ptr Config::lookupBase(const std::string& name) {
	RWMutexType::ReadLock lock(getMutex());
	auto				  it = getDatas().find(name);
	return it == getDatas().end() ? nullptr : it->second;
}

void Config::visit(std::function<void(ConfigVarBase::ptr)> callback) {
	RWMutexType::ReadLock lock(getMutex());
	ConfigVarMap&		  m = getDatas();
	for(auto it = m.begin(); it != m.end(); ++it) {
		callback(it->second);
	}
}

Config::ConfigVarMap& Config::getDatas() {
	static Config::ConfigVarMap sDatas;
	return sDatas;
}

Config::RWMutexType& Config::getMutex() {
	static Config::RWMutexType sMutex;	// protect `sDatas`
	return sMutex;
}

}  // namespace azzato
