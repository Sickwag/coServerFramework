#pragma once

#include "mutex.h"
#include "streams/service_discovery.h"
#include "streams/socket_stream.h"
#include "utils/util.h"
#include <unordered_map>
#include <vector>

namespace azzato {

class HolderStatsSet;

class HolderStats {
	friend class HolderStatsSet;

  public:
	uint32_t getUsedTime() const { return _usedTime; }

	uint32_t getTotal() const { return _total; }

	uint32_t getDoing() const { return _doing; }

	uint32_t getTimeouts() const { return _timeouts; }

	uint32_t getOks() const { return _oks; }

	uint32_t getErrs() const { return _errs; }

	uint32_t incUsedTime(uint32_t v) { return azzato::Atomic::addFetch(_usedTime, v); }

	uint32_t incTotal(uint32_t v) { return azzato::Atomic::addFetch(_total, v); }

	uint32_t incDoing(uint32_t v) { return azzato::Atomic::addFetch(_doing, v); }

	uint32_t incTimeouts(uint32_t v) { return azzato::Atomic::addFetch(_timeouts, v); }

	uint32_t incOks(uint32_t v) { return azzato::Atomic::addFetch(_oks, v); }

	uint32_t incErrs(uint32_t v) { return azzato::Atomic::addFetch(_errs, v); }

	uint32_t decDoing(uint32_t v) { return azzato::Atomic::subFetch(_doing, v); }

	void clear();

	float getWeight(float rate = 1.0f);

	std::string toString();

  private:
	uint32_t _usedTime = 0;
	uint32_t _total	   = 0;
	uint32_t _doing	   = 0;
	uint32_t _timeouts = 0;
	uint32_t _oks	   = 0;
	uint32_t _errs	   = 0;
};

class HolderStatsSet {
  public:
	HolderStatsSet(uint32_t size = 5);
	HolderStats& get(const uint32_t& now = time(0));

	float getWeight(const uint32_t& now = time(0));

	HolderStats getTotal();

  private:
	void init(const uint32_t& now);

  private:
	uint32_t				 _lastUpdateTime = 0;  // seconds
	std::vector<HolderStats> _stats;
};

class LoadBalanceItem {
  public:
	typedef std::shared_ptr<LoadBalanceItem> ptr;

	virtual ~LoadBalanceItem() {}

	SocketStream::ptr getStream() const { return _stream; }

	void setStream(SocketStream::ptr v) { _stream = v; }

	void setId(uint64_t v) { _id = v; }

	uint64_t getId() const { return _id; }

	HolderStats& get(const uint32_t& now = time(0));

	template <class T>
	std::shared_ptr<T> getStreamAs() {
		return std::dynamic_pointer_cast<T>(_stream);
	}

	virtual int32_t getWeight() { return _weight; }

	void setWeight(int32_t v) { _weight = v; }

	virtual bool isValid();
	void		 close();

	std::string toString();

  protected:
	uint64_t		  _id = 0;
	SocketStream::ptr _stream;
	int32_t			  _weight = 0;
	HolderStatsSet	  _stats;
};

class ILoadBalance {
  public:
	enum Type {
		ROUNDROBIN = 1,
		WEIGHT	   = 2,
		FAIR	   = 3
	};

	enum Error {
		NO_SERVICE	  = -101,
		NO_CONNECTION = -102,
	};

	typedef std::shared_ptr<ILoadBalance> ptr;

	virtual ~ILoadBalance() {}

	virtual LoadBalanceItem::ptr get(uint64_t v = -1) = 0;
};

class LoadBalance : public ILoadBalance {
  public:
	typedef azzato::RWMutex				 RWMutexType;
	typedef std::shared_ptr<LoadBalance> ptr;
	void								 add(LoadBalanceItem::ptr v);
	void								 del(LoadBalanceItem::ptr v);
	void								 set(const std::vector<LoadBalanceItem::ptr>& vs);

	LoadBalanceItem::ptr getById(uint64_t id);
	void				 update(const std::unordered_map<uint64_t, LoadBalanceItem::ptr>& adds,
								std::unordered_map<uint64_t, LoadBalanceItem::ptr>&		  dels);
	void				 init();

	std::string statusString(const std::string& prefix);

  protected:
	virtual void initNolock() = 0;
	void		 checkInit();

  protected:
	RWMutexType										   _mutex;
	std::unordered_map<uint64_t, LoadBalanceItem::ptr> _datas;
	uint64_t										   _lastInitTime = 0;
};

class RoundRobinLoadBalance : public LoadBalance {
  public:
	typedef std::shared_ptr<RoundRobinLoadBalance> ptr;
	virtual LoadBalanceItem::ptr				   get(uint64_t v = -1) override;

  protected:
	virtual void initNolock();

  protected:
	std::vector<LoadBalanceItem::ptr> _items;
};

// class FairLoadBalance;
class FairLoadBalanceItem : public LoadBalanceItem {
	// friend class FairLoadBalance;
  public:
	typedef std::shared_ptr<FairLoadBalanceItem> ptr;

	void			clear();
	virtual int32_t getWeight();
};

class WeightLoadBalance : public LoadBalance {
  public:
	typedef std::shared_ptr<WeightLoadBalance> ptr;
	virtual LoadBalanceItem::ptr			   get(uint64_t v = -1) override;

	FairLoadBalanceItem::ptr getAsFair();

  protected:
	virtual void initNolock();

  private:
	int32_t getIdx(uint64_t v = -1);

  protected:
	std::vector<LoadBalanceItem::ptr> _items;

  private:
	std::vector<int64_t> _weights;
};

// class FairLoadBalance : public LoadBalance {
// public:
//     typedef std::shared_ptr<FairLoadBalance> ptr;
//     virtual LoadBalanceItem::ptr get() override;
//     FairLoadBalanceItem::ptr getAsFair();
//
// protected:
//     virtual void initNolock();
// private:
//     int32_t getIdx();
// protected:
//     std::vector<LoadBalanceItem::ptr> _items;
// private:
//     std::vector<int32_t> _weights;
// };

class SDLoadBalance {
  public:
	typedef std::shared_ptr<SDLoadBalance>						   ptr;
	typedef std::function<SocketStream::ptr(ServiceItemInfo::ptr)> stream_callback;
	typedef azzato::RWMutex										   RWMutexType;

	SDLoadBalance(IServiceDiscovery::ptr sd);

	virtual ~SDLoadBalance() {}

	virtual void start();
	virtual void stop();

	stream_callback getCb() const { return _cb; }

	void setCb(stream_callback v) { _cb = v; }

	LoadBalance::ptr get(const std::string& domain, const std::string& service, bool auto_create = false);

	void initConf(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& confs);

	std::string statusString();

  private:
	void onServiceChange(const std::string&										   domain,
						 const std::string&										   service,
						 const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& old_value,
						 const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& new_value);

	ILoadBalance::Type	 getType(const std::string& domain, const std::string& service);
	LoadBalance::ptr	 createLoadBalance(ILoadBalance::Type type);
	LoadBalanceItem::ptr createLoadBalanceItem(ILoadBalance::Type type);

  protected:
	RWMutexType			   _mutex;
	IServiceDiscovery::ptr _sd;
	// domain -> [ service -> [ LoadBalance ] ]
	std::unordered_map<std::string, std::unordered_map<std::string, LoadBalance::ptr>>	 _datas;
	std::unordered_map<std::string, std::unordered_map<std::string, ILoadBalance::Type>> _types;
	ILoadBalance::Type _defaultType = ILoadBalance::FAIR;
	stream_callback	   _cb;
};

}  // namespace azzato
