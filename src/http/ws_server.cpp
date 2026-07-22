#include "http/ws_server.h"
#include "utils/macro.h"

#include <cstring>

namespace azzato {
namespace http {

WSServer::WSServer(IOManager* worker, IOManager* ioWorker, IOManager* acceptWorker)
	: TcpServer(worker, ioWorker, acceptWorker) {
	_type = "websocket";
	_dispatch.reset(new WSServletDispatch);
}

void WSServer::handleClient(Socket::ptr client) {
	AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "handleClient " << *client;
	WSSession::ptr session(new WSSession(client));
	auto		   req = session->handleShake();
	if(!req) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "handleShake failed";
		return;
	}
	while(true) {
		auto msg = session->recvMessage();
		if(!msg) {
			AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "recvMessage failed";
			break;
		}
		_dispatch->handle(req, msg, session);
	}
	session->close();
}

}  // namespace http
}  // namespace azzato
