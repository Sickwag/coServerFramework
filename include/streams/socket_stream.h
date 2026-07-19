#pragma once

#include "socket.h"
#include "stream.h"
#include <memory>
#include <string>

namespace azzato {

/**
 * @brief Stream adapter over a Socket.
 */
class SocketStream : public Stream {
  public:
	using ptr = std::shared_ptr<SocketStream>;

	SocketStream(Socket::ptr sock, bool owner = true);

	~SocketStream();

	int read(void* buffer, size_t length) override;

	int read(ByteArray::ptr ba, size_t length) override;

	int write(const void* buffer, size_t length) override;

	int write(ByteArray::ptr ba, size_t length) override;

	void close() override;

	Socket::ptr getSocket() const { return _socket; }

	bool isConnected() const;

	Address::ptr getRemoteAddress();

	Address::ptr getLocalAddress();

	std::string getRemoteAddressString();

	std::string getLocalAddressString();

  protected:
	Socket::ptr _socket;
	bool		_owner;
};

}  // namespace azzato
