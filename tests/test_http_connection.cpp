#include "http/http_connection.h"
#include "http/http_server.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testHttpClient() {
	azzato::IOManager iom(2, true, "iomanager");

	azzato::http::HttpServer::ptr server(new azzato::http::HttpServer(false, &iom, &iom, &iom));
	server->setName("azzato_test_server");
	server->getServletDispatch()->addServlet("/hello",
											 [](azzato::http::HttpRequest::ptr	req,
												azzato::http::HttpResponse::ptr rsp,
												azzato::http::HttpSession::ptr) -> int32_t {
												 rsp->setBody("hello world");
												 return 0;
											 });

	bool ok = false;
	iom.schedule([&]() {
		auto addr = azzato::IPv4Address::create("127.0.0.1", 0);
		assert(server->bind(addr));
		assert(server->start());

		auto local = server->getLocalAddress();
		AZZATO_LOG_INFO(g_logger) << "server on " << local->toString();

		std::string url	   = "http://" + local->toString() + "/hello";
		auto		result = azzato::http::HttpConnection::doGet(url, 3000);
		ok =
			result && result->result == 0 && result->response && result->response->getBody() == "hello world";
		AZZATO_LOG_INFO(g_logger) << "client result: " << (ok ? "ok" : "fail");
		if(!ok && result) {
			AZZATO_LOG_INFO(g_logger) << result->toString();
		}

		server->stop();
	});

	iom.stop();
	assert(ok);
}

void testHttpPool() {
	azzato::IOManager iom(2, true, "iomanager");

	azzato::http::HttpServer::ptr server(new azzato::http::HttpServer(false, &iom, &iom, &iom));
	server->setName("azzato_pool_server");
	server->getServletDispatch()->addServlet("/pool",
											 [](azzato::http::HttpRequest::ptr	req,
												azzato::http::HttpResponse::ptr rsp,
												azzato::http::HttpSession::ptr) -> int32_t {
												 rsp->setBody("pool-ok");
												 return 0;
											 });

	bool ok = false;
	iom.schedule([&]() {
		auto addr = azzato::IPv4Address::create("127.0.0.1", 0);
		assert(server->bind(addr));
		assert(server->start());

		auto local = server->getLocalAddress();
		AZZATO_LOG_INFO(g_logger) << "pool server on " << local->toString();

		std::string base = "http://" + local->toString();
		auto		pool = azzato::http::HttpConnectionPool::create(base, "", 10, 1000, 100);
		assert(pool);

		ok = true;
		for(int i = 0; i < 5; ++i) {
			auto result = pool->doGet("/pool", 3000);
			if(!result || result->result != 0 || !result->response
			   || result->response->getBody() != "pool-ok") {
				ok = false;
				break;
			}
		}
		AZZATO_LOG_INFO(g_logger) << "pool result: " << (ok ? "ok" : "fail");

		server->stop();
	});

	iom.stop();
	assert(ok);
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_http_connection begin";
	testHttpClient();
	testHttpPool();
	AZZATO_LOG_INFO(g_logger) << "test_http_connection over";
	return 0;
}
