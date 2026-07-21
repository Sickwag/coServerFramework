#pragma once

#include "http/ws_servlet.h"
#include "http/ws_session.h"
#include "tcp_server.h"

namespace azzato {
namespace http {

class WSServer : public TcpServer {
  public:
	using ptr = std::shared_ptr<WSServer>;

	WSServer(IOManager* worker		  = IOManager::getThis(),
			 IOManager* ioWorker	  = IOManager::getThis(),
			 IOManager* acceptWorker = IOManager::getThis());

	WSServletDispatch::ptr getWSServletDispatch() const { return _dispatch; }

	void setWSServletDispatch(WSServletDispatch::ptr value) { _dispatch = std::move(value); }

  protected:
	void handleClient(Socket::ptr client) override;

  protected:
	WSServletDispatch::ptr _dispatch;
};

}  // namespace http
}  // namespace azzato
