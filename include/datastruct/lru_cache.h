#pragma once

#include "datastruct/cache_status.h"
#include "mutex.h"
#include <algorithm>
#include <atomic>
#include <list>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace azzato {

template <class K, class V, class MutexType = azzato::Mutex>
class LruCache {
  public:
	typedef std::shared_ptr<LruCache> ptr;

	typedef std::pair<K, V>										item_type;
	typedef std::list<item_type>								list_type;
	typedef std::unordered_map<K, typename list_type::iterator> map_type;
	typedef std::function<void(const K&, const V&)>				prune_callback;

	LruCache(size_t max_size = 0, size_t elasticity = 0, CacheStatus* status = nullptr)
		: _maxSize(max_size)
		, _elasticity(elasticity) {
		_status = status;

		if(_status == nullptr) {
			_status	  = new CacheStatus;
			_statusOwner = true;
		}
	}

	~LruCache() {
		if(_statusOwner && _status) {
			delete _status;
		}
	}

	void set(const K& k, const V& v) {
		_status->incSet();
		typename MutexType::Lock lock(_mutex);
		auto					 it = _cache.find(k);
		if(it != _cache.end()) {
			it->second->second = v;
			_keys.splice(_keys.begin(), _keys, it->second);
			return;
		}
		_keys.emplace_front(std::make_pair(k, v));
		_cache.insert(std::make_pair(k, _keys.begin()));
		prune();
	}

	bool get(const K& k, V& v) {
		_status->incGet();
		typename MutexType::Lock lock(_mutex);
		auto					 it = _cache.find(k);
		if(it == _cache.end()) {
			return false;
		}
		_keys.splice(_keys.begin(), _keys, it->second);
		v = it->second->second;
		lock.unlock();
		_status->incHit();
		return true;
	}

	V get(const K& k) {
		_status->incGet();
		typename MutexType::Lock lock(_mutex);
		auto					 it = _cache.find(k);
		if(it == _cache.end()) {
			return V();
		}
		_keys.splice(_keys.begin(), _keys, it->second);
		auto v = it->second->second;
		lock.unlock();
		_status->incHit();
		return v;
	}

	bool del(const K& k) {
		_status->incDel();
		typename MutexType::Lock lock(_mutex);
		auto					 it = _cache.find(k);
		if(it == _cache.end()) {
			return false;
		}
		_keys.erase(it->second);
		_cache.erase(it);
		return true;
	}

	bool exists(const K& k) {
		typename MutexType::Lock lock(_mutex);
		return _cache.find(k) != _cache.end();
	}

	size_t size() {
		typename MutexType::Lock lock(_mutex);
		return _cache.size();
	}

	bool empty() {
		typename MutexType::Lock lock(_mutex);
		return _cache.empty();
	}

	bool clear() {
		typename MutexType::Lock lock(_mutex);
		_cache.clear();
		_keys.clear();
		return true;
	}

	void setMaxSize(const size_t& v) { _maxSize = v; }
	void setElasticity(const size_t& v) { _elasticity = v; }

	size_t getMaxSize() const { return _maxSize; }
	size_t getElasticity() const { return _elasticity; }
	size_t getMaxAllowedSize() const { return _maxSize + _elasticity; }

	template <class F>
	void foreach(F& f) {
		typename MutexType::Lock lock(_mutex);
		std::for_each(_cache.begin(), _cache.end(), f);
	}

	void setPruneCallback(prune_callback cb) { _cb = cb; }

	std::string toStatusString() {
		std::stringstream ss;
		ss << (_status ? _status->toString() : "(no status)") << " total=" << size();
		return ss.str();
	}

	CacheStatus* getStatus() const { return _status; }

	void setStatus(CacheStatus* v, bool owner = false) {
		if(_statusOwner && _status) {
			delete _status;
		}
		_status	  = v;
		_statusOwner = owner;

		if(_status == nullptr) {
			_status	  = new CacheStatus;
			_statusOwner = true;
		}
	}

