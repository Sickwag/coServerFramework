#include "http/http.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testRequest() {
	azzato::http::HttpRequest::ptr req(new azzato::http::HttpRequest);
	req->setMethod(azzato::http::HttpMethod::Get);
	req->setPath("/index.html");
	req->setQuery("name=azzato");
	req->setHeader("Host", "localhost:8080");
	req->setBody("hello");

	std::string text = req->toString();
	AZZATO_LOG_INFO(g_logger) << "--- request ---\n" << text;

	assert(text.find("GET /index.html?name=azzato HTTP/1.1") != std::string::npos);
	assert(text.find("Host: localhost:8080") != std::string::npos);
	assert(text.find("content-length: 5") != std::string::npos);
	assert(text.find("hello") != std::string::npos);
}

void testResponse() {
	azzato::http::HttpResponse::ptr rsp(new azzato::http::HttpResponse);
	rsp->setStatus(azzato::http::HttpStatus::Ok);
	rsp->setBody("hello");
	rsp->setHeader("Content-Type", "text/plain");

	std::string text = rsp->toString();
	AZZATO_LOG_INFO(g_logger) << "--- response ---\n" << text;

	assert(text.find("HTTP/1.1 200 OK") != std::string::npos);
	assert(text.find("Content-Type: text/plain") != std::string::npos);
}

void testCookie() {
	azzato::http::HttpResponse::ptr rsp(new azzato::http::HttpResponse);
	rsp->setCookie("session", "abc123", 0, "/", "example.com", false);
	std::string text = rsp->toString();
	AZZATO_LOG_INFO(g_logger) << "--- cookie ---\n" << text;
	assert(text.find("Set-Cookie: session=abc123") != std::string::npos);
	assert(text.find("domain=example.com") != std::string::npos);
	assert(text.find("path=/") != std::string::npos);
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_http begin";
	testRequest();
	testResponse();
	testCookie();
	AZZATO_LOG_INFO(g_logger) << "test_http over";
	return 0;
}
