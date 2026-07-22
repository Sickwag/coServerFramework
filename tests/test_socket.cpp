#include "log.h"
#include "socket.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

// Blocking echo over a local TCP socket, exercising bind/listen/accept/connect/send/recv.
void testTcpEcho() {
	auto server = azzato::Socket::createTcpSocket();
	auto addr	= azzato::IPv4Address::create("127.0.0.1", 0);
	assert(server->bind(addr));
	assert(server->listen());

	auto local = server->getLocalAddress();
	AZZATO_LOG_INFO(g_logger) << "server listening on " << local->toString();

	auto client = azzato::Socket::createTcpSocket();
	assert(client->connect(local));

	auto conn = server->accept();
	assert(conn);
	AZZATO_LOG_INFO(g_logger) << "accepted: " << conn->toString();

	const char* msg	 = "hello socket";
	ssize_t		sent = client->send(msg, std::strlen(msg));
	assert(sent == static_cast<ssize_t>(std::strlen(msg)));

	char buf[64] = {0};
	int	 got	 = conn->recv(buf, sizeof(buf));
	assert(got == static_cast<int>(std::strlen(msg)));
	assert(std::strcmp(buf, msg) == 0);

	server->close();
	client->close();
	conn->close();
	AZZATO_LOG_INFO(g_logger) << "tcp echo ok: " << buf;
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_socket begin";
	testTcpEcho();
	AZZATO_LOG_INFO(g_logger) << "test_socket over";
	return 0;
}
