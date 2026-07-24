#include "log.h"
#include "protocol.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testRequest() {
	azzato::Request::ptr req(new azzato::Request);
	req->setSn(42);
	req->setCmd(1001);

	auto ba = req->toByteArray();
	assert(ba);

	azzato::Request::ptr parsed(new azzato::Request);
	assert(parsed->parseFromByteArray(ba));
	assert(parsed->getSn() == 42);
	assert(parsed->getCmd() == 1001);
	assert(parsed->getType() == azzato::Message::Request);
	AZZATO_LOG_INFO(g_logger) << "Request round trip ok";
}

void testResponse() {
	azzato::Response::ptr rsp(new azzato::Response);
	rsp->setSn(7);
	rsp->setCmd(200);
	rsp->setResult(0);
	rsp->setResultStr("ok");

	auto ba = rsp->toByteArray();
	assert(ba);

	azzato::Response::ptr parsed(new azzato::Response);
	assert(parsed->parseFromByteArray(ba));
	assert(parsed->getSn() == 7);
	assert(parsed->getCmd() == 200);
	assert(parsed->getResult() == 0);
	assert(parsed->getResultStr() == "ok");
	AZZATO_LOG_INFO(g_logger) << "Response round trip ok";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_protocol begin";
	testRequest();
	testResponse();
	AZZATO_LOG_INFO(g_logger) << "test_protocol over";
	return 0;
}
