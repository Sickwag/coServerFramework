#include "http/http_parser.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testRequest() {
	const char* raw = "POST /submit?name=azzato HTTP/1.1\r\n"
					  "Host: localhost:8080\r\n"
					  "Content-Type: application/x-www-form-urlencoded\r\n"
					  "Content-Length: 11\r\n"
					  "\r\n"
					  "hello=world";

	azzato::http::HttpRequestParser::ptr parser(new azzato::http::HttpRequestParser);
	size_t								 consumed = parser->execute(raw, std::strlen(raw));
	assert(consumed == std::strlen(raw));
	assert(parser->isFinished());
	assert(!parser->hasError());

	auto req = parser->getData();
	assert(req->getMethod() == azzato::http::HttpMethod::Post);
	assert(req->getPath() == "/submit");
	assert(req->getQuery() == "name=azzato");
	assert(req->getVersion() == 0x11);
	assert(req->getHeader("Host") == "localhost:8080");
	assert(req->getBody() == "hello=world");
	assert(req->getParam("hello") == "world");
	AZZATO_LOG_INFO(g_logger) << "request parsed: " << azzato::http::httpMethodToString(req->getMethod())
							  << " " << req->getPath();
}

void testChunkedInput() {
	// Feed the request in small chunks to exercise the incremental parser.
	const char* raw = "GET /chunked HTTP/1.1\r\n"
					  "Host: example.com\r\n"
					  "\r\n";

	azzato::http::HttpRequestParser::ptr parser(new azzato::http::HttpRequestParser);
	for(size_t i = 0; i < std::strlen(raw); ++i) {
		parser->execute(raw + i, 1);
	}
	assert(parser->isFinished());
	assert(!parser->hasError());
	auto req = parser->getData();
	assert(req->getMethod() == azzato::http::HttpMethod::Get);
	assert(req->getPath() == "/chunked");
	AZZATO_LOG_INFO(g_logger) << "chunked request parsed ok";
}

void testResponse() {
	const char* raw = "HTTP/1.1 200 OK\r\n"
					  "Content-Type: text/plain\r\n"
					  "Content-Length: 5\r\n"
					  "\r\n"
					  "hello";

	azzato::http::HttpResponseParser::ptr parser(new azzato::http::HttpResponseParser);
	parser->execute(raw, std::strlen(raw));
	assert(parser->isFinished());
	assert(!parser->hasError());

	auto rsp = parser->getData();
	assert(rsp->getStatus() == azzato::http::HttpStatus::Ok);
	assert(rsp->getVersion() == 0x11);
	assert(rsp->getBody() == "hello");
	assert(rsp->getHeader("Content-Type") == "text/plain");
	AZZATO_LOG_INFO(g_logger) << "response parsed: " << static_cast<int>(rsp->getStatus());
}

void testInvalid() {
	const char*							 bad = "NOT A VALID REQUEST\r\n";
	azzato::http::HttpRequestParser::ptr parser(new azzato::http::HttpRequestParser);
	parser->execute(bad, std::strlen(bad));
	// A malformed request line leaves an invalid method but should not crash;
	// the line is accepted and headers state is reached.
	AZZATO_LOG_INFO(g_logger) << "invalid input did not crash";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_http_parser begin";
	testRequest();
	testChunkedInput();
	testResponse();
	testInvalid();
	AZZATO_LOG_INFO(g_logger) << "test_http_parser over";
	return 0;
}
