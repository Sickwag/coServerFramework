#pragma once

#include "address.h"
#include "iomanager.h"
#include "socket.h"
#include "utils/config.h"
#include "utils/noncopyable.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace azzato {

struct TcpServerConf {
	using ptr = std::shared_ptr<TcpServerConf>;

	std::vector<std::string> address;
	int						 keepalive = 0;
	int						 timeout   = 1000 * 2 * 60;
	int						 ssl	   = 0;
	std::string				 id;
	std::string				 type = "http";
	std::string				 name;
	std::string				 certFile;
	std::string				 keyFile;
	std::string				 acceptWorker;
	std::string				 ioWorker;
	std::string				 processWorker;
	std::map<std::string, std::string> args;

	bool isValid() const { return !address.empty(); }

	bool operator==(const TcpServerConf& other) const {
		return address == other.address && keepalive == other.keepalive && timeout == other.timeout
			   && name == other.name && ssl == other.ssl && certFile == other.certFile && keyFile == other.keyFile
			   && acceptWorker == other.acceptWorker && ioWorker == other.ioWorker
			   && processWorker == other.processWorker && args == other.args && id == other.id && type == other.type;
	}
};

template <>
class LexicalCast<std::string, TcpServerConf> {
  public:
	TcpServerConf operator()(const std::string& v) {
		YAML::Node	   node = YAML::Load(v);
		TcpServerConf conf;
		conf.id			   = node["id"].as<std::string>(conf.id);
		conf.type		   = node["type"].as<std::string>(conf.type);
		conf.keepalive	   = node["keepalive"].as<int>(conf.keepalive);
		conf.timeout	   = node["timeout"].as<int>(conf.timeout);
		conf.name		   = node["name"].as<std::string>(conf.name);
		conf.ssl		   = node["ssl"].as<int>(conf.ssl);
		conf.certFile	   = node["cert_file"].as<std::string>(conf.certFile);
		conf.keyFile	   = node["key_file"].as<std::string>(conf.keyFile);
		conf.acceptWorker  = node["accept_worker"].as<std::string>(conf.acceptWorker);
		conf.ioWorker	   = node["io_worker"].as<std::string>(conf.ioWorker);
		conf.processWorker = node["process_worker"].as<std::string>(conf.processWorker);
		conf.args = LexicalCast<std::string, std::map<std::string, std::string>>()(node["args"].as<std::string>(""));
		if(node["address"].IsDefined()) {
			for(size_t i = 0; i < node["address"].size(); ++i) {
				conf.address.push_back(node["address"][i].as<std::string>());
			}
		}
		return conf;
	}
};

template <>
class LexicalCast<TcpServerConf, std::string> {
  public:
	std::string operator()(const TcpServerConf& conf) {
		YAML::Node node;
		node["id"]			   = conf.id;
		node["type"]		   = conf.type;
		node["name"]		   = conf.name;
		node["keepalive"]	   = conf.keepalive;
		node["timeout"]		   = conf.timeout;
		node["ssl"]			   = conf.ssl;
		node["cert_file"]	   = conf.certFile;
		node["key_file"]	   = conf.keyFile;
		node["accept_worker"]  = conf.acceptWorker;
		node["io_worker"]	   = conf.ioWorker;
		node["process_worker"] = conf.processWorker;
		node["args"] = YAML::Load(LexicalCast<std::map<std::string, std::string>, std::string>()(conf.args));
		for(auto& i : conf.address) {
			node["address"].push_back(i);
		}
		std::stringstream ss;
		ss << node;
		return ss.str();
	}
};

/**
 * @brief Generic TCP server: accepts connections on an IOManager and
 *        dispatches each client to a handler fiber.
 */
class TcpServer : public std::enable_shared_from_this<TcpServer>, private Noncopyable {
  public:
	using ptr = std::shared_ptr<TcpServer>;

	TcpServer(IOManager* worker		  = IOManager::getThis(),
			  IOManager* ioWorker	  = IOManager::getThis(),
			  IOManager* acceptWorker = IOManager::getThis());

	virtual ~TcpServer();

	void setConf(const TcpServerConf& conf);

	virtual bool bind(Address::ptr addr, bool ssl = false);

	virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails, bool ssl = false);

	virtual bool start();

	virtual void stop();

	virtual void handleClient(Socket::ptr client);

	bool loadCertificates(const std::string& certFile, const std::string& keyFile);

	std::string toString(const std::string& prefix = "");

	const std::string& getName() const { return _name; }

	virtual void setName(const std::string& name) { _name = name; }

	Address::ptr getLocalAddress() const {
		return _socks.empty() ? nullptr : _socks[0]->getLocalAddress();
	}

	bool isStop() const { return _isStop; }

	uint64_t getRecvTimeout() const { return _recvTimeout; }

	void setRecvTimeout(uint64_t value) { _recvTimeout = value; }

  protected:
	virtual void startAccept(Socket::ptr sock);

  protected:
	std::vector<Socket::ptr> _socks;
	IOManager*				 _worker;
	IOManager*				 _ioWorker;
	IOManager*				 _acceptWorker;
	uint64_t				 _recvTimeout;
	std::string				 _name;
	std::string				 _type = "tcp";
	bool					 _isStop;
	bool					 _ssl = false;
	TcpServerConf::ptr		 _conf;
};

}  // namespace azzato
