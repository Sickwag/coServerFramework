#include "hook.h"
#include "iomanager.h"
#include "log.h"
#include "utils/macro.h"
#include "utils/util.h"

#include <cassert>
#include <unistd.h>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

// Hooked sleep inside fibers should let the three sleeps overlap: the whole
// batch completes in ~2s instead of ~6s, proving the blocking call yielded.
void testHookedSleep() {
	azzato::IOManager iom(2, false, "iomanager");
	int				  done = 0;

	auto tick = [&done]() { ++done; };

	iom.schedule([tick]() {
		sleep(2);
		tick();
		AZZATO_LOG_INFO(g_logger) << "sleep 2s done";
	});
	iom.schedule([tick]() {
		sleep(1);
		tick();
		AZZATO_LOG_INFO(g_logger) << "sleep 1s done";
	});
	iom.schedule([tick]() {
		sleep(1);
		tick();
		AZZATO_LOG_INFO(g_logger) << "sleep 1s (2) done";
	});

	auto start = azzato::getCurrentMS();
	iom.stop();
	auto elapsed = azzato::getCurrentMS() - start;

	assert(done == 3);
	assert(elapsed < 3000);
	AZZATO_LOG_INFO(g_logger) << "hooked sleep elapsed=" << elapsed << "ms, done=" << done;
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_hook begin";
	testHookedSleep();
	AZZATO_LOG_INFO(g_logger) << "test_hook over";
	return 0;
}
