#include "module.h"
#include "application.h"
#include "env.h"
#include "library.h"
#include "log.h"
#include "utils/config.h"
#include "utils/util.h"

namespace azzato {

static azzato::ConfigVar<std::string>::ptr g_module_path =
	Config::lookup("module.path", std::string("module"), "module path");

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

Module::Module(const std::string& name,
			   const std::string& version,
			   const std::string& filename,
			   uint32_t			  type)
	: _name(name)
	, _version(version)
	, _filename(filename)
	, _id(name + "/" + version)
	, _type(type) {}

void Module::onBeforeArgsParse(int argc, char** argv) {}

void Module::onAfterArgsParse(int argc, char** argv) {}

bool Module::handleRequest(azzato::Message::ptr req, azzato::Message::ptr rsp, azzato::Stream::ptr stream) {
	AZZATO_LOG_DEBUG(g_logger) << "handleRequest req=" << req->toString() << " rsp=" << rsp->toString()
							   << " stream=" << stream;
	return true;
}

bool Module::handleNotify(azzato::Message::ptr notify, azzato::Stream::ptr stream) {
	AZZATO_LOG_DEBUG(g_logger) << "handleNotify nty=" << notify->toString() << " stream=" << stream;
	return true;
}

bool Module::onLoad() { return true; }

bool Module::onUnload() { return true; }

bool Module::onConnect(azzato::Stream::ptr stream) { return true; }

bool Module::onDisconnect(azzato::Stream::ptr stream) { return true; }

bool Module::onServerReady() { return true; }

bool Module::onServerUp() { return true; }

void Module::registerService(const std::string& server_type,
							 const std::string& domain,
							 const std::string& service) {
	auto sd = Application::getInstance()->getServiceDiscovery();
	if(!sd) {
		return;
	}
	std::vector<TcpServer::ptr> svrs;
	if(!Application::getInstance()->getServer(server_type, svrs)) {
		return;
	}
	for(auto& i : svrs) {
		auto socks = i->getSocks();
		for(auto& s : socks) {
			auto addr = std::dynamic_pointer_cast<IPv4Address>(s->getLocalAddress());
			if(!addr) {
				continue;
			}
			auto str = addr->toString();
			if(str.find("127.0.0.1") == 0) {
				continue;
			}
			std::string ip_and_port;
			if(str.find("0.0.0.0") == 0) {
				ip_and_port = azzato::getIPv4() + ":" + std::to_string(addr->getPort());
			} else {
				ip_and_port = addr->toString();
			}
			sd->registerServer(domain, service, ip_and_port, server_type);
		}
	}
}

std::string Module::statusString() {
	std::stringstream ss;
	ss << "Module name=" << getName() << " version=" << getVersion() << " filename=" << getFilename()
	   << std::endl;
	return ss.str();
}

RockModule::RockModule(const std::string& name, const std::string& version, const std::string& filename)
	: Module(name, version, filename, ROCK) {}

bool RockModule::handleRequest(azzato::Message::ptr req,
							   azzato::Message::ptr rsp,
							   azzato::Stream::ptr	stream) {
	auto rock_req	 = std::dynamic_pointer_cast<azzato::RockRequest>(req);
	auto rock_rsp	 = std::dynamic_pointer_cast<azzato::RockResponse>(rsp);
	auto rock_stream = std::dynamic_pointer_cast<azzato::RockStream>(stream);
	return handleRockRequest(rock_req, rock_rsp, rock_stream);
}

bool RockModule::handleNotify(azzato::Message::ptr notify, azzato::Stream::ptr stream) {
	auto rock_nty	 = std::dynamic_pointer_cast<azzato::RockNotify>(notify);
	auto rock_stream = std::dynamic_pointer_cast<azzato::RockStream>(stream);
	return handleRockNotify(rock_nty, rock_stream);
}

ModuleManager::ModuleManager() {}

Module::ptr ModuleManager::get(const std::string& name) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _modules.find(name);
	return it == _modules.end() ? nullptr : it->second;
}

void ModuleManager::add(Module::ptr m) {
	del(m->getId());
	RWMutexType::WriteLock lock(_mutex);
	_modules[m->getId()]					= m;
	_type2Modules[m->getType()][m->getId()] = m;
}

void ModuleManager::del(const std::string& name) {
	Module::ptr			   module;
	RWMutexType::WriteLock lock(_mutex);
	auto				   it = _modules.find(name);
	if(it == _modules.end()) {
		return;
	}
	module = it->second;
	_modules.erase(it);
	_type2Modules[module->getType()].erase(module->getId());
	if(_type2Modules[module->getType()].empty()) {
		_type2Modules.erase(module->getType());
	}
	lock.unlock();
	module->onUnload();
}

void ModuleManager::delAll() {
	RWMutexType::ReadLock lock(_mutex);
	auto				  tmp = _modules;
	lock.unlock();

	for(auto& i : tmp) {
		del(i.first);
	}
}

void ModuleManager::init() {
	auto path = EnvMgr::getInstance()->getAbsolutePath(g_module_path->getValue());

	std::vector<std::string> files;
	azzato::FSUtil::listAllFile(files, path, ".so");

	std::sort(files.begin(), files.end());
	for(auto& i : files) {
		initModule(i);
	}
}

void ModuleManager::listByType(uint32_t type, std::vector<Module::ptr>& ms) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _type2Modules.find(type);
	if(it == _type2Modules.end()) {
		return;
	}
	for(auto& i : it->second) {
		ms.push_back(i.second);
	}
}

void ModuleManager::foreach(uint32_t type, std::function<void(Module::ptr)> cb) {
	std::vector<Module::ptr> ms;
	listByType(type, ms);
	for(auto& i : ms) {
		cb(i);
	}
}

void ModuleManager::onConnect(Stream::ptr stream) {
	std::vector<Module::ptr> ms;
	listAll(ms);

	for(auto& m : ms) {
		m->onConnect(stream);
	}
}

void ModuleManager::onDisconnect(Stream::ptr stream) {
	std::vector<Module::ptr> ms;
	listAll(ms);

	for(auto& m : ms) {
		m->onDisconnect(stream);
	}
}

void ModuleManager::listAll(std::vector<Module::ptr>& ms) {
	RWMutexType::ReadLock lock(_mutex);
	for(auto& i : _modules) {
		ms.push_back(i.second);
	}
}

void ModuleManager::initModule(const std::string& path) {
	Module::ptr m = Library::GetModule(path);
	if(m) {
		add(m);
	}
}

}  // namespace azzato
