#pragma once

#include "datastruct/util.h"
#include "mutex.h"
#include "utils/util.h"
#include <functional>
#include <iostream>
#include <memory>

namespace azzato {

template <class K, class V, class PosHash = azzato::Murmur3Hash<K>>
class HashMultimap {
  public:
	typedef std::shared_ptr<HashMultimap>					 ptr;
	typedef std::function<bool(const K& k, const V* v, int)> rcallback;
	typedef std::function<bool(const K& k, V* v, int)>		 wcallback;

	HashMultimap(const uint32_t& size = 10)
		: _total(0)
		, _elements(0) {
		_size  = basket(size);
		_datas = new std::vector<Node>[_size]();
	}

	~HashMultimap() { freeDatas(_datas, _size); }

	void rforeach(rcallback cb) {
		azzato::RWMutex::ReadLock lock(_mutex);
		for(size_t i = 0; i < _size; ++i) {
			azzato::RWMutex::ReadLock lock2(s_mutex[i % MAX_MUTEX]);
			for(auto n : _datas[i]) {
				if(!cb(n.key, n.val, n.size)) {
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
				if(!cb(n.key, n.val, n.size)) {
					return;
				}
			}
		}
	}

	SharedArray<V> get(const K& k, bool duplicate = true) {
		uint32_t				   hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock  lock(_mutex);
		uint32_t				   pos = hashvalue % _size;
		typename RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					   it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			return SharedArray<V>();
		}
		if(duplicate) {
			V* tmp = new V[it->size]();
			memcpy(tmp, it->val, sizeof(V) * it->size);
			return SharedArray<V>(it->size, tmp);
		} else {
			return SharedArray<V>(it->size, it->val, nop<V>);
		}
	}

	bool get(const K& k, std::vector<V>& v) {
		uint32_t				   hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock  lock(_mutex);
		uint32_t				   pos = hashvalue % _size;
		typename RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					   it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			return SharedArray<V>();
		}
		v.resize(it->size);
		memcpy(&v[0], it->val, sizeof(V) * it->size);
		return true;
	}

	bool get(const K& k, V& v) {
		uint32_t				   hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock  lock(_mutex);
		uint32_t				   pos = hashvalue % _size;
		typename RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					   it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			return false;
		}

