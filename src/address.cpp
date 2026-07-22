#include "address.h"
#include "utils/endian.h"
#include "utils/macro.h"

#include <cstring>
#include <ifaddrs.h>
#include <netdb.h>
#include <sstream>
#include <stdexcept>

namespace azzato {

namespace {

template <typename T>
T createMask(uint32_t bits) {
	return static_cast<T>((1 << (sizeof(T) * 8 - bits)) - 1);
}

template <typename T>
uint32_t countBytes(T value) {
	uint32_t result = 0;
	for(; value; ++result) {
		value &= static_cast<T>(value - 1);
	}
	return result;
}

}  // namespace

Address::ptr Address::lookupAny(const std::string& host, int family, int type, int protocol) {
	std::vector<Address::ptr> result;
	if(lookup(result, host, family, type, protocol)) {
		return result[0];
	}
	return nullptr;
}

IPAddress::ptr Address::lookupAnyIPAddress(const std::string& host, int family, int type, int protocol) {
	std::vector<Address::ptr> result;
	if(lookup(result, host, family, type, protocol)) {
		for(auto& item : result) {
			IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(item);
			if(v) {
				return v;
			}
		}
	}
	return nullptr;
}

bool Address::lookup(std::vector<Address::ptr>& result,
					 const std::string&			host,
					 int						family,
					 int						type,
					 int						protocol) {
	addrinfo hints, *results, *next;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_flags	  = 0;
	hints.ai_family	  = family;
	hints.ai_socktype = type;
	hints.ai_protocol = protocol;

	std::string node;
	const char* service = nullptr;

	// IPv6 address in [addr] or [addr]:port form
	if(!host.empty() && host[0] == '[') {
		const char* end_ipv6 = static_cast<const char*>(std::memchr(host.c_str() + 1, ']', host.size() - 1));
		if(end_ipv6) {
			if(*(end_ipv6 + 1) == ':') {
				service = end_ipv6 + 2;
			}
			node = host.substr(1, static_cast<size_t>(end_ipv6 - host.c_str()) - 1);
		}
	}

	// node:service form
	if(node.empty()) {
		service = static_cast<const char*>(std::memchr(host.c_str(), ':', host.size()));
		if(service) {
			if(!std::memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
				node = host.substr(0, static_cast<size_t>(service - host.c_str()));
				++service;
			}
		}
	}

	if(node.empty()) {
		node = host;
	}
	int error = getaddrinfo(node.c_str(), service, &hints, &results);
	if(error) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT())
			<< "Address::lookup getaddrinfo(" << host << ", " << family << ", " << type << ") err=" << error
			<< " errstr=" << gai_strerror(error);
		return false;
	}

	next = results;
	while(next) {
		result.push_back(create(next->ai_addr, static_cast<socklen_t>(next->ai_addrlen)));
		next = next->ai_next;
	}
	freeaddrinfo(results);
	return !result.empty();
}

bool Address::getInterfaceAddresses(std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
									int															   family) {
	struct ifaddrs *next, *results;
	if(getifaddrs(&results) != 0) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "Address::getInterfaceAddresses getifaddrs err=" << errno
											<< " errstr=" << std::strerror(errno);
		return false;
	}

	try {
		for(next = results; next; next = next->ifa_next) {
			Address::ptr addr;
			uint32_t	 prefix_len = ~0u;
			if(family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
				continue;
			}
			switch(next->ifa_addr->sa_family) {
			case AF_INET: {
				addr			 = create(next->ifa_addr, sizeof(sockaddr_in));
				uint32_t netmask = reinterpret_cast<sockaddr_in*>(next->ifa_netmask)->sin_addr.s_addr;
				prefix_len		 = countBytes(netmask);
			} break;
			case AF_INET6: {
				addr			  = create(next->ifa_addr, sizeof(sockaddr_in6));
				in6_addr& netmask = reinterpret_cast<sockaddr_in6*>(next->ifa_netmask)->sin6_addr;
				prefix_len		  = 0;
				for(int i = 0; i < 16; ++i) {
					prefix_len += countBytes(netmask.s6_addr[i]);
				}
			} break;
			default:
				break;
			}

			if(addr) {
				result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_len)));
			}
		}
	} catch(...) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "Address::getInterfaceAddresses exception";
		freeifaddrs(results);
		return false;
	}
	freeifaddrs(results);
	return !result.empty();
}

