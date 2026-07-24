#include "log.h"
#include "rock/rock_protocol.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testRockRequest() {
	azzato::RockRequest::ptr req(new azzato::RockRequest);
	req->setSn(1);
	req->setCmd(100);
	req->setBody("rock body");

	auto ba = req->toByteArray();
	assert(ba);

	azzato::RockRequest::ptr parsed(new azzato::RockRequest);
	assert(parsed->parseFromByteArray(ba));
	assert(parsed->getSn() == 1);
	assert(parsed->getCmd() == 100);
	assert(parsed->getBody() == "rock body");
	AZZATO_LOG_INFO(g_logger) << "rock request ok";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_rock begin";
	testRockRequest();
	AZZATO_LOG_INFO(g_logger) << "test_rock over";
	return 0;
}
