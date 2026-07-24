#include "streams/service_discovery.h"
#include "log.h"
#include "utils/hash_util.h"
#include "utils/util.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace azzato {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

ServiceItemInfo::ptr ServiceItemInfo::Create(const std::string& ip_and_port, const std::string& data) {
	auto pos = ip_and_port.find(':');
	if(pos == std::string::npos) {
		return nullptr;
	}
	auto	  ip	  = ip_and_port.substr(0, pos);
	auto	  port	  = azzato::TypeUtil::atoi(ip_and_port.substr(pos + 1));
	in_addr_t ip_addr = inet_addr(ip.c_str());
	if(ip_addr == 0) {
		return nullptr;
	}

	ServiceItemInfo::ptr rt(new ServiceItemInfo);
	rt->_id	  = ((uint64_t)ip_addr << 32) | port;
	rt->_ip	  = ip;
	rt->_port = port;
	rt->_data = data;
	return rt;
}

std::string ServiceItemInfo::toString() const {
	std::stringstream ss;
	ss << "[ServiceItemInfo id=" << _id << " ip=" << _ip << " port=" << _port << " data=" << _data << "]";
	return ss.str();
}

void IServiceDiscovery::setQueryServer(
	const std::unordered_map<std::string, std::unordered_set<std::string>>& v) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_queryInfos = v;
}

void IServiceDiscovery::registerServer(const std::string& domain,
									   const std::string& service,
									   const std::string& ip_and_port,
									   const std::string& data) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_registerInfos[domain][service][ip_and_port] = data;
}

void IServiceDiscovery::queryServer(const std::string& domain, const std::string& service) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_queryInfos[domain].insert(service);
}

void IServiceDiscovery::listServer(
	std::unordered_map<std::string,
					   std::unordered_map<std::string, std::unordered_map<uint64_t, ServiceItemInfo::ptr>>>&
		infos) {
	azzato::RWMutex::ReadLock lock(_mutex);
	infos = _datas;
}

void IServiceDiscovery::listRegisterServer(
	std::unordered_map<std::string,
					   std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>&
		infos) {
	azzato::RWMutex::ReadLock lock(_mutex);
	infos = _registerInfos;
}

void IServiceDiscovery::listQueryServer(
	std::unordered_map<std::string, std::unordered_set<std::string>>& infos) {
	azzato::RWMutex::ReadLock lock(_mutex);
	infos = _queryInfos;
}

ZKServiceDiscovery::ZKServiceDiscovery(const std::string& hosts)
	: _hosts(hosts) {}

void ZKServiceDiscovery::start() {
	if(_client) {
		return;
	}
	auto self = shared_from_this();
	_client.reset(new azzato::ZKClient);
	bool b = _client->init(_hosts,
						   6000,
						   std::bind(&ZKServiceDiscovery::onWatch,
									 self,
									 std::placeholders::_1,
									 std::placeholders::_2,
									 std::placeholders::_3,
									 std::placeholders::_4));
	if(!b) {
		AZZATO_LOG_ERROR(g_logger) << "ZKClient init fail, hosts=" << _hosts;
	}
	_timer = azzato::IOManager::getThis()->addTimer(
		60 * 1000,
		[self, this]() {
			_isOnTimer = true;
			onZKConnect("", _client);
			_isOnTimer = false;
		},
		true);
}

void ZKServiceDiscovery::stop() {
	if(_client) {
		_client->close();
		_client = nullptr;
	}
	if(_timer) {
		_timer->cancel();
		_timer = nullptr;
	}
}

void ZKServiceDiscovery::onZKConnect(const std::string& path, ZKClient::ptr client) {
	azzato::RWMutex::ReadLock lock(_mutex);
	auto					  rinfo = _registerInfos;
	auto					  qinfo = _queryInfos;
	lock.unlock();

	bool ok = true;
	for(auto& i : rinfo) {
		for(auto& x : i.second) {
			for(auto& v : x.second) {
				ok &= registerInfo(i.first, x.first, v.first, v.second);
			}
		}
	}

	if(!ok) {
		AZZATO_LOG_ERROR(g_logger) << "onZKConnect register fail";
	}

	ok = true;
	for(auto& i : qinfo) {
		for(auto& x : i.second) {
			ok &= queryInfo(i.first, x);
		}
	}
	if(!ok) {
		AZZATO_LOG_ERROR(g_logger) << "onZKConnect query fail";
	}

	ok = true;
	for(auto& i : qinfo) {
		for(auto& x : i.second) {
			ok &= queryData(i.first, x);
		}
	}

	if(!ok) {
		AZZATO_LOG_ERROR(g_logger) << "onZKConnect queryData fail";
	}
}