bool Address::getInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t>>& result,
									const std::string&								iface,
									int												family) {
	if(iface.empty() || iface == "*") {
		if(family == AF_INET || family == AF_UNSPEC) {
			result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
		}
		if(family == AF_INET6 || family == AF_UNSPEC) {
			result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
		}
		return true;
	}

	std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;
	if(!getInterfaceAddresses(results, family)) {
		return false;
	}

	auto its = results.equal_range(iface);
	for(; its.first != its.second; ++its.first) {
		result.push_back(its.first->second);
	}
	return !result.empty();
}

int Address::getFamily() const { return getAddr()->sa_family; }

std::string Address::toString() const {
	std::stringstream ss;
	insert(ss);
	return ss.str();
}

Address::ptr Address::create(const sockaddr* addr, socklen_t addrlen) {
	if(addr == nullptr) {
		return nullptr;
	}

	Address::ptr result;
	switch(addr->sa_family) {
	case AF_INET:
		result.reset(new IPv4Address(*reinterpret_cast<const sockaddr_in*>(addr)));
		break;
	case AF_INET6:
		result.reset(new IPv6Address(*reinterpret_cast<const sockaddr_in6*>(addr)));
		break;
	default:
		result.reset(new UnknownAddress(*addr));
		break;
	}
	(void)addrlen;
	return result;
}

bool Address::operator<(const Address& rhs) const {
	socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
	int		  result = std::memcmp(getAddr(), rhs.getAddr(), minlen);
	if(result < 0) {
		return true;
	}
	if(result > 0) {
		return false;
	}
	return getAddrLen() < rhs.getAddrLen();
}

bool Address::operator==(const Address& rhs) const {
	return getAddrLen() == rhs.getAddrLen() && std::memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const { return !(*this == rhs); }

IPAddress::ptr IPAddress::create(const char* address, uint16_t port) {
	addrinfo hints, *results;
	std::memset(&hints, 0, sizeof(addrinfo));

	hints.ai_flags	= AI_NUMERICHOST;
	hints.ai_family = AF_UNSPEC;

	int error		= getaddrinfo(address, nullptr, &hints, &results);
	if(error) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT())
			<< "IPAddress::create(" << address << ", " << port << ") error=" << error << " errno=" << errno
			<< " errstr=" << std::strerror(errno);
		return nullptr;
	}

	try {
		IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
			Address::create(results->ai_addr, static_cast<socklen_t>(results->ai_addrlen)));
		if(result) {
			result->setPort(port);
		}
		freeaddrinfo(results);
		return result;
	} catch(...) {
		freeaddrinfo(results);
		return nullptr;
	}
}

IPv4Address::ptr IPv4Address::create(const char* address, uint16_t port) {
	IPv4Address::ptr result(new IPv4Address);
	result->_addr.sin_port = byteswapOnLittleEndian(port);
	int ok				   = inet_pton(AF_INET, address, &result->_addr.sin_addr);
	if(ok <= 0) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT())
			<< "IPv4Address::create(" << address << ", " << port << ") rt=" << ok << " errno=" << errno
			<< " errstr=" << std::strerror(errno);
		return nullptr;
	}
	return result;
}

IPv4Address::IPv4Address(const sockaddr_in& address) { _addr = address; }

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
	std::memset(&_addr, 0, sizeof(_addr));
	_addr.sin_family	  = AF_INET;
	_addr.sin_port		  = byteswapOnLittleEndian(port);
	_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
}

