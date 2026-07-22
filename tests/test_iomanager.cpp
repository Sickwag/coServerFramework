#include "iomanager.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <sys/socket.h>
#include <unistd.h>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testTimer() {
	azzato::IOManager iom(1, true, "iomanager");
	int				  fired = 0;
	iom.addTimer(500, [&fired]() { ++fired; }, false);
	iom.addTimer(200, [&fired]() { ++fired; }, false);
	iom.stop();
	assert(fired == 2);
	AZZATO_LOG_INFO(g_logger) << "testTimer fired=" << fired;
}

void testFdEvent() {
	azzato::IOManager iom(1, true, "iomanager");
	int				  sockets[2];
	assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

	int read_hits  = 0;
	int write_hits = 0;
	iom.addEvent(sockets[0], azzato::IOManager::Read, [&read_hits]() { ++read_hits; });
	iom.addEvent(sockets[1], azzato::IOManager::Write, [&sockets, &write_hits]() {
		++write_hits;
		azzato::IOManager::getThis()->delEvent(sockets[1], azzato::IOManager::Write);
	});
	::write(sockets[1], "x", 1);
	::write(sockets[0], "y", 1);

	iom.stop();
	assert(read_hits == 1);
	assert(write_hits >= 1);
	::close(sockets[0]);
	::close(sockets[1]);
	AZZATO_LOG_INFO(g_logger) << "testFdEvent read_hits=" << read_hits << " write_hits=" << write_hits;
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_iomanager begin";
	testTimer();
	testFdEvent();
	AZZATO_LOG_INFO(g_logger) << "test_iomanager over";
	return 0;
}
