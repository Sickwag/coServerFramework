#include "db/fox_thread.h"
#include "log.h"
#include "utils/macro.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testFoxThread() {
	azzato::FoxThread thread("test_fox");
	thread.start();

	std::atomic<int> counter{0};
	const int		 n = 100;
	for(int i = 0; i < n; ++i) {
		thread.dispatch([&]() { counter.fetch_add(1); });
	}
	thread.stop();
	thread.join();

	AZZATO_LOG_INFO(g_logger) << "fox_thread executed " << counter.load() << "/" << n << " tasks";
	assert(counter.load() == n);
}

void testFoxThreadPool() {
	azzato::FoxThreadPool pool(4, "pool", true);
	pool.start();

	std::atomic<int>						  counter{0};
	const int								  n = 200;
	std::vector<azzato::IFoxThread::callback> cbs;
	for(int i = 0; i < n; ++i) {
		cbs.push_back([&]() { counter.fetch_add(1); });
	}
	pool.batchDispatch(cbs);
	// give the pool a moment to drain
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	pool.stop();
	pool.join();

	AZZATO_LOG_INFO(g_logger) << "fox_thread_pool executed " << counter.load() << "/" << n << " tasks";
	assert(counter.load() == n);
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_fox_thread begin";
	testFoxThread();
	testFoxThreadPool();
	AZZATO_LOG_INFO(g_logger) << "test_fox_thread over";
	return 0;
}