sockaddr* IPv4Address::getAddr() { return reinterpret_cast<sockaddr*>(&_addr); }

const sockaddr* IPv4Address::getAddr() const { return reinterpret_cast<const sockaddr*>(&_addr); }

socklen_t IPv4Address::getAddrLen() const { return sizeof(_addr); }

std::ostream& IPv4Address::insert(std::ostream& os) const {
	uint32_t addr = byteswapOnLittleEndian(_addr.sin_addr.s_addr);
	os << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "." << ((addr >> 8) & 0xff) << "."
	   << (addr & 0xff);
	os << ":" << byteswapOnLittleEndian(_addr.sin_port);
	return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefixLen) {
	if(prefixLen > 32) {
		return nullptr;
	}
	sockaddr_in baddr(_addr);
	baddr.sin_addr.s_addr |= byteswapOnLittleEndian(createMask<uint32_t>(prefixLen));
	return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefixLen) {
	if(prefixLen > 32) {
		return nullptr;
	}
	sockaddr_in baddr(_addr);
	baddr.sin_addr.s_addr &= byteswapOnLittleEndian(createMask<uint32_t>(prefixLen));
	return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefixLen) {
	sockaddr_in subnet;
	std::memset(&subnet, 0, sizeof(subnet));
	subnet.sin_family	   = AF_INET;
	subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(createMask<uint32_t>(prefixLen));
	return IPv4Address::ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::getPort() const { return byteswapOnLittleEndian(_addr.sin_port); }

void IPv4Address::setPort(uint16_t value) { _addr.sin_port = byteswapOnLittleEndian(value); }

IPv6Address::ptr IPv6Address::create(const char* address, uint16_t port) {
	IPv6Address::ptr result(new IPv6Address);
	result->_addr.sin6_port = byteswapOnLittleEndian(port);
	int ok					= inet_pton(AF_INET6, address, &result->_addr.sin6_addr);
	if(ok <= 0) {
		AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT())
			<< "IPv6Address::create(" << address << ", " << port << ") rt=" << ok << " errno=" << errno
			<< " errstr=" << std::strerror(errno);
		return nullptr;
	}
	return result;
}

IPv6Address::IPv6Address() {
	std::memset(&_addr, 0, sizeof(_addr));
	_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& address) { _addr = address; }

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
	std::memset(&_addr, 0, sizeof(_addr));
	_addr.sin6_family = AF_INET6;
	_addr.sin6_port	  = byteswapOnLittleEndian(port);
	std::memcpy(&_addr.sin6_addr.s6_addr, address, 16);
}

sockaddr* IPv6Address::getAddr() { return reinterpret_cast<sockaddr*>(&_addr); }

const sockaddr* IPv6Address::getAddr() const { return reinterpret_cast<const sockaddr*>(&_addr); }

socklen_t IPv6Address::getAddrLen() const { return sizeof(_addr); }

