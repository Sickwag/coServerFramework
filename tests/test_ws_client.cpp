#include "http/ws_connection.h"
#include "http/ws_server.h"
#include "iomanager.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testWsClient() {
	azzato::IOManager iom(2, true, "iomanager");

	azzato::http::WSServer::ptr server(new azzato::http::WSServer(&iom, &iom, &iom));
	server->setName("azzato_ws_server");
	int connectCount = 0;
	int closeCount	 = 0;
	server->getWSServletDispatch()->addServlet(
		"/sylar",
		[](azzato::http::HttpRequest::ptr,
		   azzato::http::WSFrameMessage::ptr msg,
		   azzato::http::WSSession::ptr		 session) -> int32_t {
			session->sendMessage(msg);
			return 0;
		},
		[&](azzato::http::HttpRequest::ptr, azzato::http::WSSession::ptr) -> int32_t {
			++connectCount;
			return 0;
		},
		[&](azzato::http::HttpRequest::ptr, azzato::http::WSSession::ptr) -> int32_t {
			++closeCount;
			return 0;
		});

	bool ok = false;
	iom.schedule([&]() {
		auto addr = azzato::IPv4Address::create("127.0.0.1", 0);
		assert(server->bind(addr));
		assert(server->start());

		auto		local = server->getLocalAddress();
		std::string url	  = "ws://" + local->toString() + "/sylar";
		AZZATO_LOG_INFO(g_logger) << "connecting to " << url;

		auto [result, conn] = azzato::http::WSConnection::create(url, 1000);
		if(!conn) {
			AZZATO_LOG_ERROR(g_logger) << "WSConnection::create failed: " << result->error;
			ok = false;
			server->stop();
			return;
		}
		assert(result->result == static_cast<int>(azzato::http::HttpResult::Error::Ok));
		assert(conn->isConnected());
		AZZATO_LOG_INFO(g_logger) << "ws handshake ok";

		int32_t rt = conn->sendMessage("hello");
		assert(rt > 0);
		auto msg = conn->recvMessage();
		assert(msg);
		AZZATO_LOG_INFO(g_logger) << "echo: " << msg->getData();
		assert(msg->getData() == "hello");

		conn->close();
		server->stop();
		ok = true;
	});

	iom.stop();
	AZZATO_LOG_INFO(g_logger) << "connectCount=" << connectCount << " closeCount=" << closeCount;
	assert(connectCount == 1);
	assert(ok);
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_ws_client begin";
	testWsClient();
	AZZATO_LOG_INFO(g_logger) << "test_ws_client over";
	return 0;
}
