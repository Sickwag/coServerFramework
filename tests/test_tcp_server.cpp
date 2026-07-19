#include "tcp_server.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

class EchoServer : public azzato::TcpServer {
  public:
	using ptr = std::shared_ptr<EchoServer>;

	explicit EchoServer(azzato::IOManager* worker)
		: azzato::TcpServer(worker, worker, worker) {}

	azzato::Address::ptr getBoundAddress() { return _socks.empty() ? nullptr : _socks[0]->getLocalAddress(); }

	void handleClient(azzato::Socket::ptr client) override {
		char buf[1024];
		int	 len = client->recv(buf, sizeof(buf));
		if(len > 0) {
			client->send(buf, len);
		}
		client->close();
	}
};

// The whole flow runs inside an IOManager fiber so the hook layer registers an
// FdCtx for every socket (making accept/send/recv fiber-friendly).
void testEchoServer() {
	azzato::IOManager iom(2, true, "iomanager");
	EchoServer::ptr	 server(new EchoServer(&iom));
	bool			 ok = false;

	iom.schedule([&]() {
		auto addr = azzato::IPv4Address::create("127.0.0.1", 0);
		assert(server->bind(addr));
		server->setName("echo_server");
		server->start();

		auto local = server->getBoundAddress();
		AZZATO_LOG_INFO(g_logger) << "echo server on " << local->toString();

		auto client = azzato::Socket::createTcpSocket();
		assert(client->connect(local));

		const char* msg = "echo me";
		assert(client->send(msg, std::strlen(msg)) == static_cast<int>(std::strlen(msg)));

		char buf[64] = {0};
		int	 got	 = client->recv(buf, sizeof(buf));
		ok			 = (got == static_cast<int>(std::strlen(msg)) && std::strcmp(buf, msg) == 0);
		AZZATO_LOG_INFO(g_logger) << "echo received: " << buf;

		client->close();
		server->stop();
	});

	iom.stop();
	assert(ok);
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_tcp_server begin";
	testEchoServer();
	AZZATO_LOG_INFO(g_logger) << "test_tcp_server over";
	return 0;
}
