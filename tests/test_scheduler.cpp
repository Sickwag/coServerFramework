#include "scheduler.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <iostream>
#include <unistd.h>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

static int s_count = 5;

void testFiber() {
	AZZATO_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;
	if(--s_count >= 0) {
		azzato::Scheduler::getThis()->schedule(&testFiber);
	}
}

void testScheduler() {
	azzato::Scheduler sc(3, false, "test");
	sc.start();
	sc.schedule(&testFiber);
	sc.stop();
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_scheduler begin";
	testScheduler();
	AZZATO_LOG_INFO(g_logger) << "test_scheduler over";
	return 0;
}
