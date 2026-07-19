#include "fiber.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <iostream>
#include <string>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void runInFiber() {
	AZZATO_LOG_INFO(g_logger) << "runInFiber begin";
	azzato::Fiber::yieldToHold();
	AZZATO_LOG_INFO(g_logger) << "runInFiber end";
	azzato::Fiber::yieldToHold();
}

void testFiber() {
	AZZATO_LOG_INFO(g_logger) << "main begin -1";
	{
		azzato::Fiber::getThis();
		AZZATO_LOG_INFO(g_logger) << "main begin";
		azzato::Fiber::ptr fiber(new azzato::Fiber(runInFiber, 0, false));
		fiber->call();
		AZZATO_LOG_INFO(g_logger) << "main after swapIn";
		fiber->call();
		AZZATO_LOG_INFO(g_logger) << "main after end";
		fiber->call();
	}
	AZZATO_LOG_INFO(g_logger) << "main after end2";
	assert(azzato::Fiber::getTotalFibers() == 1);
}

int main() {
	testFiber();
	AZZATO_LOG_INFO(g_logger) << "test_fiber over";
	return 0;
}
