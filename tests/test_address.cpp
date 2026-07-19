#include "address.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testIPv4() {
	auto addr = azzato::IPAddress::create("127.0.0.1", 8080);
	assert(addr);
	AZZATO_LOG_INFO(g_logger) << addr->toString();
	assert(addr->toString() == "127.0.0.1:8080");
	assert(addr->getFamily() == AF_INET);
	assert(addr->getPort() == 8080);
}

void testIPv6() {
	auto addr = azzato::IPv6Address::create("::1", 8080);
	assert(addr);
	AZZATO_LOG_INFO(g_logger) << addr->toString();
	assert(addr->getFamily() == AF_INET6);
	assert(addr->getPort() == 8080);
}

void testLookup() {
	std::vector<azzato::Address::ptr> result;
	bool ok = azzato::Address::lookup(result, "localhost:80", AF_INET, SOCK_STREAM);
	assert(ok);
	assert(!result.empty());
	for(auto& addr : result) {
		AZZATO_LOG_INFO(g_logger) << "localhost: " << addr->toString();
	}

	auto any = azzato::Address::lookupAny("www.baidu.com:80");
	AZZATO_LOG_INFO(g_logger) << "lookupAny: " << (any ? any->toString() : "null");
}

void testInterface() {
	std::multimap<std::string, std::pair<azzato::Address::ptr, uint32_t>> result;
	bool ok = azzato::Address::getInterfaceAddresses(result, AF_INET);
	assert(ok);
	for(auto& item : result) {
		AZZATO_LOG_INFO(g_logger) << "iface=" << item.first << " addr=" << item.second.first->toString()
								  << " prefix=" << item.second.second;
	}
}

void testUnix() {
	azzato::UnixAddress addr("/tmp/azzato_test.sock");
	AZZATO_LOG_INFO(g_logger) << "unix path: " << addr.getPath();
	assert(addr.getPath() == "/tmp/azzato_test.sock");
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_address begin";
	testIPv4();
	testIPv6();
	testLookup();
	testInterface();
	testUnix();
	AZZATO_LOG_INFO(g_logger) << "test_address over";
	return 0;
}
