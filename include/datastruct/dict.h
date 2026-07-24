#pragma once

#include "datastruct/util.h"
#include "log.h"
#include "mutex.h"
#include "utils/util.h"
#include <functional>
#include <iostream>
#include <memory>

namespace azzato {

class StringDict;

template <class K, class V, class PosHash = azzato::Murmur3Hash<K>>
class Dict {
	friend class StringDict;

  public:
	typedef std::shared_ptr<Dict>									 ptr;
	typedef std::function<bool(const K& k, const V* v, size_t size)> callback;

	Dict(const uint32_t& size = 0)
		: _total(0) {
		_size  = basket(size);
		_datas = new std::vector<Node>[_size]();
	}

	~Dict() { freeDatas(_datas, _size); }

	SharedArray<V> get(const K& k, bool duplicate = true) {
		uint32_t				  hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock lock(_mutex);
		uint32_t				  pos = hashvalue % _size;
		azzato::RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					  it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		// AZZATO_ASSERT(it == std::find(_datas[pos].begin(), _datas[pos].end(), Node(k)));
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
		uint32_t				  hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock lock(_mutex);
		uint32_t				  pos = hashvalue % _size;
		azzato::RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					  it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		// AZZATO_ASSERT(it == std::find(_datas[pos].begin(), _datas[pos].end(), Node(k)));
		if(it == _datas[pos].end()) {
			return false;
		}
		v.resize(it->size);
		memcpy(&v[0], it->val, sizeof(V) * it->size);
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

	bool insert(const K& k, const V* v, const uint32_t& size) {
		if(size == 0) {
			return true;
		}

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
			node.size = size;
			node.val  = new V[node.size]();
			memcpy(node.val, v, size * sizeof(V));

			_datas[pos].push_back(node);
			sortLast(&_datas[pos][0], _datas[pos].size());
			// std::sort(_datas[pos].begin(), _datas[pos].end());

			azzato::Atomic::addFetch(_total);
		} else {
			if(it->size == (int)size) {
				memcpy(it->val, v, sizeof(V) * size);
			} else {
				V* datas = new V[size]();
				memcpy(datas, v, size * sizeof(V));
				it->size = size;
				if(!inValues(it->val)) {
					delete[] it->val;
				}
				it->val = datas;
			}
		}
		return true;
	}

	void foreach(callback cb) {
		azzato::RWMutex::ReadLock lock(_mutex);
		for(size_t i = 0; i < _size; ++i) {
			azzato::RWMutex::ReadLock lock2(s_mutex[i % MAX_MUTEX]);
			for(auto n : _datas[i]) {
				if(!cb(n.key, n.val, n.size)) {
					break;
				}
			}
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
			if(!inValues(it->val)) {
				delete[] it->val;
			}
			if(_datas[pos].size() > 1) {
				std::swap(*it, _datas[pos].back());
			}
			_datas[pos].resize(_datas[pos].size() - 1);
			std::sort(_datas[pos].begin(), _datas[pos].end());
			azzato::Atomic::subFetch(_total);
		}
		return true;
	}

	void rehash() {
		azzato::RWMutex::WriteLock lock(_mutex);
		if(needRehash()) {
			rehashUnlock();
		}
	}

	std::ostream& dump(std::ostream& os) {
		typename RWMutex::ReadLock lock(_mutex);
		os << "[Dict total=" << _total << " bucket=" << _size << " rate=" << getRate() << "]" << std::endl;
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
			azzato::RWMutex::ReadLock lock2(s_mutex[i % MAX_MUTEX]);
			for(auto n : _datas[i]) {
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
				// LOG_INFO() << "_size: " << _size;
				std::vector<V>&	  vs = _values;
				std::vector<Node> ns;

				uint64_t size;
				if(!readFromStream(is, size)) {
					break;
				}
				// LOG_INFO() << "vs_size: " << size;
				vs.resize(size / sizeof(V));
				if(speed == (uint64_t)-1) {
					if(!readFixFromStream(is, (char*)&vs[0], size)) {
						// LOG_ERROR() << "error";
						break;
					}
				} else {
					if(!readFixFromStreamWithSpeed(is, (char*)&vs[0], size, speed)) {
						// LOG_ERROR() << "error";
						break;
					}
				}
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
					n.val = &vs[0] + (uint64_t)n.val;
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
	std::string getString(const K& k) {
		uint32_t				  hashvalue = _posHash(k);
		azzato::RWMutex::ReadLock lock(_mutex);
		uint32_t				  pos = hashvalue % _size;
		azzato::RWMutex::ReadLock lock2(s_mutex[pos % MAX_MUTEX]);
		auto					  it = binarySearch(_datas[pos].begin(), _datas[pos].end(), Node(k));
		// AZZATO_ASSERT(it == std::find(_datas[pos].begin(), _datas[pos].end(), Node(k)));
		if(it == _datas[pos].end()) {
			return std::string();
		}
		return std::string(it->val, it->size);
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
				if(!inValues(n.val)) {
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
	std::vector<Node>* _datas;
	azzato::RWMutex	   _mutex;
	PosHash			   _posHash;

	std::vector<V> _values;

	static const uint32_t  MAX_MUTEX = 1024 * 128;
	static azzato::RWMutex s_mutex[MAX_MUTEX];
};

template <class K, class V, class PosHash>
azzato::RWMutex Dict<K, V, PosHash>::s_mutex[MAX_MUTEX];

class StringDict {
  public:
	static uint64_t GetID(const std::string& str) {
		if(str.empty()) {
			return 0;
		}
		return azzato::murmur3_hash64(str.c_str(), str.size(), 1060627423, 1050126127);
	}

	static uint64_t GetID(const char* str, const uint32_t& size) {
		if(size == 0) {
			return 0;
		}
		return azzato::murmur3_hash64(str, size, 1060627423, 1050126127);
	}

	static uint64_t GetID(const char* str) {
		if(str == nullptr) {
			return 0;
		}
		return azzato::murmur3_hash64(str, 1060627423, 1050126127);
	}

	uint64_t update(const std::string& str) { return update(str.c_str(), str.size()); }

	uint64_t update(const char* str, const uint32_t& size) {
		uint64_t id = GetID(str, size);
		if(id == 0) {
			return 0;
		}
		_dict.insert(id, str, size);
		return id;
	}

	uint64_t update(const char* str) {
		uint64_t id = GetID(str);
		if(id == 0) {
			return 0;
		}
		_dict.insert(id, str, strlen(str));
		return id;
	}

	std::string get(const uint64_t& id) { return _dict.getString(id); }

	SharedArray<char> getRaw(const uint64_t& id, bool duplicate = true) { return _dict.get(id, duplicate); }

	bool readFrom(std::istream& is, uint64_t speed = -1) { return _dict.readFrom(is, speed); }

	bool writeTo(std::ostream& os, uint64_t speed = -1) { return _dict.writeTo(os, speed); }

	std::ostream& dump(std::ostream& os) { return _dict.dump(os); }

	void foreach(std::function<bool(const uint64_t& k, const char* v, size_t size)> cb) { _dict.foreach(cb); }

	uint64_t getTotal() { return _dict.getTotal(); }

  private:
	Dict<uint64_t, char> _dict;
};

}  // namespace azzato