bool ZKServiceDiscovery::existsOrCreate(const std::string& path) {
	int32_t v = _client->exists(path, false);
	if(v == ZOK) {
		return true;
	} else {
		auto pos = path.find_last_of('/');
		if(pos == std::string::npos) {
			AZZATO_LOG_ERROR(g_logger) << "existsOrCreate invalid path=" << path;
			return false;
		}
		if(pos == 0 || existsOrCreate(path.substr(0, pos))) {
			std::string new_val(1024, 0);
			v = _client->create(path, "", new_val);
			if(v != ZOK) {
				AZZATO_LOG_ERROR(g_logger)
					<< "create path=" << path << " error:" << zerror(v) << " (" << v << ")";
				return false;
			}
			return true;
		}
		// if(pos == 0) {
		//     std::string new_val(1024, 0);
		//     if(_client->create(path, "", new_val) != ZOK) {
		//         return false;
		//     }
		//     return true;
		// }
	}
	return false;
}

static std::string GetProvidersPath(const std::string& domain, const std::string& service) {
	return "/sylar/" + domain + "/" + service + "/providers";
}

static std::string GetConsumersPath(const std::string& domain, const std::string& service) {
	return "/sylar/" + domain + "/" + service + "/consumers";
}

static std::string GetDomainPath(const std::string& domain) { return "/sylar/" + domain; }

bool ParseDomainService(const std::string& path, std::string& domain, std::string& service) {
	auto v = azzato::split(path, '/');
	if(v.size() != 5) {
		return false;
	}
	domain	= v[2];
	service = v[3];
	return true;
}

bool ZKServiceDiscovery::registerInfo(const std::string& domain,
									  const std::string& service,
									  const std::string& ip_and_port,
									  const std::string& data) {
	std::string path = GetProvidersPath(domain, service);
	bool		v	 = existsOrCreate(path);
	if(!v) {
		AZZATO_LOG_ERROR(g_logger) << "create path=" << path << " fail";
		return false;
	}

	std::string new_val(1024, 0);
	int32_t		rt = _client->create(
		path + "/" + ip_and_port, data, new_val, &ZOO_OPEN_ACL_UNSAFE, ZKClient::FlagsType::EPHEMERAL);
	if(rt == ZOK) {
		return true;
	}
	if(!_isOnTimer) {
		AZZATO_LOG_ERROR(g_logger) << "create path=" << (path + "/" + ip_and_port)
								   << " fail, error:" << zerror(rt) << " (" << rt << ")";
	}
	return rt == ZNODEEXISTS;
}

bool ZKServiceDiscovery::queryInfo(const std::string& domain, const std::string& service) {
	if(service != "all") {
		std::string path = GetConsumersPath(domain, service);
		bool		v	 = existsOrCreate(path);
		if(!v) {
			AZZATO_LOG_ERROR(g_logger) << "create path=" << path << " fail";
			return false;
		}

		if(_selfInfo.empty()) {
			AZZATO_LOG_ERROR(g_logger) << "queryInfo selfInfo is null";
			return false;
		}

		std::string new_val(1024, 0);
		int32_t		rt = _client->create(
			path + "/" + _selfInfo, _selfData, new_val, &ZOO_OPEN_ACL_UNSAFE, ZKClient::FlagsType::EPHEMERAL);
		if(rt == ZOK) {
			return true;
		}
		if(!_isOnTimer) {
			AZZATO_LOG_ERROR(g_logger) << "create path=" << (path + "/" + _selfInfo)
									   << " fail, error:" << zerror(rt) << " (" << rt << ")";
		}
		return rt == ZNODEEXISTS;
	} else {
		std::vector<std::string> children;
		_client->getChildren(GetDomainPath(domain), children, false);
		bool rt = true;
		for(auto& i : children) {
			rt &= queryInfo(domain, i);
		}
		return rt;
	}
}

