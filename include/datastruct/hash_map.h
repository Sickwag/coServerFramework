#pragma once

#include "datastruct/util.h"
#include "log.h"
#include "mutex.h"
#include "utils/util.h"
#include <functional>
#include <iostream>
#include <memory>

namespace azzato {

template <class K, class V, class PosHash = azzato::Murmur3Hash<K>>
class HashMap {
  public:
	typedef std::shared_ptr<HashMap> ptr;
	typedef Pair<K, V>				 value_type;

	typedef std::function<bool(const K& k, const V& v)> rcallback;
	typedef std::function<bool(const K& k, V& v)>		wcallback;

	HashMap(const uint32_t& size = 0)
		: _total(0) {
		_size  = basket(size);
		_datas = new std::vector<Node>[_size]();
	}

	~HashMap() { freeDatas(_datas, _size); }

	void rforeach(rcallback cb) {
		azzato::RWMutex::ReadLock lock(_mutex);
		for(size_t i = 0; i < _size; ++i) {
			azzato::RWMutex::ReadLock lock2(s_mutex[i % MAX_MUTEX]);
			for(auto n : _datas[i]) {
				if(!cb(n.key, n.val)) {
					return;
				}
			}
		}
	}

	void wforeach(wcallback cb) {
		azzato::RWMutex::ReadLock lock(_mutex);
		for(size_t i = 0; i < _size; ++i) {
			azzato::RWMutex::ReadLock lock2(s_mutex[i % MAX_MUTEX]);
			for(auto n : _datas[i]) {
				if(!cb(n.key, n.val)) {
					return;
				}
			}
		}
	}

	bool get(const K& k, V& v) {
		uint32_t				  hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock lock(_mutex);
		uint32_t				  pos = hashvalue % _size;
		azzato::RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					  it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		// AZZATO_ASSERT(it == std::find(_datas[pos].begin(), _datas[pos].end(), Node(k)));
		if(it == _datas[pos].end()) {
			return false;
		}
		v = it->val;
		return true;
	}

	bool exists(const K& k) {
		uint32_t				  hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock lock(_mutex);
		uint32_t				  pos = hashvalue % _size;
		azzato::RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					  it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		// AZZATO_ASSERT(it == std::find(_datas[pos].begin(), _datas[pos].end(), Node(k)));
		return it != _datas[pos].end();
	}

	bool set(const K& k, const V& v) {
		if(needRehash()) {
			rehash();
		}

		uint32_t				   hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock  lock(_mutex);
		uint32_t				   pos = hashvalue % _size;
		azzato::RWMutex::WriteLock lock2(s_mutex[pos % MAX_MUTEX]);

		auto it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		// AZZATO_ASSERT(it == std::find(_datas[pos].begin(), _datas[pos].end(), Node(k)));
		if(it == _datas[pos].end()) {
			Node node(k);
			node.val = v;

			_datas[pos].push_back(node);
			sortLast(&_datas[pos][0], _datas[pos].size());
			// std::sort(_datas[pos].begin(), _datas[pos].end());
			azzato::Atomic::addFetch(_total);
			return true;
		} else {
			it->val = v;
			return false;
		}
	}

	uint64_t getTotal() const { return _total; }

	bool del(const K& k) {
		uint32_t				   hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock  lock(_mutex);
		uint32_t				   pos = hashvalue % _size;
		azzato::RWMutex::WriteLock lock2(s_mutex[pos % MAX_MUTEX]);

		auto it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		// AZZATO_ASSERT(it == std::find(_datas[pos].begin(), _datas[pos].end(), Node(k)));
		if(it == _datas[pos].end()) {
			return false;
		} else {
			if(_datas[pos].size() > 1) {
				std::swap(*it, _datas[pos].back());
			}
			_datas[pos].resize(_datas[pos].size() - 1);
			std::sort(_datas[pos].begin(), _datas[pos].end());
			azzato::Atomic::subFetch(_total);
		}
		return true;
	}

	void clear() {
		azzato::RWMutex::WriteLock lock(_mutex);
		_total = 0;
		_size  = 0;
		freeDatas(_datas, _size);
	}

	void swap(HashMap& oth) {
		azzato::RWMutex::WriteLock lock(_mutex);
		azzato::RWMutex::WriteLock lock2(oth._mutex);
		std::swap(_total, oth._total);
		std::swap(_size, oth._size);
		std::swap(_datas, oth._datas);
	}

