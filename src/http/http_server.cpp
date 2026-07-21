#include "http/http_server.h"
#include "utils/macro.h"

#include <cstring>

namespace azzato {
namespace http {

HttpServer::HttpServer(bool keepalive, IOManager* worker, IOManager* ioWorker, IOManager* acceptWorker)
	: TcpServer(worker, ioWorker, acceptWorker)
	, _isKeepalive(keepalive) {
	_dispatch.reset(new ServletDispatch);
	_type = "http";
}

void HttpServer::setName(const std::string& v) {
	TcpServer::setName(v);
	_dispatch->setDefault(std::make_shared<NotFoundServlet>(v));
}

void HttpServer::handleClient(Socket::ptr client) {
	AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "handleClient " << *client;
	HttpSession::ptr session(new HttpSession(client));
	do {
		auto req = session->recvRequest();
		if(!req) {
			AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT())
				<< "recv http request fail, errno=" << errno << " errstr=" << std::strerror(errno)
				<< " client:" << *client << " keep_alive=" << _isKeepalive;
			break;
		}

		HttpResponse::ptr rsp(new HttpResponse(req->getVersion(), req->isClose() || !_isKeepalive));
		rsp->setHeader("Server", getName());
		_dispatch->handle(req, rsp, session);
		session->sendResponse(rsp);

		if(!_isKeepalive || req->isClose()) {
			break;
		}
	} while(true);
	session->close();
}

}  // namespace http
}  // namespace azzato
