#pragma once

#include "http/http_server.h"
#include "iomanager.h"
#include "streams/service_discovery.h"
#include "tcp_server.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace azzato {

/**
 * @brief Server application entry point: parses args, loads YAML config,
 *        starts the configured TCP/HTTP/WebSocket servers and runs the
 *        main event loop until interrupted.
 */
class Application {
  public:
	Application();

	static Application* getInstance() { return s_instance; }

	bool init(int argc, char** argv);

	bool run();

	bool getServer(const std::string& type, std::vector<TcpServer::ptr>& servers);

	IServiceDiscovery::ptr getServiceDiscovery() const { return nullptr; }

	void listAllServer(std::map<std::string, std::vector<TcpServer::ptr>>& servers);

  private:
	int runFiber();

	void registerServer(const std::string& type, TcpServer::ptr server);

  private:
	int												   _argc = 0;
	char**											   _argv = nullptr;
	std::map<std::string, std::vector<TcpServer::ptr>> _servers;
	IOManager::ptr									   _mainIOManager;
	static Application*								   s_instance;
};

}  // namespace azzato
