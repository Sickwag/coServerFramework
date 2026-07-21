#include "http/http_server.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testHttpServer() {
	azzato::IOManager iom(2, true, "iomanager");

	azzato::http::HttpServer::ptr server(new azzato::http::HttpServer(true, &iom, &iom, &iom));
	server->setName("azzato_test_server");

	server->getServletDispatch()->addServlet("/echo", [](azzato::http::HttpRequest::ptr req,
														 azzato::http::HttpResponse::ptr rsp,
														 azzato::http::HttpSession::ptr) -> int32_t {
		rsp->setBody(req->getPath());
		return 0;
	});
	server->getServletDispatch()->addGlobServlet(
		"/api/*", [](azzato::http::HttpRequest::ptr req,
					 azzato::http::HttpResponse::ptr rsp,
					 azzato::http::HttpSession::ptr) -> int32_t {
			rsp->setBody("api:" + req->getPath());
			return 0;
		});

	bool ok = false;
	iom.schedule([&]() {
		auto addr = azzato::IPv4Address::create("127.0.0.1", 0);
		assert(server->bind(addr));
		assert(server->start());

		auto local = server->getLocalAddress();
		AZZATO_LOG_INFO(g_logger) << "http server on " << local->toString();

		auto client = azzato::Socket::createTcpSocket();
		assert(client->connect(local));

		const char* reqText =
			"GET /echo HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Connection: close\r\n"
			"\r\n";
		assert(client->send(reqText, std::strlen(reqText)) == static_cast<int>(std::strlen(reqText)));

		char buf[1024] = {0};
		int	 total	  = 0;
		while(total < static_cast<int>(sizeof(buf) - 1)) {
			int n = client->recv(buf + total, sizeof(buf) - 1 - static_cast<size_t>(total));
			if(n <= 0) {
				break;
			}
			total += n;
			if(std::strstr(buf, "\r\n\r\n")) {
				// headers complete; if a body follows, grab it
				if(total > static_cast<int>(std::strstr(buf, "\r\n\r\n") - buf) + 4) {
					break;
				}
			}
		}
		std::string response(buf, static_cast<size_t>(total));
		AZZATO_LOG_INFO(g_logger) << "--- response ---\n" << response;
		ok = response.find("HTTP/1.1 200 OK") != std::string::npos
			 && response.find("/echo") != std::string::npos;

		client->close();
		server->stop();
	});

	iom.stop();
	assert(ok);
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_http_server begin";
	testHttpServer();
	AZZATO_LOG_INFO(g_logger) << "test_http_server over";
	return 0;
}
