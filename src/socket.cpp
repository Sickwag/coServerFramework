#include "socket.h"
#include "fd_manager.h"
#include "hook.h"
#include "iomanager.h"
#include "utils/macro.h"
#include "utils/util.h"

#include <cstring>
#include <limits.h>

namespace azzato {

Socket::ptr Socket::createTcp(Address::ptr address) {
	Socket::ptr sock(new Socket(address->getFamily(), Tcp, 0));
	return sock;
}

Socket::ptr Socket::createUdp(Address::ptr address) {
	Socket::ptr sock(new Socket(address->getFamily(), Udp, 0));
	sock->newSock();
	sock->_isConnected = true;
	return sock;
}

Socket::ptr Socket::createTcpSocket() {
	Socket::ptr sock(new Socket(Ipv4, Tcp, 0));
	return sock;
}

Socket::ptr Socket::createUdpSocket() {
	Socket::ptr sock(new Socket(Ipv4, Udp, 0));
	sock->newSock();
	sock->_isConnected = true;
	return sock;
}

Socket::ptr Socket::createTcpSocket6() {
	Socket::ptr sock(new Socket(Ipv6, Tcp, 0));
	return sock;
}

Socket::ptr Socket::createUdpSocket6() {
	Socket::ptr sock(new Socket(Ipv6, Udp, 0));
	sock->newSock();
	sock->_isConnected = true;
	return sock;
}

Socket::ptr Socket::createUnixTcpSocket() {
	Socket::ptr sock(new Socket(Unix, Tcp, 0));
	return sock;
}

Socket::ptr Socket::createUnixUdpSocket() {
	Socket::ptr sock(new Socket(Unix, Udp, 0));
	return sock;
}

Socket::Socket(int family, int type, int protocol)
	: _sock(-1)
	, _family(family)
	, _type(type)
	, _protocol(protocol)
	, _isConnected(false) {}

Socket::~Socket() { close(); }

int64_t Socket::getSendTimeout() {
	FdCtx::ptr ctx = FdMgr::getInstance()->get(_sock);
	if(ctx) {
		return static_cast<int64_t>(ctx->getTimeout(SO_SNDTIMEO));
	}
	return -1;
}

void Socket::setSendTimeout(int64_t value) {
	struct timeval tv {
		static_cast<int>(value / 1000), static_cast<int>(value % 1000 * 1000)
	};

	setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
	FdCtx::ptr ctx = FdMgr::getInstance()->get(_sock);
	if(ctx) {
		return static_cast<int64_t>(ctx->getTimeout(SO_RCVTIMEO));
	}
	return -1;
}

void Socket::setRecvTimeout(int64_t value) {
	struct timeval tv {
		static_cast<int>(value / 1000), static_cast<int>(value % 1000 * 1000)
	};

	setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, socklen_t* len) {
	int rt = getsockopt(_sock, level, option, result, len);
	if(rt) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT())
			<< "getOption sock=" << _sock << " level=" << level << " option=" << option << " errno=" << errno
			<< " errstr=" << std::strerror(errno);
		return false;
	}
	return true;
}

bool Socket::setOption(int level, int option, const void* result, socklen_t len) {
	if(setsockopt(_sock, level, option, result, len)) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT())
			<< "setOption sock=" << _sock << " level=" << level << " option=" << option << " errno=" << errno
			<< " errstr=" << std::strerror(errno);
		return false;
	}
	return true;
}

Socket::ptr Socket::accept() {
	Socket::ptr sock(new Socket(_family, _type, _protocol));
	int			newsock = ::accept(_sock, nullptr, nullptr);
	if(newsock == -1) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "accept(" << _sock << ") errno=" << errno << " errstr=" << std::strerror(errno);
		return nullptr;
	}
	if(sock->init(newsock)) {
		return sock;
	}
	return nullptr;
}

bool Socket::init(int sock) {
	FdCtx::ptr ctx = FdMgr::getInstance()->get(sock);
	if(ctx && ctx->isSocket() && !ctx->isClosed()) {
		_sock		 = sock;
		_isConnected = true;
		initSock();
		getLocalAddress();
		getRemoteAddress();
		return true;
	}
	return false;
}

