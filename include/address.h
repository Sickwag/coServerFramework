#pragma once

#include "utils/noncopyable.h"
#include <arpa/inet.h>
#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <vector>

namespace azzato {

class IPAddress;

/**
 * @brief Abstract base of network addresses (IPv4/IPv6/Unix).
 */
class Address {
  public:
	using ptr		   = std::shared_ptr<Address>;

	virtual ~Address() = default;

	static ptr create(const sockaddr* addr, socklen_t addrlen);

	static bool lookup(std::vector<ptr>&  result,
					   const std::string& host,
					   int				  family   = AF_INET,
					   int				  type	   = 0,
					   int				  protocol = 0);

	static ptr lookupAny(const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);

	static std::shared_ptr<IPAddress>
	lookupAnyIPAddress(const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);

	static bool getInterfaceAddresses(std::multimap<std::string, std::pair<ptr, uint32_t>>& result,
									  int													family = AF_INET);

	static bool getInterfaceAddresses(std::vector<std::pair<ptr, uint32_t>>& result,
									  const std::string&					 iface,
									  int									 family = AF_INET);

	int getFamily() const;

	virtual const sockaddr* getAddr() const				 = 0;

	virtual sockaddr* getAddr()							 = 0;

	virtual socklen_t getAddrLen() const				 = 0;

	virtual std::ostream& insert(std::ostream& os) const = 0;

	std::string toString() const;

	bool operator<(const Address& rhs) const;

	bool operator==(const Address& rhs) const;

	bool operator!=(const Address& rhs) const;
};

/**
 * @brief Base class of IP addresses.
 */
class IPAddress : public Address {
  public:
	using ptr = std::shared_ptr<IPAddress>;

	static ptr create(const char* address, uint16_t port = 0);

	virtual ptr broadcastAddress(uint32_t prefixLen) = 0;

	virtual ptr networkAddress(uint32_t prefixLen)	 = 0;

	virtual ptr subnetMask(uint32_t prefixLen)		 = 0;

	virtual uint32_t getPort() const				 = 0;

	virtual void setPort(uint16_t value)			 = 0;
};

/**
 * @brief IPv4 address.
 */
class IPv4Address : public IPAddress {
  public:
	using ptr = std::shared_ptr<IPv4Address>;

	static ptr create(const char* address, uint16_t port = 0);

	explicit IPv4Address(const sockaddr_in& address);

	explicit IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

	const sockaddr* getAddr() const override;
	sockaddr*		getAddr() override;
	socklen_t		getAddrLen() const override;
	std::ostream&	insert(std::ostream& os) const override;

	IPAddress::ptr broadcastAddress(uint32_t prefixLen) override;
	IPAddress::ptr networkAddress(uint32_t prefixLen) override;
	IPAddress::ptr subnetMask(uint32_t prefixLen) override;
	uint32_t	   getPort() const override;
	void		   setPort(uint16_t value) override;

  private:
	sockaddr_in _addr;
};

/**
 * @brief IPv6 address.
 */
class IPv6Address : public IPAddress {
  public:
	using ptr = std::shared_ptr<IPv6Address>;

	static ptr create(const char* address, uint16_t port = 0);

	IPv6Address();

	explicit IPv6Address(const sockaddr_in6& address);

	IPv6Address(const uint8_t address[16], uint16_t port = 0);

	const sockaddr* getAddr() const override;
	sockaddr*		getAddr() override;
	socklen_t		getAddrLen() const override;
	std::ostream&	insert(std::ostream& os) const override;

	IPAddress::ptr broadcastAddress(uint32_t prefixLen) override;
	IPAddress::ptr networkAddress(uint32_t prefixLen) override;
	IPAddress::ptr subnetMask(uint32_t prefixLen) override;
	uint32_t	   getPort() const override;
	void		   setPort(uint16_t value) override;

  private:
	sockaddr_in6 _addr;
};

/**
 * @brief Unix-domain socket address.
 */
class UnixAddress : public Address {
  public:
	using ptr = std::shared_ptr<UnixAddress>;

	UnixAddress();

	explicit UnixAddress(const std::string& path);

	const sockaddr* getAddr() const override;
	sockaddr*		getAddr() override;
	socklen_t		getAddrLen() const override;
	void			setAddrLen(uint32_t value);
	std::string		getPath() const;
	std::ostream&	insert(std::ostream& os) const override;

  private:
	sockaddr_un _addr;
	socklen_t	_length;
};

/**
 * @brief Address of an unknown/unsupported family.
 */
class UnknownAddress : public Address {
  public:
	using ptr = std::shared_ptr<UnknownAddress>;

	explicit UnknownAddress(int family);

	explicit UnknownAddress(const sockaddr& addr);

	const sockaddr* getAddr() const override;
	sockaddr*		getAddr() override;
	socklen_t		getAddrLen() const override;
	std::ostream&	insert(std::ostream& os) const override;

  private:
	sockaddr _addr;
};

std::ostream& operator<<(std::ostream& os, const Address& addr);

}  // namespace azzato
