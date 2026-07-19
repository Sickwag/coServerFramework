#include "streams/socket_stream.h"

namespace azzato {

SocketStream::SocketStream(Socket::ptr sock, bool owner)
	: _socket(std::move(sock))
	, _owner(owner) {}

SocketStream::~SocketStream() {
	if(_owner && _socket) {
		_socket->close();
	}
}

bool SocketStream::isConnected() const { return _socket && _socket->isConnected(); }

int SocketStream::read(void* buffer, size_t length) {
	if(!isConnected()) {
		return -1;
	}
	return _socket->recv(buffer, length);
}

int SocketStream::read(ByteArray::ptr ba, size_t length) {
	if(!isConnected()) {
		return -1;
	}
	std::vector<iovec> iovs;
	ba->getWriteBuffers(iovs, length);
	int rt = _socket->recv(iovs.data(), iovs.size());
	if(rt > 0) {
		ba->setPosition(ba->getPosition() + static_cast<size_t>(rt));
	}
	return rt;
}

int SocketStream::write(const void* buffer, size_t length) {
	if(!isConnected()) {
		return -1;
	}
	return _socket->send(buffer, length);
}

int SocketStream::write(ByteArray::ptr ba, size_t length) {
	if(!isConnected()) {
		return -1;
	}
	std::vector<iovec> iovs;
	ba->getReadBuffers(iovs, length);
	int rt = _socket->send(iovs.data(), iovs.size());
	if(rt > 0) {
		ba->setPosition(ba->getPosition() + static_cast<size_t>(rt));
	}
	return rt;
}

void SocketStream::close() {
	if(_socket) {
		_socket->close();
	}
}

Address::ptr SocketStream::getRemoteAddress() {
	if(_socket) {
		return _socket->getRemoteAddress();
	}
	return nullptr;
}

Address::ptr SocketStream::getLocalAddress() {
	if(_socket) {
		return _socket->getLocalAddress();
	}
	return nullptr;
}

std::string SocketStream::getRemoteAddressString() {
	auto addr = getRemoteAddress();
	return addr ? addr->toString() : "";
}

std::string SocketStream::getLocalAddressString() {
	auto addr = getLocalAddress();
	return addr ? addr->toString() : "";
}

}  // namespace azzato
