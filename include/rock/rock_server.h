#pragma once

#include "rock/rock_stream.h"
#include "tcp_server.h"

namespace azzato {

class RockServer : public TcpServer {
  public:
	typedef std::shared_ptr<RockServer> ptr;
	RockServer(const std::string& type			= "rock",
			   azzato::IOManager* worker		= azzato::IOManager::getThis(),
			   azzato::IOManager* io_worker		= azzato::IOManager::getThis(),
			   azzato::IOManager* accept_worker = azzato::IOManager::getThis());

  protected:
	virtual void handleClient(Socket::ptr client) override;
};

}  // namespace azzato
