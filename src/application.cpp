#include "application.h"

#include <signal.h>
#include <unistd.h>

#include "daemon.h"
#include "db/fox_thread.h"
#include "db/redis.h"
#include "env.h"
#include "http/ws_server.h"
#include "module.h"
#include "ns/name_server_module.h"
#include "rock/rock_server.h"
#include "utils/config.h"
#include "utils/macro.h"
#include "utils/util.h"
#include "worker.h"

namespace azzato {

namespace {
ConfigVar<std::string>::ptr gServerWorkPath =
	Config::lookup("server.work_path", std::string("/apps/work/azzato"), "server work path");

ConfigVar<std::string>::ptr gServerPidFile =
	Config::lookup("server.pid_file", std::string("azzato.pid"), "server pid file");

ConfigVar<std::string>::ptr gServiceDiscoveryZk =
	Config::lookup("service_discovery.zk", std::string(""), "service discovery zookeeper");

ConfigVar<std::vector<TcpServerConf>>::ptr gServersConf =
	Config::lookup("servers", std::vector<TcpServerConf>(), "server config");
}  // namespace

Application* Application::s_instance = nullptr;

Application::Application() { s_instance = this; }

bool Application::init(int argc, char** argv) {
	_argc = argc;
	_argv = argv;

	EnvMgr::getInstance()->addHelp("s", "start with the terminal");
	EnvMgr::getInstance()->addHelp("d", "run as daemon");
	EnvMgr::getInstance()->addHelp("c", "conf path default: ./conf");
	EnvMgr::getInstance()->addHelp("p", "print help");

	bool isPrintHelp = false;
	if(!EnvMgr::getInstance()->init(argc, argv)) {
		isPrintHelp = true;
	}

	if(EnvMgr::getInstance()->has("p")) {
		isPrintHelp = true;
	}

	std::string confPath = EnvMgr::getInstance()->getConfigPath();
	AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "load conf path:" << confPath;
	Config::loadFromConfigDir(confPath);

	ModuleMgr::getInstance()->init();
	std::vector<Module::ptr> modules;
	ModuleMgr::getInstance()->listAll(modules);

	for(auto i : modules) {
		i->onBeforeArgsParse(argc, argv);
	}

	if(isPrintHelp) {
		EnvMgr::getInstance()->printHelp();
		return false;
	}

	for(auto i : modules) {
		i->onAfterArgsParse(argc, argv);
	}
	modules.clear();

	int runType = 0;
	if(EnvMgr::getInstance()->has("s")) {
		runType = 1;
	}
	if(EnvMgr::getInstance()->has("d")) {
		runType = 2;
	}

	if(runType == 0) {
		EnvMgr::getInstance()->printHelp();
		return false;
	}

	std::string pidfile = gServerWorkPath->getValue() + "/" + gServerPidFile->getValue();
	if(FSUtil::isRunningPidfile(pidfile)) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "server is running:" << pidfile;
		return false;
	}

	if(!FSUtil::mkdir(gServerWorkPath->getValue())) {
		AZZATO_LOG_FATAL(AZZATO_LOG_ROOT()) << "create work path [" << gServerWorkPath->getValue()
											<< " errno=" << errno << " errstr=" << strerror(errno);
		return false;
	}
	return true;
}

bool Application::run() {
	bool isDaemon = EnvMgr::getInstance()->has("d");
	return start_daemon(_argc,
						_argv,
						std::bind(&Application::main, this, std::placeholders::_1, std::placeholders::_2),
						isDaemon);
}

int Application::main(int argc, char** argv) {
	signal(SIGPIPE, SIG_IGN);
	AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "main";
	std::string confPath = EnvMgr::getInstance()->getConfigPath();
	Config::loadFromConfigDir(confPath, true);
	{
		std::string	  pidfile = gServerWorkPath->getValue() + "/" + gServerPidFile->getValue();
		std::ofstream ofs(pidfile);
		if(!ofs) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "open pidfile " << pidfile << " failed";
			return false;
		}
		ofs << getpid();
	}

	_mainIOManager.reset(new IOManager(1, true, "main"));
	_mainIOManager->schedule(std::bind(&Application::runFiber, this));
	_mainIOManager->stop();
	return 0;
}

