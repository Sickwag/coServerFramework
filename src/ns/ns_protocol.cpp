#include "ns/ns_protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>

namespace azzato {
namespace ns {

NSNode::NSNode(const std::string& ip, uint16_t port, uint32_t weight)
	: _ip(ip)
	, _port(port)
	, _weight(weight) {
	_id = GetID(ip, port);
}

uint64_t NSNode::GetID(const std::string& ip, uint16_t port) {
	in_addr_t ip_addr = inet_addr(ip.c_str());
	uint64_t  v		  = (((uint64_t)ip_addr) << 32) | port;
	return v;
}

std::ostream& NSNode::dump(std::ostream& os, const std::string& prefix) {
	os << prefix << "[NSNode id=" << _id << " ip=" << _ip << " port=" << _port << " weight=" << _weight
	   << "]";
	return os;
}

std::string NSNode::toString(const std::string& prefix) {
	std::stringstream ss;
	dump(ss, prefix);
	return ss.str();
}

NSNodeSet::NSNodeSet(uint32_t cmd)
	: _cmd(cmd) {}

size_t NSNodeSet::size() {
	azzato::RWMutex::WriteLock lock(_mutex);
	return _datas.size();
}

void NSNodeSet::add(NSNode::ptr info) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_datas[info->getId()] = info;
}

NSNode::ptr NSNodeSet::del(uint64_t id) {
	NSNode::ptr				   rt;
	azzato::RWMutex::WriteLock lock(_mutex);
	auto					   it = _datas.find(id);
	if(it != _datas.end()) {
		rt = it->second;
		_datas.erase(it);
	}
	return rt;
}

std::ostream& NSNodeSet::dump(std::ostream& os, const std::string& prefix) {
	os << prefix << "[NSNodeSet cmd=" << _cmd;
	azzato::RWMutex::ReadLock lock(_mutex);
	os << " size=" << _datas.size() << "]" << std::endl;
	for(auto& i : _datas) {
		i.second->dump(os, prefix + "    ") << std::endl;
	}
	return os;
}

std::string NSNodeSet::toString(const std::string& prefix) {
	std::stringstream ss;
	dump(ss, prefix);
	return ss.str();
}

NSNode::ptr NSNodeSet::get(uint64_t id) {
	azzato::RWMutex::ReadLock lock(_mutex);
	auto					  it = _datas.find(id);
	return it == _datas.end() ? nullptr : it->second;
}

void NSNodeSet::listAll(std::vector<NSNode::ptr>& infos) {
	azzato::RWMutex::ReadLock lock(_mutex);
	for(auto& i : _datas) {
		infos.push_back(i.second);
	}
}

void NSDomain::add(NSNodeSet::ptr info) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_datas[info->getCmd()] = info;
}

size_t NSDomain::size() {
	azzato::RWMutex::WriteLock lock(_mutex);
	return _datas.size();
}

std::ostream& NSDomain::dump(std::ostream& os, const std::string& prefix) {
	os << prefix << "[NSDomain name=" << _domain;
	azzato::RWMutex::ReadLock lock(_mutex);
	os << " cmd_size=" << _datas.size() << "]" << std::endl;
	for(auto& i : _datas) {
		i.second->dump(os, prefix + "    ") << std::endl;
	}
	return os;
}

std::string NSDomain::toString(const std::string& prefix) {
	std::stringstream ss;
	dump(ss, prefix);
	return ss.str();
}

void NSDomain::add(uint32_t cmd, NSNode::ptr info) {
	auto ns = get(cmd);
	if(!ns) {
		ns.reset(new NSNodeSet(cmd));
		add(ns);
	}
	ns->add(info);
}

void NSDomain::del(uint32_t cmd) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_datas.erase(cmd);
}

NSNode::ptr NSDomain::del(uint32_t cmd, uint64_t id) {
	auto ns = get(cmd);
	if(!ns) {
		return nullptr;
	}
	auto info = ns->del(id);
	if(!ns->size()) {
		del(cmd);
	}
	return info;
}

NSNodeSet::ptr NSDomain::get(uint32_t cmd) {
	azzato::RWMutex::ReadLock lock(_mutex);
	auto					  it = _datas.find(cmd);
	return it == _datas.end() ? nullptr : it->second;
}

void NSDomain::listAll(std::vector<NSNodeSet::ptr>& infos) {
	azzato::RWMutex::ReadLock lock(_mutex);
	for(auto& i : _datas) {
		infos.push_back(i.second);
	}
}

void NSDomainSet::add(NSDomain::ptr info) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_datas[info->getDomain()] = info;
}

void NSDomainSet::del(const std::string& domain) {
	azzato::RWMutex::WriteLock lock(_mutex);
	_datas.erase(domain);
}

NSDomain::ptr NSDomainSet::get(const std::string& domain, bool auto_create) {
	{
		azzato::RWMutex::ReadLock lock(_mutex);
		auto					  it = _datas.find(domain);
		if(!auto_create) {
			return it == _datas.end() ? nullptr : it->second;
		}
	}
	azzato::RWMutex::WriteLock lock(_mutex);
	auto					   it = _datas.find(domain);
	if(it != _datas.end()) {
		return it->second;
	}
	NSDomain::ptr d(new NSDomain(domain));
	_datas[domain] = d;
	return d;
}

void NSDomainSet::del(const std::string& domain, uint32_t cmd, uint64_t id) {
	auto d = get(domain);
	if(!d) {
		return;
	}
	auto ns = d->get(cmd);
	if(!ns) {
		return;
	}
	ns->del(id);
}

void NSDomainSet::listAll(std::vector<NSDomain::ptr>& infos) {
	azzato::RWMutex::ReadLock lock(_mutex);
	for(auto& i : _datas) {
		infos.push_back(i.second);
	}
}

std::ostream& NSDomainSet::dump(std::ostream& os, const std::string& prefix) {
	azzato::RWMutex::ReadLock lock(_mutex);
	os << prefix << "[NSDomainSet domain_size=" << _datas.size() << "]" << std::endl;
	for(auto& i : _datas) {
		os << prefix;
		i.second->dump(os, prefix + "    ") << std::endl;
	}
	return os;
}

std::string NSDomainSet::toString(const std::string& prefix) {
	std::stringstream ss;
	dump(ss, prefix);
	return ss.str();
}

void NSDomainSet::swap(NSDomainSet& ds) {
	if(this == &ds) {
		return;
	}
	azzato::RWMutex::WriteLock lock(_mutex);
	azzato::RWMutex::WriteLock lock2(ds._mutex);
	_datas.swap(ds._datas);
}

}  // namespace ns
}  // namespace azzato
