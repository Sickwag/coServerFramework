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
	auto		   header = session->handleShake();
	if(!header) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "handleShake failed";
		return;
	}
	WSServlet::ptr servlet = _dispatch->getMatchedWSServlet(header->getPath());
	if(!servlet) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "no matched WSServlet";
		session->close();
		return;
	}
	if(servlet->onConnect(header, session)) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "onConnect return fail";
		session->close();
		return;
	}
	while(true) {
		auto msg = session->recvMessage();
		if(!msg) {
			AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "recvMessage failed";
			break;
		}
		servlet->handle(header, msg, session);
	}
	servlet->onClose(header, session);
	session->close();
}

}  // namespace http
}  // namespace azzato
