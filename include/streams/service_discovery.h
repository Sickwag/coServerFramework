#pragma once

#include "iomanager.h"
#include "mutex.h"
#include "zk_client.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace azzato {

class ServiceItemInfo {
  public:
	typedef std::shared_ptr<ServiceItemInfo> ptr;
	static ServiceItemInfo::ptr				 Create(const std::string& ip_and_port, const std::string& data);

	uint64_t getId() const { return _id; }

	uint16_t getPort() const { return _port; }

	const std::string& getIp() const { return _ip; }

	const std::string& getData() const { return _data; }

	std::string toString() const;

  private:
	uint64_t	_id;
	uint16_t	_port;
	std::string _ip;
	std::string _data;
};

class IServiceDiscovery {
  public:
	typedef std::shared_ptr<IServiceDiscovery> ptr;
	typedef std::function<void(const std::string&										 domain,
							   const std::string&										 service,
							   const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& old_value,
							   const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& new_value)>
		service_callback;

	virtual ~IServiceDiscovery() {}

	void registerServer(const std::string& domain,
						const std::string& service,
						const std::string& ip_and_port,
						const std::string& data);
	void queryServer(const std::string& domain, const std::string& service);
	void
		 listServer(std::unordered_map<
					std::string,
					std::unordered_map<std::string, std::unordered_map<uint64_t, ServiceItemInfo::ptr>>>& infos);
	void listRegisterServer(
		std::unordered_map<std::string,
						   std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>&
			infos);
	void listQueryServer(std::unordered_map<std::string, std::unordered_set<std::string>>& infos);

	virtual void start() = 0;
	virtual void stop()	 = 0;

	service_callback getServiceCallback() const { return _cb; }

	void setServiceCallback(service_callback v) { _cb = v; }

	void setQueryServer(const std::unordered_map<std::string, std::unordered_set<std::string>>& v);

  protected:
	azzato::RWMutex _mutex;
	// domain -> [service -> [id -> ServiceItemInfo] ]
	std::unordered_map<std::string,
					   std::unordered_map<std::string, std::unordered_map<uint64_t, ServiceItemInfo::ptr>>>
		_datas;
	// domain -> [service -> [ip_and_port -> data] ]
	std::unordered_map<std::string,
					   std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>
		_registerInfos;
	// domain -> [service]
	std::unordered_map<std::string, std::unordered_set<std::string>> _queryInfos;

	service_callback _cb;
};

class ZKServiceDiscovery : public IServiceDiscovery, public std::enable_shared_from_this<ZKServiceDiscovery> {
  public:
	typedef std::shared_ptr<ZKServiceDiscovery> ptr;
	ZKServiceDiscovery(const std::string& hosts);

	const std::string& getSelfInfo() const { return _selfInfo; }

	void setSelfInfo(const std::string& v) { _selfInfo = v; }

	const std::string& getSelfData() const { return _selfData; }

	void setSelfData(const std::string& v) { _selfData = v; }

	virtual void start();
	virtual void stop();

  private:
	void onWatch(int type, int stat, const std::string& path, ZKClient::ptr);
	void onZKConnect(const std::string& path, ZKClient::ptr client);
	void onZKChild(const std::string& path, ZKClient::ptr client);
	void onZKChanged(const std::string& path, ZKClient::ptr client);
	void onZKDeleted(const std::string& path, ZKClient::ptr client);
	void onZKExpiredSession(const std::string& path, ZKClient::ptr client);

	bool registerInfo(const std::string& domain,
					  const std::string& service,
					  const std::string& ip_and_port,
					  const std::string& data);
	bool queryInfo(const std::string& domain, const std::string& service);
	bool queryData(const std::string& domain, const std::string& service);

	bool existsOrCreate(const std::string& path);
	bool getChildren(const std::string& path);

  private:
	std::string		   _hosts;
	std::string		   _selfInfo;
	std::string		   _selfData;
	ZKClient::ptr	   _client;
	azzato::Timer::ptr _timer;
	bool			   _isOnTimer = false;
};

}  // namespace azzato
