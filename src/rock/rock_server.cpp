#include "rock/rock_server.h"
#include "log.h"
#include "module.h"

namespace azzato {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

RockServer::RockServer(const std::string& type,
					   azzato::IOManager* worker,
					   azzato::IOManager* io_worker,
					   azzato::IOManager* accept_worker)
	: TcpServer(worker, io_worker, accept_worker) {
	_type = type;
}

void RockServer::handleClient(Socket::ptr client) {
	AZZATO_LOG_DEBUG(g_logger) << "handleClient " << *client;
	azzato::RockSession::ptr session(new azzato::RockSession(client));
	session->setWorker(_worker);
	ModuleMgr::getInstance()->foreach(Module::ROCK, [session](Module::ptr m) { m->onConnect(session); });
	session->setDisconnectCb([](AsyncSocketStream::ptr stream) {
		ModuleMgr::getInstance()->foreach(Module::ROCK, [stream](Module::ptr m) { m->onDisconnect(stream); });
	});
	session->setRequestHandler([](azzato::RockRequest::ptr	req,
								  azzato::RockResponse::ptr rsp,
								  azzato::RockStream::ptr	conn) -> bool {
		// AZZATO_LOG_INFO(g_logger) << "handleReq " << req->toString()
		//                          << " body=" << req->getBody();
		bool rt = false;
		ModuleMgr::getInstance()->foreach(Module::ROCK, [&rt, req, rsp, conn](Module::ptr m) {
			if(rt) {
				return;
			}
			rt = m->handleRequest(req, rsp, conn);
		});
		return rt;
	});
	session->setNotifyHandler([](azzato::RockNotify::ptr nty, azzato::RockStream::ptr conn) -> bool {
		AZZATO_LOG_INFO(g_logger) << "handleNty " << nty->toString() << " body=" << nty->getBody();
		bool rt = false;
		ModuleMgr::getInstance()->foreach(Module::ROCK, [&rt, nty, conn](Module::ptr m) {
			if(rt) {
				return;
			}
			rt = m->handleNotify(nty, conn);
		});
		return rt;
	});
	session->start();
}

}  // namespace azzato