int Application::runFiber() {
	std::vector<Module::ptr> modules;
	ModuleMgr::getInstance()->listAll(modules);
	bool hasError = false;
	for(auto& i : modules) {
		if(!i->onLoad()) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
				<< "module name=" << i->getName() << " version=" << i->getVersion()
				<< " filename=" << i->getFilename();
			hasError = true;
		}
	}
	if(hasError) {
		_exit(0);
	}

	WorkerMgr::getInstance()->init();
	FoxThreadMgr::getInstance()->init();
	FoxThreadMgr::getInstance()->start();
	RedisMgr::getInstance();

	auto						httpConfs = gServersConf->getValue();
	std::vector<TcpServer::ptr> svrs;
	for(auto& i : httpConfs) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << std::endl << LexicalCast<TcpServerConf, std::string>()(i);

		std::vector<Address::ptr> address;
		for(auto& a : i.address) {
			size_t pos = a.find(":");
			if(pos == std::string::npos) {
				address.push_back(std::make_shared<UnixAddress>(a));
				continue;
			}
			int32_t port = atoi(a.substr(pos + 1).c_str());
			auto	addr = IPAddress::create(a.substr(0, pos).c_str(), static_cast<uint16_t>(port));
			if(addr) {
				address.push_back(addr);
				continue;
			}
			std::vector<std::pair<Address::ptr, uint32_t>> result;
			if(Address::getInterfaceAddresses(result, a.substr(0, pos))) {
				for(auto& x : result) {
					auto ipaddr = std::dynamic_pointer_cast<IPAddress>(x.first);
					if(ipaddr) {
						ipaddr->setPort(static_cast<uint16_t>(atoi(a.substr(pos + 1).c_str())));
					}
					address.push_back(ipaddr);
				}
				continue;
			}

			auto aaddr = Address::lookupAny(a);
			if(aaddr) {
				address.push_back(aaddr);
				continue;
			}
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "invalid address: " << a;
			_exit(0);
		}
		IOManager* acceptWorker	 = IOManager::getThis();
		IOManager* ioWorker		 = IOManager::getThis();
		IOManager* processWorker = IOManager::getThis();
		if(!i.acceptWorker.empty()) {
			acceptWorker = WorkerMgr::getInstance()->getAsIOManager(i.acceptWorker).get();
			if(!acceptWorker) {
				AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "accept_worker: " << i.acceptWorker << " not exists";
				_exit(0);
			}
		}
		if(!i.ioWorker.empty()) {
			ioWorker = WorkerMgr::getInstance()->getAsIOManager(i.ioWorker).get();
			if(!ioWorker) {
				AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "io_worker: " << i.ioWorker << " not exists";
				_exit(0);
			}
		}
		if(!i.processWorker.empty()) {
			processWorker = WorkerMgr::getInstance()->getAsIOManager(i.processWorker).get();
			if(!processWorker) {
				AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "process_worker: " << i.processWorker << " not exists";
				_exit(0);
			}
		}

		TcpServer::ptr server;
		if(i.type == "http") {
			server.reset(new http::HttpServer(i.keepalive == 1, processWorker, ioWorker, acceptWorker));
		} else if(i.type == "ws") {
			server.reset(new http::WSServer(processWorker, ioWorker, acceptWorker));
		} else if(i.type == "rock") {
			server.reset(new RockServer("rock", processWorker, ioWorker, acceptWorker));
		} else if(i.type == "nameserver") {
			server.reset(new RockServer("nameserver", processWorker, ioWorker, acceptWorker));
			ModuleMgr::getInstance()->add(std::make_shared<ns::NameServerModule>());
		} else {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
				<< "invalid server type=" << i.type << LexicalCast<TcpServerConf, std::string>()(i);
			_exit(0);
		}
		if(!i.name.empty()) {
			server->setName(i.name);
		}
		std::vector<Address::ptr> fails;
		if(!server->bind(address, fails, i.ssl == 1)) {
			for(auto& x : fails) {
				AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "bind address fail:" << *x;
			}
			_exit(0);
		}
		if(i.ssl == 1) {
			if(!server->loadCertificates(i.certFile, i.keyFile)) {
				AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
					<< "loadCertificates fail, cert_file=" << i.certFile << " key_file=" << i.keyFile;
			}
		}
		server->setConf(i);
		_servers[i.type].push_back(server);
		svrs.push_back(server);
	}

	if(!gServiceDiscoveryZk->getValue().empty()) {
		ZKServiceDiscovery::ptr zksd(new ZKServiceDiscovery(gServiceDiscoveryZk->getValue()));
		_serviceDiscovery = zksd;
		_rockSDLoadBalance.reset(new RockSDLoadBalance(_serviceDiscovery));

		std::vector<TcpServer::ptr> httpServers;
		if(!getServer("http", httpServers)) {
			zksd->setSelfInfo(getIPv4() + ":0:" + getHostName());
		} else {
			std::string ipAndPort;
			for(auto& i : httpServers) {
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
					if(str.find("0.0.0.0") == 0) {
						ipAndPort = getIPv4() + ":" + std::to_string(addr->getPort());
						break;
					} else {
						ipAndPort = addr->toString();
					}
				}
				if(!ipAndPort.empty()) {
					break;
				}
			}
			zksd->setSelfInfo(ipAndPort + ":" + getHostName());
		}
	}

	for(auto& i : modules) {
		i->onServerReady();
	}

	for(auto& i : svrs) {
		i->start();
	}

	if(_rockSDLoadBalance) {
		_rockSDLoadBalance->start();
	}

	for(auto& i : modules) {
		i->onServerUp();
	}
	return 0;
}

bool Application::getServer(const std::string& type, std::vector<TcpServer::ptr>& servers) {
	auto it = _servers.find(type);
	if(it == _servers.end()) {
		return false;
	}
	servers = it->second;
	return true;
}

void Application::listAllServer(std::map<std::string, std::vector<TcpServer::ptr>>& servers) {
	servers = _servers;
}

void Application::registerServer(const std::string& type, TcpServer::ptr server) {
	_servers[type].push_back(std::move(server));
}

}  // namespace azzato