  protected:
	size_t prune() {
		if(_maxSize == 0 || _cache.size() < getMaxAllowedSize()) {
			return 0;
		}
		size_t count = 0;
		while(_cache.size() > _maxSize) {
			auto& back = _keys.back();
			if(_cb) {
				_cb(back.first, back.second);
			}
			_cache.erase(back.first);
			_keys.pop_back();
			++count;
		}
		_status->incPrune(count);
		return count;
	}

  private:
	MutexType	   _mutex;
	map_type	   _cache;
	list_type	   _keys;
	size_t		   _maxSize;
	size_t		   _elasticity;
	prune_callback _cb;
	CacheStatus*   _status		 = nullptr;
	bool		   _statusOwner = false;
};

template <class K, class V, class MutexType = azzato::Mutex, class Hash = std::hash<K>>
class HashLruCache {
  public:
	typedef std::shared_ptr<HashLruCache> ptr;
	typedef LruCache<K, V, MutexType>	  cache_type;

	HashLruCache(size_t bucket, size_t max_size, size_t elasticity)
		: _bucket(bucket) {
		_datas.resize(bucket);

		size_t pre_max_size	   = std::ceil(max_size * 1.0 / bucket);
		size_t pre_elasiticity = std::ceil(elasticity * 1.0 / bucket);
		_maxSize			   = pre_max_size * bucket;
		_elasticity		   = pre_elasiticity * bucket;

		for(size_t i = 0; i < bucket; ++i) {
			_datas[i] = new cache_type(pre_max_size, pre_elasiticity, &_status);
		}
	}

	~HashLruCache() {
		for(size_t i = 0; i < _datas.size(); ++i) {
			delete _datas[i];
		}
	}

	void set(const K& k, const V& v) { _datas[_hash(k) % _bucket]->set(k, v); }

	bool get(const K& k, V& v) { return _datas[_hash(k) % _bucket]->get(k, v); }

	V get(const K& k) { return _datas[_hash(k) % _bucket]->get(k); }

	bool del(const K& k) { return _datas[_hash(k) % _bucket]->del(k); }

	bool exists(const K& k) { return _datas[_hash(k) % _bucket]->exists(k); }

	size_t size() {
		size_t total = 0;
		for(auto& i : _datas) {
			total += i->size();
		}
		return total;
	}

	bool empty() {
		for(auto& i : _datas) {
			if(!i->empty()) {
				return false;
			}
		}
		return true;
	}

	void clear() {
		for(auto& i : _datas) {
			i->clear();
		}
	}

	size_t getMaxSize() const { return _maxSize; }
	size_t getElasticity() const { return _elasticity; }
	size_t getMaxAllowedSize() const { return _maxSize + _elasticity; }
	size_t getBucket() const { return _bucket; }

	void setMaxSize(const size_t& v) {
		size_t pre_max_size = std::ceil(v * 1.0 / _bucket);
		_maxSize			= pre_max_size * _bucket;
		for(auto& i : _datas) {
			i->setMaxSize(pre_max_size);
		}
	}

	void setElasticity(const size_t& v) {
		size_t pre_elasiticity = std::ceil(v * 1.0 / _bucket);
		_elasticity		   = pre_elasiticity * _bucket;
		for(auto& i : _datas) {
			i->setElasticity(pre_elasiticity);
		}
	}

	template <class F>
	void foreach(F& f) {
		for(auto& i : _datas) {
			i->foreach(f);
		}
	}

	void setPruneCallback(typename cache_type::prune_callback cb) {
		for(auto& i : _datas) {
			i->setPruneCallback(cb);
		}
	}

	CacheStatus* getStatus() { return &_status; }

	std::string toStatusString() {
		std::stringstream ss;
		ss << _status.toString() << " total=" << size();
		return ss.str();
	}

  private:
	std::vector<cache_type*> _datas;
	size_t					 _maxSize;
	size_t					 _bucket;
	size_t					 _elasticity;
	Hash					 _hash;
	CacheStatus				 _status;
};

}  // namespace azzato

