#include "streams/load_balance.h"
#include "log.h"
#include "utils/macro.h"
#include "worker.h"
#include <math.h>

namespace azzato {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

HolderStats HolderStatsSet::getTotal() {
	HolderStats rt;
	for(auto& i : _stats) {
#define XX(f) rt.f += i.f
		XX(_usedTime);
		XX(_total);
		XX(_doing);
		XX(_timeouts);
		XX(_oks);
		XX(_errs);
#undef XX
	}
	return rt;
}

std::string HolderStats::toString() {
	std::stringstream ss;
	ss << "[Stat total=" << _total << " used_time=" << _usedTime << " doing=" << _doing
	   << " timeouts=" << _timeouts << " oks=" << _oks << " errs=" << _errs
	   << " oks_rate=" << (_total ? (_oks * 100.0 / _total) : 0)
	   << " errs_rate=" << (_total ? (_errs * 100.0 / _total) : 0)
	   << " avg_used=" << (_oks ? (_usedTime * 1.0 / _oks) : 0) << " weight=" << getWeight(1) << "]";
	return ss.str();
}

void LoadBalanceItem::close() {
	if(_stream) {
		auto stream = _stream;
		azzato::WorkerMgr::getInstance()->schedule("service_io", [stream]() { stream->close(); });
	}
}

bool LoadBalanceItem::isValid() { return _stream && _stream->isConnected(); }

std::string LoadBalanceItem::toString() {
	std::stringstream ss;
	ss << "[Item id=" << _id << " weight=" << getWeight();
	if(!_stream) {
		ss << " stream=null";
	} else {
		ss << " stream=[" << _stream->getRemoteAddressString() << " is_connected=" << _stream->isConnected()
		   << "]";
	}
	ss << _stats.getTotal().toString() << "]";
	// float w = 0;
	// float w2 = 0;
	// for(uint64_t n = 0; n < 5; ++n) {
	//     if(n) {
	//         ss << ",";
	//     } else {
	//         ss << _stats.get(time(0) - n).toString();
	//     }
	//     w += _stats.get(time(0) - n).getWeight();
	//     w2 += _stats.get(time(0) - n).getWeight() * (1 - n * 0.1);
	//     ss << _stats.get(time(0) - n).getWeight();
	// }
	// ss << " w=" << w;
	// ss << " w2=" << w2;
	return ss.str();
}

LoadBalanceItem::ptr LoadBalance::getById(uint64_t id) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _datas.find(id);
	return it == _datas.end() ? nullptr : it->second;
}

void LoadBalance::add(LoadBalanceItem::ptr v) {
	RWMutexType::WriteLock lock(_mutex);
	_datas[v->getId()] = v;
	initNolock();
}

void LoadBalance::del(LoadBalanceItem::ptr v) {
	RWMutexType::WriteLock lock(_mutex);
	_datas.erase(v->getId());
	initNolock();
}

void LoadBalance::update(const std::unordered_map<uint64_t, LoadBalanceItem::ptr>& adds,
						 std::unordered_map<uint64_t, LoadBalanceItem::ptr>&	   dels) {
	RWMutexType::WriteLock lock(_mutex);
	for(auto& i : dels) {
		auto it = _datas.find(i.first);
		if(it != _datas.end()) {
			i.second = it->second;
			_datas.erase(it);
		}
	}
	for(auto& i : adds) {
		_datas[i.first] = i.second;
	}
	initNolock();
}

void LoadBalance::set(const std::vector<LoadBalanceItem::ptr>& vs) {
	RWMutexType::WriteLock lock(_mutex);
	_datas.clear();
	for(auto& i : vs) {
		_datas[i->getId()] = i;
	}
	initNolock();
}

void LoadBalance::init() {
	RWMutexType::WriteLock lock(_mutex);
	initNolock();
}

