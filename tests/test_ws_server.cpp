#include "http/http_parser.h"
#include "http/ws_server.h"
#include "log.h"
#include "streams/socket_stream.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

// Minimal WS client: perform the HTTP upgrade handshake, then send a masked
// text frame and read back the server's echo.
bool wsClientRoundTrip(azzato::Address::ptr addr) {
	auto socket = azzato::Socket::createTcpSocket();
	if(!socket->connect(addr)) {
		return false;
	}
	azzato::SocketStream::ptr stream(new azzato::SocketStream(socket));

	// HTTP upgrade request
	azzato::http::HttpRequest::ptr req(new azzato::http::HttpRequest);
	req->setMethod(azzato::http::HttpMethod::Get);
	req->setPath("/echo");
	req->setHeader("Upgrade", "websocket");
	req->setHeader("Connection", "Upgrade");
	req->setHeader("Sec-WebSocket-Version", "13");
	req->setHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
	req->setHeader("Host", "localhost");
	req->setWebsocket(true);

	std::string reqText = req->toString();
	if(stream->writeFixSize(reqText.c_str(), reqText.size()) <= 0) {
		return false;
	}

	// Read and parse the 101 response
	azzato::http::HttpResponseParser::ptr rspParser(new azzato::http::HttpResponseParser);
	std::string							  responseBuffer;
	char								  buf[256];
	while(!rspParser->isFinished()) {
		int n = stream->read(buf, sizeof(buf));
		if(n <= 0) {
			return false;
		}
		responseBuffer.append(buf, static_cast<size_t>(n));
		rspParser->execute(responseBuffer.data(), responseBuffer.size());
		if(rspParser->hasError()) {
			return false;
		}
	}
	if(rspParser->getData()->getStatus() != azzato::http::HttpStatus::SwitchingProtocols) {
		return false;
	}
	AZZATO_LOG_INFO(g_logger) << "handshake ok, Sec-WebSocket-Accept="
							  << rspParser->getData()->getHeader("Sec-WebSocket-Accept");

	// Send a masked text frame "hello"
	auto msg = std::make_shared<azzato::http::WSFrameMessage>(azzato::http::WSFrameHead::TextFrame, "hello");
	if(azzato::http::wsSendMessage(stream.get(), msg, true, true) <= 0) {
		return false;
	}

	// Read the echo (server->client frames are unmasked)
	auto echo = azzato::http::wsRecvMessage(stream.get(), true);
	if(!echo) {
		return false;
	}
	AZZATO_LOG_INFO(g_logger) << "echo received: " << echo->getData();
	return echo->getData() == "hello";
}

void testWsServer() {
	azzato::IOManager iom(2, true, "iomanager");

	azzato::http::WSServer::ptr server(new azzato::http::WSServer(&iom, &iom, &iom));
	server->setName("azzato_ws_server");
	server->getWSServletDispatch()->addServlet("/echo",
											   [](azzato::http::HttpRequest::ptr,
												  azzato::http::WSFrameMessage::ptr msg,
												  azzato::http::WSSession::ptr		session) -> int32_t {
												   session->sendMessage(msg);
												   return 0;
											   });

	bool ok = false;
	iom.schedule([&]() {
		auto addr = azzato::IPv4Address::create("127.0.0.1", 0);
		assert(server->bind(addr));
		assert(server->start());

		auto local = server->getLocalAddress();
		AZZATO_LOG_INFO(g_logger) << "ws server on " << local->toString();
		ok = wsClientRoundTrip(local);

		server->stop();
	});

	iom.stop();
	assert(ok);
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_ws_server begin";
	testWsServer();
	AZZATO_LOG_INFO(g_logger) << "test_ws_server over";
	return 0;
}
