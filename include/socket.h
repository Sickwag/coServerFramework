#pragma once

#include "address.h"
#include "utils/noncopyable.h"
#include <cstdint>
#include <memory>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <ostream>
#include <sys/socket.h>
#include <sys/types.h>

namespace azzato {

class Socket : public std::enable_shared_from_this<Socket>, private Noncopyable {
  public:
	using ptr	   = std::shared_ptr<Socket>;
	using weak_ptr = std::weak_ptr<Socket>;

	enum Type {
		Tcp = SOCK_STREAM,
		Udp = SOCK_DGRAM
	};

	enum Family {
		Ipv4 = AF_INET,
		Ipv6 = AF_INET6,
		Unix = AF_UNIX,
	};

	static ptr createTcp(Address::ptr address);

	static ptr createUdp(Address::ptr address);

	static ptr createTcpSocket();

	static ptr createUdpSocket();

	static ptr createTcpSocket6();

	static ptr createUdpSocket6();

	static ptr createUnixTcpSocket();

	static ptr createUnixUdpSocket();

	Socket(int family, int type, int protocol = 0);

	virtual ~Socket();

	int64_t getSendTimeout();

	void setSendTimeout(int64_t value);

	int64_t getRecvTimeout();

	void setRecvTimeout(int64_t value);

	bool getOption(int level, int option, void* result, socklen_t* len);

	template <typename T>
	bool getOption(int level, int option, T& result) {
		socklen_t length = sizeof(T);
		return getOption(level, option, &result, &length);
	}

	bool setOption(int level, int option, const void* result, socklen_t len);

	template <typename T>
	bool setOption(int level, int option, const T& value) {
		return setOption(level, option, &value, sizeof(T));
	}

	virtual ptr accept();

	virtual bool bind(const Address::ptr addr);

	virtual bool connect(const Address::ptr addr, uint64_t timeoutMs = static_cast<uint64_t>(-1));

	virtual bool reconnect(uint64_t timeoutMs = static_cast<uint64_t>(-1));

	virtual bool listen(int backlog = SOMAXCONN);

	virtual bool close();

	virtual int send(const void* buffer, size_t length, int flags = 0);

	virtual int send(const iovec* buffers, size_t length, int flags = 0);

	virtual int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);

	virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);

	virtual int recv(void* buffer, size_t length, int flags = 0);

	virtual int recv(iovec* buffers, size_t length, int flags = 0);

	virtual int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);

	virtual int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

	Address::ptr getRemoteAddress();

	Address::ptr getLocalAddress();

	int getFamily() const { return _family; }

	int getType() const { return _type; }

	int getProtocol() const { return _protocol; }

	bool isConnected() const { return _isConnected; }

	bool isValid() const;

	int getError();

	virtual std::ostream& dump(std::ostream& os) const;

	virtual std::string toString() const;

	int getSocket() const { return _sock; }

	bool cancelRead();

	bool cancelWrite();

	bool cancelAccept();

	bool cancelAll();

  protected:
	void initSock();

	void newSock();

	virtual bool init(int sock);

  protected:
	int			 _sock;
	int			 _family;
	int			 _type;
	int			 _protocol;
	bool		 _isConnected;
	Address::ptr _localAddress;
	Address::ptr _remoteAddress;
};

class SSLSocket : public Socket {
  public:
	using ptr = std::shared_ptr<SSLSocket>;

	static ptr createTcp(Address::ptr address);

	static ptr createTcpSocket();

	static ptr createTcpSocket6();

	SSLSocket(int family, int type, int protocol = 0);

	Socket::ptr accept() override;
	bool		bind(const Address::ptr addr) override;
	bool		connect(const Address::ptr addr, uint64_t timeoutMs = static_cast<uint64_t>(-1)) override;
	bool		listen(int backlog = SOMAXCONN) override;
	bool		close() override;
	int			send(const void* buffer, size_t length, int flags = 0) override;
	int			send(const iovec* buffers, size_t length, int flags = 0) override;
	int			sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0) override;
	int			sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0) override;
	int			recv(void* buffer, size_t length, int flags = 0) override;
	int			recv(iovec* buffers, size_t length, int flags = 0) override;
	int			recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0) override;
	int			recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0) override;

	bool		  loadCertificates(const std::string& certFile, const std::string& keyFile);
	std::ostream& dump(std::ostream& os) const override;

  protected:
	bool init(int sock) override;

  private:
	std::shared_ptr<SSL_CTX> _ctx;
	std::shared_ptr<SSL>	 _ssl;
};

std::ostream& operator<<(std::ostream& os, const Socket& sock);

}  // namespace azzato