bool Socket::bind(const Address::ptr addr) {
	if(!isValid()) {
		newSock();
		if(AZZATO_UNLIKELY(!isValid())) {
			return false;
		}
	}

	if(AZZATO_UNLIKELY(addr->getFamily() != _family)) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "bind sock.family(" << _family << ") addr.family("
											<< addr->getFamily() << ") not equal, addr=" << addr->toString();
		return false;
	}

	UnixAddress::ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
	if(uaddr) {
		Socket::ptr sock = Socket::createUnixTcpSocket();
		if(sock->connect(uaddr)) {
			return false;
		} else {
			FSUtil::unlink(uaddr->getPath(), true);
		}
	}

	if(::bind(_sock, addr->getAddr(), addr->getAddrLen())) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "bind error errno=" << errno << " errstr=" << std::strerror(errno);
		return false;
	}
	getLocalAddress();
	return true;
}

bool Socket::reconnect(uint64_t timeoutMs) {
	if(!_remoteAddress) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "reconnect _remoteAddress is null";
		return false;
	}
	_localAddress.reset();
	return connect(_remoteAddress, timeoutMs);
}

bool Socket::connect(const Address::ptr addr, uint64_t timeoutMs) {
	_remoteAddress = addr;
	if(!isValid()) {
		newSock();
		if(AZZATO_UNLIKELY(!isValid())) {
			return false;
		}
	}

	if(AZZATO_UNLIKELY(addr->getFamily() != _family)) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "connect sock.family(" << _family << ") addr.family("
											<< addr->getFamily() << ") not equal, addr=" << addr->toString();
		return false;
	}

	if(timeoutMs == static_cast<uint64_t>(-1)) {
		if(::connect(_sock, addr->getAddr(), addr->getAddrLen())) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
				<< "sock=" << _sock << " connect(" << addr->toString() << ") error errno=" << errno
				<< " errstr=" << std::strerror(errno);
			close();
			return false;
		}
	} else {
		if(::connect_with_timeout(_sock, addr->getAddr(), addr->getAddrLen(), timeoutMs)) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
				<< "sock=" << _sock << " connect(" << addr->toString() << ") timeout=" << timeoutMs
				<< " error errno=" << errno << " errstr=" << std::strerror(errno);
			close();
			return false;
		}
	}
	_isConnected = true;
	getRemoteAddress();
	getLocalAddress();
	return true;
}

bool Socket::listen(int backlog) {
	if(!isValid()) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "listen error sock=-1";
		return false;
	}
	if(::listen(_sock, backlog)) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "listen error errno=" << errno << " errstr=" << std::strerror(errno);
		return false;
	}
	return true;
}

bool Socket::close() {
	if(!_isConnected && _sock == -1) {
		return true;
	}
	_isConnected = false;
	if(_sock != -1) {
		::close(_sock);
		_sock = -1;
	}
	return false;
}

int Socket::send(const void* buffer, size_t length, int flags) {
	if(isConnected()) {
		return ::send(_sock, buffer, length, flags);
	}
	return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags) {
	if(isConnected()) {
		msghdr msg;
		std::memset(&msg, 0, sizeof(msg));
		msg.msg_iov	   = const_cast<iovec*>(buffers);
		msg.msg_iovlen = length;
		return ::sendmsg(_sock, &msg, flags);
	}
	return -1;
}

int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
	if(isConnected()) {
		return ::sendto(_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
	}
	return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
	if(isConnected()) {
		msghdr msg;
		std::memset(&msg, 0, sizeof(msg));
		msg.msg_iov		= const_cast<iovec*>(buffers);
		msg.msg_iovlen	= length;
		msg.msg_name	= const_cast<sockaddr*>(to->getAddr());
		msg.msg_namelen = to->getAddrLen();
		return ::sendmsg(_sock, &msg, flags);
	}
	return -1;
}

int Socket::recv(void* buffer, size_t length, int flags) {
	if(isConnected()) {
		return ::recv(_sock, buffer, length, flags);
	}
	return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags) {
	if(isConnected()) {
		msghdr msg;
		std::memset(&msg, 0, sizeof(msg));
		msg.msg_iov	   = buffers;
		msg.msg_iovlen = length;
		return ::recvmsg(_sock, &msg, flags);
	}
	return -1;
}

int Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
	if(isConnected()) {
		socklen_t len = from->getAddrLen();
		return ::recvfrom(_sock, buffer, length, flags, from->getAddr(), &len);
	}
	return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
	if(isConnected()) {
		msghdr msg;
		std::memset(&msg, 0, sizeof(msg));
		msg.msg_iov		= buffers;
		msg.msg_iovlen	= length;
		msg.msg_name	= from->getAddr();
		msg.msg_namelen = from->getAddrLen();
		return ::recvmsg(_sock, &msg, flags);
	}
	return -1;
}