bool ZKServiceDiscovery::getChildren(const std::string& path) {
	std::string domain;
	std::string service;
	if(!ParseDomainService(path, domain, service)) {
		AZZATO_LOG_ERROR(g_logger) << "get_children path=" << path << " invalid path";
		return false;
	}
	{
		azzato::RWMutex::ReadLock lock(_mutex);
		auto					  it = _queryInfos.find(domain);
		if(it == _queryInfos.end()) {
			AZZATO_LOG_ERROR(g_logger)
				<< "get_children path=" << path << " domian=" << domain << " not exists";
			return false;
		}
		if(it->second.count(service) == 0 && it->second.count("all") == 0) {
			AZZATO_LOG_ERROR(g_logger)
				<< "get_children path=" << path << " service=" << service << " not exists "
				<< azzato::join(it->second.begin(), it->second.end(), ",");
			return false;
		}
	}

	std::vector<std::string> vals;
	int32_t					 v = _client->getChildren(path, vals, true);
	if(v != ZOK) {
		AZZATO_LOG_ERROR(g_logger) << "get_children path=" << path << " fail, error:" << zerror(v) << " ("
								   << v << ")";
		return false;
	}
	std::unordered_map<uint64_t, ServiceItemInfo::ptr> infos;
	for(auto& i : vals) {
		auto info = ServiceItemInfo::Create(i, "");
		if(!info) {
			continue;
		}
		infos[info->getId()] = info;
		AZZATO_LOG_INFO(g_logger) << "domain=" << domain << " service=" << service
								  << " info=" << info->toString();
	}

	auto					   new_vals = infos;
	azzato::RWMutex::WriteLock lock(_mutex);
	_datas[domain][service].swap(infos);
	lock.unlock();

	_cb(domain, service, infos, new_vals);
	return true;
}

bool ZKServiceDiscovery::queryData(const std::string& domain, const std::string& service) {
	// AZZATO_LOG_INFO(g_logger) << "query_data domain=" << domain
	//                          << " service=" << service;
	if(service != "all") {
		std::string path = GetProvidersPath(domain, service);
		return getChildren(path);
	} else {
		std::vector<std::string> children;
		_client->getChildren(GetDomainPath(domain), children, false);
		bool rt = true;
		for(auto& i : children) {
			rt &= queryData(domain, i);
		}
		return rt;
	}
}

void ZKServiceDiscovery::onZKChild(const std::string& path, ZKClient::ptr client) {
	// AZZATO_LOG_INFO(g_logger) << "onZKChild path=" << path;
	getChildren(path);
}

void ZKServiceDiscovery::onZKChanged(const std::string& path, ZKClient::ptr client) {
	AZZATO_LOG_INFO(g_logger) << "onZKChanged path=" << path;
}

void ZKServiceDiscovery::onZKDeleted(const std::string& path, ZKClient::ptr client) {
	AZZATO_LOG_INFO(g_logger) << "onZKDeleted path=" << path;
}

void ZKServiceDiscovery::onZKExpiredSession(const std::string& path, ZKClient::ptr client) {
	AZZATO_LOG_INFO(g_logger) << "onZKExpiredSession path=" << path;
	client->reconnect();
}

void ZKServiceDiscovery::onWatch(int type, int stat, const std::string& path, ZKClient::ptr client) {
	if(stat == ZKClient::StateType::CONNECTED) {
		if(type == ZKClient::EventType::SESSION) {
			return onZKConnect(path, client);
		} else if(type == ZKClient::EventType::CHILD) {
			return onZKChild(path, client);
		} else if(type == ZKClient::EventType::CHANGED) {
			return onZKChanged(path, client);
		} else if(type == ZKClient::EventType::DELETED) {
			return onZKDeleted(path, client);
		}
	} else if(stat == ZKClient::StateType::EXPIRED_SESSION) {
		if(type == ZKClient::EventType::SESSION) {
			return onZKExpiredSession(path, client);
		}
	}
	AZZATO_LOG_ERROR(g_logger) << "onWatch hosts=" << _hosts << " type=" << type << " stat=" << stat
							   << " path=" << path << " client=" << client;
}

}  // namespace azzato
