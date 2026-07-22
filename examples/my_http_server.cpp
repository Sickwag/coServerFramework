#include "http/http_server.h"
#include "log.h"
#include "utils/macro.h"

#include <unistd.h>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

// A minimal HTTP server built directly on the framework.
void run() {
	azzato::http::HttpServer::ptr server(new azzato::http::HttpServer(true));
	server->setName("azzato_example_server");
	server->getServletDispatch()->addServlet("/hello",
											 [](azzato::http::HttpRequest::ptr	req,
												azzato::http::HttpResponse::ptr rsp,
												azzato::http::HttpSession::ptr) -> int32_t {
												 rsp->setBody("hello, world");
												 return 0;
											 });
	server->getServletDispatch()->addServlet("/echo",
											 [](azzato::http::HttpRequest::ptr	req,
												azzato::http::HttpResponse::ptr rsp,
												azzato::http::HttpSession::ptr) -> int32_t {
												 rsp->setBody(req->getBody());
												 return 0;
											 });

	auto addr = azzato::IPv4Address::create("0.0.0.0", 8021);
	if(!server->bind(addr)) {
		AZZATO_LOG_ERROR(g_logger) << "bind failed";
		return;
	}
	if(!server->start()) {
		AZZATO_LOG_ERROR(g_logger) << "start failed";
		return;
	}
	AZZATO_LOG_INFO(g_logger) << "example http server on 0.0.0.0:8021";
}

int main() {
	azzato::IOManager iom(2, false, "iomanager");
	iom.schedule(&run);
	while(true) {
		::sleep(1);
	}
	return 0;
}
