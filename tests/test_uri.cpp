#include "log.h"
#include "uri.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testUri() {
	auto uri = azzato::Uri::create("http://user@example.com:8080/over/there?name=ferret#nose");
	assert(uri);
	assert(uri->getScheme() == "http");
	assert(uri->getUserinfo() == "user");
	assert(uri->getHost() == "example.com");
	assert(uri->getPort() == 8080);
	assert(uri->getPath() == "/over/there");
	assert(uri->getQuery() == "name=ferret");
	assert(uri->getFragment() == "nose");
	AZZATO_LOG_INFO(g_logger) << uri->toString();
}

void testDefaultPort() {
	auto uri = azzato::Uri::create("https://example.com/path");
	assert(uri);
	assert(uri->getPort() == 443);	// default https port
	AZZATO_LOG_INFO(g_logger) << uri->toString();
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_uri begin";
	testUri();
	testDefaultPort();
	AZZATO_LOG_INFO(g_logger) << "test_uri over";
	return 0;
}
