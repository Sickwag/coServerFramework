#include "application.h"
#include "env.h"
#include "http/ws_server.h"
#include "utils/config.h"
#include "utils/macro.h"

#include <signal.h>
#include <unistd.h>

namespace azzato {

namespace {
ConfigVar<std::string>::ptr g_serverWorkPath =
	Config::lookup("server.work_path", std::string("/apps/work/azzato"), "server work path");

ConfigVar<std::vector<TcpServerConf>>::ptr g_serversConf =
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

	if(isPrintHelp) {
		EnvMgr::getInstance()->printHelp();
		return false;
	}

	if(!EnvMgr::getInstance()->has("s") && !EnvMgr::getInstance()->has("d")) {
		EnvMgr::getInstance()->printHelp();
		return false;
	}
	return true;
}

bool Application::run() {
	_mainIOManager.reset(new IOManager(2, true, "main"));
	_mainIOManager->schedule([this]() { runFiber(); });
	_mainIOManager->stop();
	return true;
}

int Application::runFiber() {
	std::vector<TcpServerConf> confs = g_serversConf->getValue();
	for(auto& conf : confs) {
		if(!conf.isValid()) {
			continue;
		}
		if(conf.type == "http") {
			http::HttpServer::ptr server(new http::HttpServer(
				conf.keepalive == 1, _mainIOManager.get(), _mainIOManager.get(), _mainIOManager.get()));
			server->setName(conf.name.empty() ? "azzato_http" : conf.name);
			std::vector<Address::ptr> addrs, fails;
			for(auto& addrStr : conf.address) {
				auto addr = Address::lookupAny(addrStr);
				if(addr) {
					addrs.push_back(addr);
				}
			}
			if(server->bind(addrs, fails, conf.ssl == 1)) {
				registerServer("http", server);
				server->start();
				AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "http server started";
			}
		} else if(conf.type == "websocket" || conf.type == "ws") {
			http::WSServer::ptr server(
				new http::WSServer(_mainIOManager.get(), _mainIOManager.get(), _mainIOManager.get()));
			server->setName(conf.name.empty() ? "azzato_ws" : conf.name);
			std::vector<Address::ptr> addrs, fails;
			for(auto& addrStr : conf.address) {
				auto addr = Address::lookupAny(addrStr);
				if(addr) {
					addrs.push_back(addr);
				}
			}
			if(server->bind(addrs, fails, conf.ssl == 1)) {
				registerServer("websocket", server);
				server->start();
				AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "websocket server started";
			}
		}
	}

	AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "application running, press Ctrl-C to stop";
	while(true) {
		::sleep(1);
		if(EnvMgr::getInstance()->has("d")) {
			// daemon mode: keep running
		}
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
