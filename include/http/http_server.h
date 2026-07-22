#pragma once

#include "http/http_session.h"
#include "http/servlet.h"
#include "tcp_server.h"

namespace azzato {
namespace http {

class HttpServer : public TcpServer {
  public:
	using ptr = std::shared_ptr<HttpServer>;

	HttpServer(bool		  keepalive	   = false,
			   IOManager* worker	   = IOManager::getThis(),
			   IOManager* ioWorker	   = IOManager::getThis(),
			   IOManager* acceptWorker = IOManager::getThis());

	ServletDispatch::ptr getServletDispatch() const { return _dispatch; }

	void setServletDispatch(ServletDispatch::ptr value) { _dispatch = std::move(value); }

	void setName(const std::string& v) override;

  protected:
	void handleClient(Socket::ptr client) override;

  private:
	bool				 _isKeepalive;
	ServletDispatch::ptr _dispatch;
};

}  // namespace http
}  // namespace azzato