Address::ptr Socket::getRemoteAddress() {
	if(_remoteAddress) {
		return _remoteAddress;
	}

	Address::ptr result;
	switch(_family) {
	case AF_INET:
		result.reset(new IPv4Address());
		break;
	case AF_INET6:
		result.reset(new IPv6Address());
		break;
	case AF_UNIX:
		result.reset(new UnixAddress());
		break;
	default:
		result.reset(new UnknownAddress(_family));
		break;
	}
	socklen_t addrlen = result->getAddrLen();
	if(getpeername(_sock, result->getAddr(), &addrlen)) {
		return Address::ptr(new UnknownAddress(_family));
	}
	if(_family == AF_UNIX) {
		UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
		addr->setAddrLen(addrlen);
	}
	_remoteAddress = result;
	return _remoteAddress;
}

Address::ptr Socket::getLocalAddress() {
	if(_localAddress) {
		return _localAddress;
	}

	Address::ptr result;
	switch(_family) {
	case AF_INET:
		result.reset(new IPv4Address());
		break;
	case AF_INET6:
		result.reset(new IPv6Address());
		break;
	case AF_UNIX:
		result.reset(new UnixAddress());
		break;
	default:
		result.reset(new UnknownAddress(_family));
		break;
	}
	socklen_t addrlen = result->getAddrLen();
	if(getsockname(_sock, result->getAddr(), &addrlen)) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "getsockname error sock=" << _sock << " errno=" << errno << " errstr=" << std::strerror(errno);
		return Address::ptr(new UnknownAddress(_family));
	}
	if(_family == AF_UNIX) {
		UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
		addr->setAddrLen(addrlen);
	}
	_localAddress = result;
	return _localAddress;
}

bool Socket::isValid() const { return _sock != -1; }

int Socket::getError() {
	int		  error = 0;
	socklen_t len	= sizeof(error);
	if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
		error = errno;
	}
	return error;
}

std::ostream& Socket::dump(std::ostream& os) const {
	os << "[Socket sock=" << _sock << " is_connected=" << _isConnected << " family=" << _family
	   << " type=" << _type << " protocol=" << _protocol;
	if(_localAddress) {
		os << " local_address=" << _localAddress->toString();
	}
	if(_remoteAddress) {
		os << " remote_address=" << _remoteAddress->toString();
	}
	os << "]";
	return os;
}

std::string Socket::toString() const {
	std::stringstream ss;
	dump(ss);
	return ss.str();
}

bool Socket::cancelRead() { return IOManager::getThis()->cancelEvent(_sock, IOManager::Read); }

bool Socket::cancelWrite() { return IOManager::getThis()->cancelEvent(_sock, IOManager::Write); }

bool Socket::cancelAccept() { return IOManager::getThis()->cancelEvent(_sock, IOManager::Read); }

bool Socket::cancelAll() { return IOManager::getThis()->cancelAll(_sock); }

void Socket::initSock() {
	int val = 1;
	setOption(SOL_SOCKET, SO_REUSEADDR, val);
	if(_type == SOCK_STREAM) {
		setOption(IPPROTO_TCP, TCP_NODELAY, val);
	}
}

void Socket::newSock() {
	_sock = socket(_family, _type, _protocol);
	if(AZZATO_LIKELY(_sock != -1)) {
		initSock();
	} else {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "socket(" << _family << ", " << _type << ", " << _protocol
											<< ") errno=" << errno << " errstr=" << std::strerror(errno);
	}
}

namespace {
struct SslInit {
	SslInit() { OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr); }
};

SslInit s_sslInit;
}  // namespace

SSLSocket::SSLSocket(int family, int type, int protocol)
	: Socket(family, type, protocol) {}

Socket::ptr SSLSocket::accept() {
	SSLSocket::ptr sock(new SSLSocket(_family, _type, _protocol));
	int			   newsock = ::accept(_sock, nullptr, nullptr);
	if(newsock == -1) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "accept(" << _sock << ") errno=" << errno << " errstr=" << std::strerror(errno);
		return nullptr;
	}
	sock->_ctx = _ctx;
	if(sock->init(newsock)) {
		return sock;
	}
	return nullptr;
}

bool SSLSocket::bind(const Address::ptr addr) { return Socket::bind(addr); }

