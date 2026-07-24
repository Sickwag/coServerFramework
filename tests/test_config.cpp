#include "log.h"
#include "utils/config.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testLookup() {
	auto port = azzato::Config::lookup("test.port", 8080, "test port");
	assert(port->getValue() == 8080);
	port->setValue(9090);
	assert(port->getValue() == 9090);

	bool changed = false;
	port->addListener([&changed](const int&, const int&) { changed = true; });
	port->setValue(10001);
	assert(changed);

	auto again = azzato::Config::lookup("test.port", 1, "again");
	assert(again == port);
	AZZATO_LOG_INFO(g_logger) << "config ok, port=" << port->getValue();
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_config begin";
	testLookup();
	AZZATO_LOG_INFO(g_logger) << "test_config over";
	return 0;
}
