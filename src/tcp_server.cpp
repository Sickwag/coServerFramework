#include "tcp_server.h"
#include "utils/config.h"
#include "utils/macro.h"

#include <cstring>

namespace azzato {

namespace {
ConfigVar<uint64_t>::ptr g_tcpServerReadTimeout =
	Config::lookup("tcp_server.read_timeout", static_cast<uint64_t>(60 * 1000 * 2), "tcp server read timeout");
}

TcpServer::TcpServer(IOManager* worker, IOManager* ioWorker, IOManager* acceptWorker)
	: _worker(worker)
	, _ioWorker(ioWorker)
	, _acceptWorker(acceptWorker)
	, _recvTimeout(g_tcpServerReadTimeout->getValue())
	, _name("azzato/1.0.0")
	, _isStop(true) {}

TcpServer::~TcpServer() {
	for(auto& sock : _socks) {
		sock->close();
	}
	_socks.clear();
}

void TcpServer::setConf(const TcpServerConf& conf) { _conf.reset(new TcpServerConf(conf)); }

bool TcpServer::bind(Address::ptr addr, bool ssl) {
	std::vector<Address::ptr> addrs;
	std::vector<Address::ptr> fails;
	addrs.push_back(addr);
	return bind(addrs, fails, ssl);
}

bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails, bool ssl) {
	_ssl = ssl;
	for(auto& addr : addrs) {
		Socket::ptr sock = ssl ? SSLSocket::createTcp(addr) : Socket::createTcp(addr);
		if(!sock->bind(addr)) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "bind fail errno=" << errno << " errstr=" << std::strerror(errno)
												<< " addr=[" << addr->toString() << "]";
			fails.push_back(addr);
			continue;
		}
		if(!sock->listen()) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "listen fail errno=" << errno << " errstr=" << std::strerror(errno)
												<< " addr=[" << addr->toString() << "]";
			fails.push_back(addr);
			continue;
		}
		_socks.push_back(sock);
	}

	if(!fails.empty()) {
		_socks.clear();
		return false;
	}

	for(auto& sock : _socks) {
		AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "type=" << _type << " name=" << _name << " ssl=" << _ssl
										   << " server bind success: " << *sock;
	}
	return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
	while(!_isStop) {
		Socket::ptr client = sock->accept();
		if(client) {
			client->setRecvTimeout(_recvTimeout);
			_ioWorker->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
		} else {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "accept errno=" << errno << " errstr=" << std::strerror(errno);
		}
	}
}

bool TcpServer::start() {
	if(!_isStop) {
		return true;
	}
	_isStop = false;
	for(auto& sock : _socks) {
		_acceptWorker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
	}
	return true;
}

void TcpServer::stop() {
	_isStop  = true;
	auto self = shared_from_this();
	_acceptWorker->schedule([this, self]() {
		for(auto& sock : _socks) {
			sock->cancelAll();
			sock->close();
		}
		_socks.clear();
	});
}

void TcpServer::handleClient(Socket::ptr client) {
	AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "handleClient: " << *client;
}

bool TcpServer::loadCertificates(const std::string& certFile, const std::string& keyFile) {
	for(auto& sock : _socks) {
		auto sslSocket = std::dynamic_pointer_cast<SSLSocket>(sock);
		if(sslSocket) {
			if(!sslSocket->loadCertificates(certFile, keyFile)) {
				return false;
			}
		}
	}
	return true;
}

std::string TcpServer::toString(const std::string& prefix) {
	std::stringstream ss;
	ss << prefix << "[type=" << _type << " name=" << _name << " ssl=" << _ssl
	   << " worker=" << (_worker ? _worker->getName() : "")
	   << " accept=" << (_acceptWorker ? _acceptWorker->getName() : "") << " recv_timeout=" << _recvTimeout << "]"
	   << std::endl;
	std::string pfx = prefix.empty() ? "    " : prefix;
	for(auto& sock : _socks) {
		ss << pfx << *sock << std::endl;
	}
	return ss.str();
}

}  // namespace azzato