bool SSLSocket::connect(const Address::ptr addr, uint64_t timeoutMs) {
	bool ok = Socket::connect(addr, timeoutMs);
	if(ok) {
		_ctx.reset(SSL_CTX_new(TLS_client_method()), SSL_CTX_free);
		_ssl.reset(SSL_new(_ctx.get()), SSL_free);
		SSL_set_fd(_ssl.get(), _sock);
		ok = (SSL_connect(_ssl.get()) == 1);
	}
	return ok;
}

bool SSLSocket::listen(int backlog) { return Socket::listen(backlog); }

bool SSLSocket::close() { return Socket::close(); }

int SSLSocket::send(const void* buffer, size_t length, int flags) {
	if(_ssl) {
		return SSL_write(_ssl.get(), buffer, static_cast<int>(length));
	}
	return -1;
}

int SSLSocket::send(const iovec* buffers, size_t length, int flags) {
	if(!_ssl) {
		return -1;
	}
	int total = 0;
	for(size_t i = 0; i < length; ++i) {
		int tmp = SSL_write(_ssl.get(), buffers[i].iov_base, static_cast<int>(buffers[i].iov_len));
		if(tmp <= 0) {
			return tmp;
		}
		total += tmp;
		if(tmp != static_cast<int>(buffers[i].iov_len)) {
			break;
		}
	}
	return total;
}

int SSLSocket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
	AZZATO_ASSERT(false);
	return -1;
}

int SSLSocket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
	AZZATO_ASSERT(false);
	return -1;
}

int SSLSocket::recv(void* buffer, size_t length, int flags) {
	if(_ssl) {
		return SSL_read(_ssl.get(), buffer, static_cast<int>(length));
	}
	return -1;
}

int SSLSocket::recv(iovec* buffers, size_t length, int flags) {
	if(!_ssl) {
		return -1;
	}
	int total = 0;
	for(size_t i = 0; i < length; ++i) {
		int tmp = SSL_read(_ssl.get(), buffers[i].iov_base, static_cast<int>(buffers[i].iov_len));
		if(tmp <= 0) {
			return tmp;
		}
		total += tmp;
		if(tmp != static_cast<int>(buffers[i].iov_len)) {
			break;
		}
	}
	return total;
}

int SSLSocket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
	AZZATO_ASSERT(false);
	return -1;
}

int SSLSocket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
	AZZATO_ASSERT(false);
	return -1;
}

bool SSLSocket::init(int sock) {
	bool ok = Socket::init(sock);
	if(ok) {
		_ssl.reset(SSL_new(_ctx.get()), SSL_free);
		SSL_set_fd(_ssl.get(), _sock);
		ok = (SSL_accept(_ssl.get()) == 1);
	}
	return ok;
}

bool SSLSocket::loadCertificates(const std::string& certFile, const std::string& keyFile) {
	_ctx.reset(SSL_CTX_new(TLS_server_method()), SSL_CTX_free);
	if(SSL_CTX_use_certificate_chain_file(_ctx.get(), certFile.c_str()) != 1) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "SSL_CTX_use_certificate_chain_file(" << certFile << ") error";
		return false;
	}
	if(SSL_CTX_use_PrivateKey_file(_ctx.get(), keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "SSL_CTX_use_PrivateKey_file(" << keyFile << ") error";
		return false;
	}
	if(SSL_CTX_check_private_key(_ctx.get()) != 1) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT())
			<< "SSL_CTX_check_private_key cert_file=" << certFile << " key_file=" << keyFile;
		return false;
	}
	return true;
}

SSLSocket::ptr SSLSocket::createTcp(Address::ptr address) {
	SSLSocket::ptr sock(new SSLSocket(address->getFamily(), Tcp, 0));
	return sock;
}

SSLSocket::ptr SSLSocket::createTcpSocket() {
	SSLSocket::ptr sock(new SSLSocket(Ipv4, Tcp, 0));
	return sock;
}

SSLSocket::ptr SSLSocket::createTcpSocket6() {
	SSLSocket::ptr sock(new SSLSocket(Ipv6, Tcp, 0));
	return sock;
}

std::ostream& SSLSocket::dump(std::ostream& os) const {
	os << "[SSLSocket sock=" << _sock << " is_connected=" << _isConnected << " family=" << _family
	   << " type=" << _type << " protocol=" << _protocol;
	if(_localAddress) {
		os << " local_address=" << _localAddress->toString();
	}
	if(_remoteAddress) {
		os << " remote_address=" << _remoteAddress->toString();
	}
	os << "]";
	return os;
}

std::ostream& operator<<(std::ostream& os, const Socket& sock) { return sock.dump(os); }

}  // namespace azzato