	void merge(HashMap& oth) {
		std::vector<std::pair<K, V>> tmp;
		tmp.reserve(oth.getTotal());
		oth.rforeach([&tmp](const K& k, const V& v) {
			tmp.push_back(std::make_pair(k, v));
			return true;
		});

		for(auto& i : tmp) {
			set(i.first, i.second);
		}
	}

	void rehash() {
		azzato::RWMutex::WriteLock lock(_mutex);
		if(needRehash()) {
			rehashUnlock();
		}
	}

	std::ostream& dump(std::ostream& os) {
		typename RWMutex::ReadLock lock(_mutex);
		os << "[HashMap total=" << _total << " bucket=" << _size << " rate=" << getRate() << "]" << std::endl;
		return os;
	}

	// for K,V is POD
	bool writeTo(std::ostream& os, uint64_t speed = -1) {
		azzato::RWMutex::ReadLock lock(_mutex);
		os.write((const char*)&_size, sizeof(_size));

		std::vector<Node> ns;

		ns.reserve(_total);
		for(size_t i = 0; i < _size; ++i) {
			azzato::RWMutex::ReadLock lock2(s_mutex[i % MAX_MUTEX]);
			for(auto n : _datas[i]) {
				ns.push_back(n);
			}
		}

		size_t size = ns.size() * sizeof(Node);
		os.write((const char*)&size, sizeof(size));
		if(speed == (uint64_t)-1) {
			os.write((const char*)&ns[0], size);
		} else {
			azzato::writeFixToStreamWithSpeed(os, (const char*)&ns[0], size, speed);
		}

		// std::cout << "writeTo size: " << os.tellp() << std::endl;
		return (bool)os;
	}

	bool readFrom(std::istream& is, uint64_t speed = -1) {
		do {
			try {
				freeDatas(_datas, _size);
				if(!readFromStream(is, _size)) {
					break;
				}
				// LOG_INFO() << "_size: " << _size;
				std::vector<Node> ns;

				uint64_t size;
				if(!readFromStream(is, size)) {
					// LOG_ERROR() << "error";
					break;
				}
				// LOG_INFO() << "ns_size: " << size;
				ns.resize(size / sizeof(Node));
				if(speed == (uint64_t)-1) {
					if(!readFixFromStream(is, (char*)&ns[0], size)) {
						break;
					}
				} else {
					if(!readFixFromStreamWithSpeed(is, (char*)&ns[0], size, speed)) {
						break;
					}
				}
				// LOG_INFO() << size << ": " << (size / sizeof(Node)) << " : " << ns.size();
				_total = size / sizeof(Node);

				_datas = new std::vector<Node>[_size]();
				for(auto& n : ns) {
					_datas[_posHash(n.key) % _size].push_back(n);
				}
				for(size_t i = 0; i < _size; ++i) {
					std::sort(_datas[i].begin(), _datas[i].end());
				}
				return true;
			} catch(...) {
				// LOG_ERROR() << "error";
				return false;
			}
		} while(0);
		return false;
	}

  private:
	struct Node {
		K key;
		V val;

		Node(const K& k = K())
			: key(k)
			, val() {}

		bool operator<(const Node& o) const { return key < o.key; }

		bool operator==(const Node& o) const { return key == o.key; }
	};

	void rehashUnlock() {
		uint64_t size = basket(_total);
		if(size == _size) {
			return;
		}
		std::vector<Node>* datas = new std::vector<Node>[size]();
		for(size_t i = 0; i < _size; ++i) {
			for(auto& n : _datas[i]) {
				datas[_posHash(n.key) % size].push_back(n);
			}
		}
		for(size_t i = 0; i < size; ++i) {
			std::sort(datas[i].begin(), datas[i].end());
		}

		delete[] _datas;
		// freeDatas(_datas, _size);

		_size  = size;
		_datas = datas;
	}

	void freeDatas(std::vector<Node>*& datas, uint64_t size) {
		if(!datas) {
			return;
		}
		delete[] datas;
		datas = nullptr;
	}

	float getRate() const { return _total * 1.0 / _size; }

	bool needRehash() const { return getRate() > 16; }

  private:
	uint64_t		   _size;
	uint64_t		   _total;
	std::vector<Node>* _datas;
	azzato::RWMutex	   _mutex;
	PosHash			   _posHash;

	static const uint32_t  MAX_MUTEX = 1024 * 128;
	static azzato::RWMutex s_mutex[MAX_MUTEX];
};

template <class K, class V, class PosHash>
azzato::RWMutex HashMap<K, V, PosHash>::s_mutex[MAX_MUTEX];

}  // namespace azzato