std::string LoadBalance::statusString(const std::string& prefix) {
	RWMutexType::ReadLock lock(_mutex);
	decltype(_datas)	  datas = _datas;
	lock.unlock();
	std::stringstream ss;
	ss << prefix << "init_time: " << azzato::time2Str(_lastInitTime / 1000) << std::endl;
	for(auto& i : datas) {
		ss << prefix << i.second->toString() << std::endl;
	}
	return ss.str();
}

void LoadBalance::checkInit() {
	uint64_t ts = azzato::getCurrentMS();
	if(ts - _lastInitTime > 500) {
		init();
		_lastInitTime = ts;
	}
}

void RoundRobinLoadBalance::initNolock() {
	decltype(_items) items;
	for(auto& i : _datas) {
		if(i.second->isValid()) {
			items.push_back(i.second);
		}
	}
	items.swap(_items);
}

LoadBalanceItem::ptr RoundRobinLoadBalance::get(uint64_t v) {
	checkInit();
	RWMutexType::ReadLock lock(_mutex);
	if(_items.empty()) {
		return nullptr;
	}
	uint32_t r = (v == (uint64_t)-1 ? rand() : v) % _items.size();
	for(size_t i = 0; i < _items.size(); ++i) {
		auto& h = _items[(r + i) % _items.size()];
		if(h->isValid()) {
			return h;
		}
	}
	return nullptr;
}

FairLoadBalanceItem::ptr WeightLoadBalance::getAsFair() {
	auto item = get();
	if(item) {
		return std::static_pointer_cast<FairLoadBalanceItem>(item);
	}
	return nullptr;
}

LoadBalanceItem::ptr WeightLoadBalance::get(uint64_t v) {
	checkInit();
	RWMutexType::ReadLock lock(_mutex);
	int32_t				  idx = getIdx(v);
	if(idx == -1) {
		return nullptr;
	}

	// TODO fix weight
	for(size_t i = 0; i < _items.size(); ++i) {
		auto& h = _items[(idx + i) % _items.size()];
		if(h->isValid()) {
			return h;
		}
	}
	return nullptr;
}

void WeightLoadBalance::initNolock() {
	decltype(_items) items;
	for(auto& i : _datas) {
		if(i.second->isValid()) {
			items.push_back(i.second);
		}
	}
	items.swap(_items);

	int64_t total = 0;
	_weights.resize(_items.size());
	for(size_t i = 0; i < _items.size(); ++i) {
		total += _items[i]->getWeight();
		_weights[i] = total;
	}
}

int32_t WeightLoadBalance::getIdx(uint64_t v) {
	if(_weights.empty()) {
		return -1;
	}
	int64_t	 total = *_weights.rbegin();
	uint64_t dis   = (v == (uint64_t)-1 ? rand() : v) % total;
	auto	 it	   = std::upper_bound(_weights.begin(), _weights.end(), dis);
	AZZATO_ASSERT(it != _weights.end());
	return std::distance(_weights.begin(), it);
}

void HolderStats::clear() {
	_usedTime = 0;
	_total	  = 0;
	_doing	  = 0;
	_timeouts = 0;
	_oks	  = 0;
	_errs	  = 0;
}

float HolderStats::getWeight(float rate) {
	// if(_total == 0) {
	//     return 0.1;
	// }
	float base = _total + 20;
	return std::min((_oks * 1.0 / (_usedTime + 1)) * 2.0, 50.0) * (1 - 4.0 * _timeouts / base)
		   * (1 - 1 * _doing / base) * (1 - 10.0 * _errs / base) * rate;
	// return std::min((_oks * 1.0 / (_usedTime + 1)) * 10.0, 100.0)
	//     * (1 - (2.0 * pow(_timeouts, 1.3) / base))
	//     * (1 - (1.0 * pow(_doing, 1.1) / base))
	//     * (1 - (4.0 * pow(_errs, 1.5) / base)) * rate;
	// return std::min(((_oks + 1) * 1.0 / (_usedTime + 1)) * 10.0, 100.0)
	//     * std::min((base / (_timeouts * 3.0 + 1)) / 100.0, 10.0)
	//     * std::min((base / ( _doing * 1.0 + 1)) / 100.0, 10.0)
	//     * std::min((base / (_errs * 5.0 + 1)) / 100.0, 10.0);
}

