#include "log.h"
#include "thread.h"
#include "utils/macro.h"

#include <atomic>
#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");
static std::atomic<int>	   g_count{0};

void worker() { ++g_count; }

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_thread begin";
	{
		azzato::Thread t(worker, "worker");
		t.join();
	}
	assert(g_count == 1);
	assert(azzato::Thread::getCurrentThreadId() != 0);
	AZZATO_LOG_INFO(g_logger) << "thread id=" << azzato::Thread::getCurrentThreadId();
	AZZATO_LOG_INFO(g_logger) << "test_thread over";
	return 0;
}