std::ostream& IPv6Address::insert(std::ostream& os) const {
	os << "[";
	const uint16_t* addr	  = reinterpret_cast<const uint16_t*>(_addr.sin6_addr.s6_addr);
	bool			usedZeros = false;
	for(size_t i = 0; i < 8; ++i) {
		if(addr[i] == 0 && !usedZeros) {
			continue;
		}
		if(i && addr[i - 1] == 0 && !usedZeros) {
			os << ":";
			usedZeros = true;
		}
		if(i) {
			os << ":";
		}
		os << std::hex << static_cast<int>(byteswapOnLittleEndian(addr[i])) << std::dec;
	}
	if(!usedZeros && addr[7] == 0) {
		os << "::";
	}
	os << "]:" << byteswapOnLittleEndian(_addr.sin6_port);
	return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefixLen) {
	sockaddr_in6 baddr(_addr);
	baddr.sin6_addr.s6_addr[prefixLen / 8] |= createMask<uint8_t>(prefixLen % 8);
	for(int i = prefixLen / 8 + 1; i < 16; ++i) {
		baddr.sin6_addr.s6_addr[i] = 0xff;
	}
	return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefixLen) {
	sockaddr_in6 baddr(_addr);
	baddr.sin6_addr.s6_addr[prefixLen / 8] &= createMask<uint8_t>(prefixLen % 8);
	for(int i = prefixLen / 8 + 1; i < 16; ++i) {
		baddr.sin6_addr.s6_addr[i] = 0x00;
	}
	return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefixLen) {
	sockaddr_in6 subnet;
	std::memset(&subnet, 0, sizeof(subnet));
	subnet.sin6_family						= AF_INET6;
	subnet.sin6_addr.s6_addr[prefixLen / 8] = ~createMask<uint8_t>(prefixLen % 8);
	for(uint32_t i = 0; i < prefixLen / 8; ++i) {
		subnet.sin6_addr.s6_addr[i] = 0xff;
	}
	return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const { return byteswapOnLittleEndian(_addr.sin6_port); }

void IPv6Address::setPort(uint16_t value) { _addr.sin6_port = byteswapOnLittleEndian(value); }

namespace {
constexpr size_t MAX_PATH_LEN = sizeof(sockaddr_un::sun_path) - 1;
}  // namespace

UnixAddress::UnixAddress() {
	std::memset(&_addr, 0, sizeof(_addr));
	_addr.sun_family = AF_UNIX;
	_length			 = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN);
}

UnixAddress::UnixAddress(const std::string& path) {
	std::memset(&_addr, 0, sizeof(_addr));
	_addr.sun_family = AF_UNIX;
	_length			 = static_cast<socklen_t>(path.size() + 1);

	if(!path.empty() && path[0] == '\0') {
		--_length;
	}

	if(_length > sizeof(_addr.sun_path)) {
		throw std::logic_error("path too long");
	}
	std::memcpy(_addr.sun_path, path.c_str(), _length);
	_length += offsetof(sockaddr_un, sun_path);
}

void UnixAddress::setAddrLen(uint32_t value) { _length = static_cast<socklen_t>(value); }

sockaddr* UnixAddress::getAddr() { return reinterpret_cast<sockaddr*>(&_addr); }

const sockaddr* UnixAddress::getAddr() const { return reinterpret_cast<const sockaddr*>(&_addr); }

socklen_t UnixAddress::getAddrLen() const { return _length; }

std::string UnixAddress::getPath() const {
	std::stringstream ss;
	if(_length > offsetof(sockaddr_un, sun_path) && _addr.sun_path[0] == '\0') {
		ss << "\\0" << std::string(_addr.sun_path + 1, _length - offsetof(sockaddr_un, sun_path) - 1);
	} else {
		ss << _addr.sun_path;
	}
	return ss.str();
}

std::ostream& UnixAddress::insert(std::ostream& os) const {
	if(_length > offsetof(sockaddr_un, sun_path) && _addr.sun_path[0] == '\0') {
		return os << "\\0" << std::string(_addr.sun_path + 1, _length - offsetof(sockaddr_un, sun_path) - 1);
	}
	return os << _addr.sun_path;
}

UnknownAddress::UnknownAddress(int family) {
	std::memset(&_addr, 0, sizeof(_addr));
	_addr.sa_family = static_cast<sa_family_t>(family);
}

UnknownAddress::UnknownAddress(const sockaddr& addr) { _addr = addr; }

sockaddr* UnknownAddress::getAddr() { return &_addr; }

const sockaddr* UnknownAddress::getAddr() const { return &_addr; }

socklen_t UnknownAddress::getAddrLen() const { return sizeof(_addr); }

std::ostream& UnknownAddress::insert(std::ostream& os) const {
	os << "[UnknownAddress family=" << _addr.sa_family << "]";
	return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) { return addr.insert(os); }

}  // namespace azzato