HolderStatsSet::HolderStatsSet(uint32_t size) { _stats.resize(size); }

void HolderStatsSet::init(const uint32_t& now) {
	if(_lastUpdateTime < now) {
		for(uint32_t t = _lastUpdateTime + 1, i = 0; t <= now && i < _stats.size(); ++t, ++i) {
			_stats[t % _stats.size()].clear();
		}
		_lastUpdateTime = now;
	}
}

HolderStats& HolderStatsSet::get(const uint32_t& now) {
	init(now);
	return _stats[now % _stats.size()];
}

float HolderStatsSet::getWeight(const uint32_t& now) {
	init(now);
	float v = 0;
	for(size_t i = 1; i < _stats.size(); ++i) {
		v += _stats[(now - i) % _stats.size()].getWeight(1 - 0.1 * i);
	}
	return v;
	// return getTotal().getWeight(1.0);
}

int32_t FairLoadBalanceItem::getWeight() {
	int32_t v = _weight * _stats.getWeight();
	if(_stream->isConnected()) {
		return v > 1 ? v : 1;
	}
	return 1;
}

HolderStats& LoadBalanceItem::get(const uint32_t& now) { return _stats.get(now); }

// LoadBalanceItem::ptr FairLoadBalance::get() {
//     RWMutexType::ReadLock lock(_mutex);
//     int32_t idx = getIdx();
//     if(idx == -1) {
//         return nullptr;
//     }
//
//     //TODO fix weight
//     for(size_t i = 0; i < _items.size(); ++i) {
//         auto& h = _items[(idx + i) % _items.size()];
//         if(h->isValid()) {
//             return h;
//         }
//     }
//     return nullptr;
// }
//
// void FairLoadBalance::initNolock() {
//     decltype(_items) items;
//     for(auto& i : _datas){
//         items.push_back(i.second);
//     }
//     items.swap(_items);
//
//     _weights.resize(_items.size());
//     int32_t total = 0;
//     for(size_t i = 0; i < _items.size(); ++i) {
//         total += _items[i]->getWeight();
//         _weights[i] = total;
//     }
// }
//
// int32_t FairLoadBalance::getIdx() {
//     if(_weights.empty()) {
//         return -1;
//     }
//     int32_t total = *_weights.rbegin();
//     auto it = std::upper_bound(_weights.begin()
//                 ,_weights.end(), rand() % total);
//     return std::distance(it, _weights.begin());
// }

SDLoadBalance::SDLoadBalance(IServiceDiscovery::ptr sd)
	: _sd(sd) {}

LoadBalance::ptr SDLoadBalance::get(const std::string& domain, const std::string& service, bool auto_create) {
	do {
		RWMutexType::ReadLock lock(_mutex);
		auto				  it = _datas.find(domain);
		if(it == _datas.end()) {
			break;
		}
		auto iit = it->second.find(service);
		if(iit == it->second.end()) {
			break;
		}
		return iit->second;
	} while(0);

	if(!auto_create) {
		return nullptr;
	}

	auto type				  = getType(domain, service);

	auto				   lb = createLoadBalance(type);
	RWMutexType::WriteLock lock(_mutex);
	_datas[domain][service] = lb;
	lock.unlock();
	return lb;
}

ILoadBalance::Type SDLoadBalance::getType(const std::string& domain, const std::string& service) {
	RWMutexType::ReadLock lock(_mutex);
	auto				  it = _types.find(domain);
	if(it == _types.end()) {
		return _defaultType;
	}
	auto iit = it->second.find(service);
	if(iit == it->second.end()) {
		return _defaultType;
	}
	return iit->second;
}