		auto iit = binarySearch(it->val, it->val + it->size, v);
		if(iit != it->val + it->size) {
			v = *iit;
			return true;
		}
		return false;
	}

	bool exists(const K& k) {
		uint32_t				   hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock  lock(_mutex);
		uint32_t				   pos = hashvalue % _size;
		typename RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					   it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		return it != _datas[pos].end();
	}

	bool exists(const K& k, const V& v) {
		uint32_t				   hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock  lock(_mutex);
		uint32_t				   pos = hashvalue % _size;
		typename RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					   it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			return false;
		}

		auto iit = binarySearch(it->val, it->val + it->size, v);
		return iit != it->val + it->size;
	}

	bool insert(const K& k, const V& v) {
		if(needRehash()) {
			rehash();
		}

		uint32_t					hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock	lock(_mutex);
		uint32_t					pos = hashvalue % _size;
		typename RWMutex::WriteLock lock2(s_mutex[pos % MAX_MUTEX]);

		auto it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			Node node(k);
			node.size	= 1;
			node.val	= new V[node.size]();
			node.val[0] = v;

			_datas[pos].push_back(node);
			sortLast(&_datas[pos][0], _datas[pos].size());
			// std::sort(_datas[pos].begin(), _datas[pos].end());

			azzato::Atomic::addFetch(_total);
			azzato::Atomic::addFetch(_elements);
		} else {
			auto iit = binarySearch(it->val, it->val + it->size, v);
			if(iit != it->val + it->size) {
				*iit = v;
				return true;
			}
			V* datas = new V[it->size + 1]();
			memcpy(datas, it->val, it->size * sizeof(V));
			datas[it->size] = v;
			++it->size;
			sortLast(datas, it->size);
			// std::sort(datas, datas + it->size);
			if(!inValues(it->val)) {
				delete[] it->val;
			}
			it->val = datas;
			azzato::Atomic::addFetch(_elements);
		}
		return true;
	}

	bool insert(const K& k, const V* v, const uint32_t& size) {
		if(needRehash()) {
			rehash();
		}

		uint32_t					hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock	lock(_mutex);
		uint32_t					pos = hashvalue % _size;
		typename RWMutex::WriteLock lock2(s_mutex[pos % MAX_MUTEX]);

		auto it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			Node node(k);
			node.size = size;
			node.val  = new V[node.size]();
			memcpy(node.val, v, size * sizeof(V));
			std::sort(node.val, node.val + size);

			_datas[pos].push_back(node);
			sortLast(&_datas[pos][0], _datas[pos].size());
			// std::sort(_datas[pos].begin(), _datas[pos].end());

			azzato::Atomic::addFetch(_total);
			azzato::Atomic::addFetch(_elements, (uint64_t)size);
		} else {
			std::set<V> news;
			for(size_t i = 0; i < size; ++i) {
				auto iit = binarySearch(it->val, it->val + it->size, v[i]);
				if(iit == it->val + it->size) {
					news.insert(v[i]);
				} else {
					*iit = v[i];
				}
			}

			V* datas = new V[it->size + news.size()]();
			memcpy(datas, it->val, it->size * sizeof(V));
			int n = 0;
			for(auto& i : news) {
				datas[it->size + n] = i;
				++n;
			}
			it->size += news.size();
			std::sort(datas, datas + it->size);
			if(!inValues(it->val)) {
				delete[] it->val;
			}
			it->val = datas;
			azzato::Atomic::addFetch(_elements, news.size());
		}
		return true;
	}

	bool set(const K& k, const V* v, const uint32_t& size) {
		if(needRehash()) {
			rehash();
		}

		uint32_t					hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock	lock(_mutex);
		uint32_t					pos = hashvalue % _size;
		typename RWMutex::WriteLock lock2(s_mutex[pos % MAX_MUTEX]);

		auto it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			Node node(k);
			node.size = size;
			node.val  = new V[node.size]();
			memcpy(node.val, v, size * sizeof(V));
			std::sort(node.val, node.val + size);

			_datas[pos].push_back(node);
			sortLast(&_datas[pos][0], _datas[pos].size());
			// std::sort(_datas[pos].begin(), _datas[pos].end());

			azzato::Atomic::addFetch(_total);
			azzato::Atomic::addFetch(_elements, (uint64_t)size);
		} else {
			V* datas = new V[size]();
			memcpy(datas, v, size * sizeof(V));
			it->size = size;
			std::sort(datas, datas + it->size);
			if(!inValues(it->val)) {
				delete[] it->val;
			}
			it->val = datas;
			azzato::Atomic::addFetch(_elements, (uint64_t)size);
		}
		return true;
	}

	bool del(const K& k) {
		uint32_t					hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock	lock(_mutex);
		uint32_t					pos = hashvalue % _size;
		typename RWMutex::WriteLock lock2(s_mutex[pos % MAX_MUTEX]);

		auto it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			return false;
		} else {
			if(!inValues(it->val)) {
				delete[] it->val;
			}
			azzato::Atomic::subFetch(_elements, (uint64_t)it->size);
			if(_datas[pos].size() > 1) {
				std::swap(*it, _datas[pos].back());
			}
			_datas[pos].resize(_datas[pos].size() - 1);
			std::sort(_datas[pos].begin(), _datas[pos].end());
			azzato::Atomic::subFetch(_total);
		}
		return true;
	}

	bool del(const K& k, const V& v) {
		uint32_t					hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock	lock(_mutex);
		uint32_t					pos = hashvalue % _size;
		typename RWMutex::WriteLock lock2(s_mutex[pos % MAX_MUTEX]);

		auto it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		if(it == _datas[pos].end()) {
			return false;
		} else {
			auto iit = binarySearch(it->val, it->val + it->size, v);
			if(iit == it->val + it->size) {
				return false;
			}

			if(it->size == 1) {
				if(!inValues(it->val)) {
					delete[] it->val;
				}
				it->val	 = nullptr;
				it->size = 0;

				if(_datas[pos].size() > 1) {
					std::swap(*it, _datas[pos].back());
				}
				_datas[pos].resize(_datas[pos].size() - 1);
				std::sort(_datas[pos].begin(), _datas[pos].end());

				azzato::Atomic::subFetch(_total);
				azzato::Atomic::subFetch(_elements);
				return true;
			}

			V* datas = new V[it->size - 1]();
			memcpy(datas, it->val, (iit - it->val) * sizeof(V));
			memcpy(datas, it + 1, (it->size - (iit - it->val) - 1) * sizeof(V));

			if(!inValues(it->val)) {
				delete[] it->val;
			}
			it->val = datas;
			--it->size;
		}
		return true;
	}

	void rehash() {
		azzato::RWMutex::WriteLock lock(_mutex);
		if(needRehash()) {
			rehashUnlock();
		}
	}

	uint64_t getTotal() const { return _total; }

	uint64_t getElements() const { return _elements; }

	std::ostream& dump(std::ostream& os) {
		typename RWMutex::ReadLock lock(_mutex);
		os << "[HashMultimap total=" << _total << " elements=" << _elements << " bucket=" << _size
		   << " rate=" << getRate() << "]" << std::endl;
		return os;
	}

	// for K,V is POD
	bool writeTo(std::ostream& os, uint64_t speed = -1) {
		azzato::RWMutex::ReadLock lock(_mutex);
		os.write((const char*)&_size, sizeof(_size));

		std::vector<V>	  vs;
		std::vector<Node> ns;

		ns.reserve(_total);
		vs.reserve(_total);

		for(size_t i = 0; i < _size; ++i) {
			typename RWMutex::ReadLock lock(s_mutex[i % MAX_MUTEX]);
			for(auto n : _datas[i]) {
				if(!n.size) {
					continue;
				}
				size_t offset = vs.size();
				vs.insert(vs.end(), n.val, n.val + n.size);
				n.val = (V*)offset;
				ns.push_back(n);
			}
		}

		uint64_t size = vs.size() * sizeof(V);
		os.write((const char*)&size, sizeof(size));
		if(speed == (uint64_t)-1) {
			os.write((const char*)&vs[0], size);
		} else {
			azzato::writeFixToStreamWithSpeed(os, (const char*)&vs[0], size, speed);
		}

		size = ns.size() * sizeof(Node);
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
				std::vector<V>&	  vs = _values;
				std::vector<Node> ns;

				uint64_t size;
				if(!readFromStream(is, size)) {
					break;
				}
				vs.resize(size / sizeof(V));
				if(speed == (uint64_t)-1) {
					if(!readFixFromStream(is, (char*)&vs[0], size)) {
						break;
					}
				} else {
					if(!readFixFromStreamWithSpeed(is, (char*)&vs[0], size, speed)) {
						break;
					}
				}
				if(!readFromStream(is, size)) {
					break;
				}
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
				_total	  = size / sizeof(Node);

				_elements = 0;
				_datas	  = new std::vector<Node>[_size]();
				for(auto& n : ns) {
					n.val = &vs[0] + (uint64_t)n.val;
					_datas[_posHash(n.key) % _size].push_back(n);
					_elements += n.size;
				}
				for(size_t i = 0; i < _size; ++i) {
					std::sort(_datas[i].begin(), _datas[i].end());
				}
				return true;
			} catch(...) {
				return false;
			}
		} while(0);
		return false;
	}

  private:
	struct Node {
		K	key;
		int size;
		V*	val;

		Node(const K& k = K())
			: key(k)
			, size(0)
			, val(nullptr) {}

		bool operator<(const Node& o) const { return key < o.key; }
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

	bool inValues(V* ptr) const {
		if(!ptr) {
			return true;
		}
		if(_values.empty()) {
			return false;
		}
		return ptr >= &_values.front() && ptr <= &_values.back();
	}

	void freeDatas(std::vector<Node>*& datas, uint64_t size) {
		if(!datas) {
			return;
		}
		for(size_t i = 0; i < size; ++i) {
			for(auto& n : datas[i]) {
				if(n.size && !inValues(n.val)) {
					delete[] n.val;
				}
			}
		}
		delete[] datas;
		datas = nullptr;
	}

	float getRate() const { return _total * 1.0 / _size; }

	bool needRehash() const { return getRate() > 16; }

  private:
	uint64_t		   _size;
	uint64_t		   _total;
	uint64_t		   _elements;
	std::vector<Node>* _datas;
	azzato::RWMutex	   _mutex;
	PosHash			   _posHash;

	std::vector<V> _values;

	static const uint32_t  MAX_MUTEX = 1024 * 128;
	static azzato::RWMutex s_mutex[MAX_MUTEX];
};

template <class K, class V, class PosHash>
azzato::RWMutex HashMultimap<K, V, PosHash>::s_mutex[MAX_MUTEX];

}  // namespace azzato
