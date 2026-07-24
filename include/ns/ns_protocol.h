#pragma once

#include "mutex.h"
#include "ns/ns_protobuf.pb.h"
#include <iostream>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>

namespace azzato {
namespace ns {

enum class NSCommand {
	/// 注册节点信息
	REGISTER		= 0x10001,
	/// 查询节点信息
	QUERY			= 0x10002,
	/// 设置黑名单
	SET_BLACKLIST	= 0x10003,
	/// 查询黑名单
	QUERY_BLACKLIST = 0x10004,
	/// 心跳
	TICK			= 0x10005,
};

enum class NSNotify {
	NODE_CHANGE = 0x10001,
};

class NSNode {
  public:
	typedef std::shared_ptr<NSNode> ptr;

	NSNode(const std::string& ip, uint16_t port, uint32_t weight);

	const std::string& getIp() const { return _ip; }

	uint16_t getPort() const { return _port; }

	uint32_t getWeight() const { return _weight; }

	// void setIp(const std::string& v) { _ip = v;}
	// void setPort(uint16_t v) { _port = v;}
	void setWeight(uint32_t v) { _weight = v; }

	uint64_t getId() const { return _id; }

	static uint64_t GetID(const std::string& ip, uint16_t port);
	std::ostream&	dump(std::ostream& os, const std::string& prefix = "");
	std::string		toString(const std::string& prefix = "");

  private:
	uint64_t	_id;
	std::string _ip;
	uint16_t	_port;
	uint32_t	_weight;
};

class NSNodeSet {
  public:
	typedef std::shared_ptr<NSNodeSet> ptr;
	NSNodeSet(uint32_t cmd);

	void		add(NSNode::ptr info);
	NSNode::ptr del(uint64_t id);
	NSNode::ptr get(uint64_t id);

	uint32_t getCmd() const { return _cmd; }

	void setCmd(uint32_t v) { _cmd = v; }

	void		  listAll(std::vector<NSNode::ptr>& infos);
	std::ostream& dump(std::ostream& os, const std::string& prefix = "");
	std::string	  toString(const std::string& prefix = "");

	size_t size();

  private:
	azzato::RWMutex					_mutex;
	uint32_t						_cmd;
	std::map<uint64_t, NSNode::ptr> _datas;
};

class NSDomain {
  public:
	typedef std::shared_ptr<NSDomain> ptr;

	NSDomain(const std::string& domain)
		: _domain(domain) {}

	const std::string& getDomain() const { return _domain; }

	void setDomain(const std::string& v) { _domain = v; }

	void		   add(NSNodeSet::ptr info);
	void		   add(uint32_t cmd, NSNode::ptr info);
	void		   del(uint32_t cmd);
	NSNode::ptr	   del(uint32_t cmd, uint64_t id);
	NSNodeSet::ptr get(uint32_t cmd);
	void		   listAll(std::vector<NSNodeSet::ptr>& infos);
	std::ostream&  dump(std::ostream& os, const std::string& prefix = "");
	std::string	   toString(const std::string& prefix = "");
	size_t		   size();

  private:
	std::string						   _domain;
	azzato::RWMutex					   _mutex;
	std::map<uint32_t, NSNodeSet::ptr> _datas;
};

class NSDomainSet {
  public:
	typedef std::shared_ptr<NSDomainSet> ptr;

	void		  add(NSDomain::ptr info);
	void		  del(const std::string& domain);
	NSDomain::ptr get(const std::string& domain, bool auto_create = false);

	void del(const std::string& domain, uint32_t cmd, uint64_t id);
	void listAll(std::vector<NSDomain::ptr>& infos);

	std::ostream& dump(std::ostream& os, const std::string& prefix = "");
	std::string	  toString(const std::string& prefix = "");

	void swap(NSDomainSet& ds);

  private:
	azzato::RWMutex						 _mutex;
	std::map<std::string, NSDomain::ptr> _datas;
};

}  // namespace ns
}  // namespace azzato