LoadBalance::ptr SDLoadBalance::createLoadBalance(ILoadBalance::Type type) {
	if(type == ILoadBalance::ROUNDROBIN) {
		return RoundRobinLoadBalance::ptr(new RoundRobinLoadBalance);
	} else if(type == ILoadBalance::WEIGHT) {
		return WeightLoadBalance::ptr(new WeightLoadBalance);
	} else if(type == ILoadBalance::FAIR) {
		return WeightLoadBalance::ptr(new WeightLoadBalance);
	}
	return nullptr;
}

LoadBalanceItem::ptr SDLoadBalance::createLoadBalanceItem(ILoadBalance::Type type) {
	LoadBalanceItem::ptr item;
	if(type == ILoadBalance::ROUNDROBIN) {
		item.reset(new LoadBalanceItem);
	} else if(type == ILoadBalance::WEIGHT) {
		item.reset(new LoadBalanceItem);
	} else if(type == ILoadBalance::FAIR) {
		item.reset(new FairLoadBalanceItem);
	}
	return item;
}

void SDLoadBalance::onServiceChange(const std::string&										  domain,
									const std::string&										  service,
									const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& old_value,
									const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& new_value) {
	AZZATO_LOG_INFO(g_logger) << "onServiceChange domain=" << domain << " service=" << service;
	auto											   type = getType(domain, service);
	auto											   lb	= get(domain, service, true);
	std::unordered_map<uint64_t, ServiceItemInfo::ptr> add_values;
	std::unordered_map<uint64_t, LoadBalanceItem::ptr> del_infos;

	for(auto& i : old_value) {
		if(new_value.find(i.first) == new_value.end()) {
			del_infos[i.first];
		}
	}
	for(auto& i : new_value) {
		if(old_value.find(i.first) == old_value.end()) {
			add_values.insert(i);
		}
	}

	std::unordered_map<uint64_t, LoadBalanceItem::ptr> add_infos;
	for(auto& i : add_values) {
		auto stream = _cb(i.second);
		if(!stream) {
			AZZATO_LOG_ERROR(g_logger) << "create stream fail, " << i.second->toString();
			continue;
		}

		LoadBalanceItem::ptr lditem = createLoadBalanceItem(type);
		lditem->setId(i.first);
		lditem->setStream(stream);
		lditem->setWeight(10000);

		add_infos[i.first] = lditem;
	}

	lb->update(add_infos, del_infos);
	for(auto& i : del_infos) {
		if(i.second) {
			i.second->close();
		}
	}
}

void SDLoadBalance::start() {
	_sd->setServiceCallback(std::bind(&SDLoadBalance::onServiceChange,
									  this,
									  std::placeholders::_1,
									  std::placeholders::_2,
									  std::placeholders::_3,
									  std::placeholders::_4));
	_sd->start();
}

void SDLoadBalance::stop() { _sd->stop(); }

void SDLoadBalance::initConf(
	const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& confs) {
	decltype(_types)												 types;
	std::unordered_map<std::string, std::unordered_set<std::string>> query_infos;
	for(auto& i : confs) {
		for(auto& n : i.second) {
			ILoadBalance::Type t = ILoadBalance::FAIR;
			if(n.second == "round_robin") {
				t = ILoadBalance::ROUNDROBIN;
			} else if(n.second == "weight") {
				t = ILoadBalance::WEIGHT;
			}
			types[i.first][n.first] = t;
			query_infos[i.first].insert(n.first);
		}
	}
	_sd->setQueryServer(query_infos);
	RWMutexType::WriteLock lock(_mutex);
	types.swap(_types);
	lock.unlock();
}

std::string SDLoadBalance::statusString() {
	RWMutexType::ReadLock lock(_mutex);
	decltype(_datas)	  datas = _datas;
	lock.unlock();
	std::stringstream ss;
	for(auto& i : datas) {
		ss << i.first << ":" << std::endl;
		for(auto& n : i.second) {
			ss << "\t" << n.first << ":" << std::endl;
			ss << n.second->statusString("\t\t") << std::endl;
		}
	}
	return ss.str();
}

}  // namespace azzato
