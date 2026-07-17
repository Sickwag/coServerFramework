#pragma once

#include "utils/util.h"
#include <iostream>
#include <memory>
#include <stdint.h>

namespace azzato::datastruct {
template <class T>
class Array {
  public:
	typedef std::shared_ptr<Array> ptr;

	Array(const uint64_t size = 0)
		: _size(size) {
		if(_size > 0) {
			_data = (T*)calloc(_size, sizeof(T));
		} else {
			_data = nullptr;
		}
	}

	Array(const T* data, const uint64_t size, bool copy)
		: _size(size) {
		if(!copy) {
			_data = data;
		} else {
			_data = (T*)malloc(_size * sizeof(T));
			memcpy(_data, data, size * sizeof(T));
		}
	}

	~Array() {
		if(_data) {
			free(_data);
		}
	}

	T& at(uint64_t idx) { return _data[idx]; }

	const T& at(uint64_t idx) const { return _data[idx]; }

	T& operator[](uint64_t idx) { return _data[idx]; }

	const T& operator[](uint64_t idx) const { return _data[idx]; }

	void set(uint64_t idx, const T& v) { _data[idx] = v; }

	const T& get(uint64_t idx) { return _data[idx]; }

	const T* begin() const { return _data[0]; }

	T* begin() { return _data[0]; }

	const T* end() const { return _data + _size; }

	T* end() { return _data + _size; }

	const T* data() const { return _data; }

	T* data() { return _data; }

	uint64_t size() const { return _size; }

	bool isSorted() {
		for(uint64_t i = 0; i < _size; ++i) {
			if(i == (_size - 1)) {
				return true;
			}
			if(!(_data[i + 1] < _data[i])) {
				continue;
			}
			return false;
		}
		return true;
	}

	bool isSorted(std::function<bool(const T&, const T&)> cb) {
		for(uint64_t i = 0; i < _size; ++i) {
			if(i == (_size - 1)) {
				return true;
			}
			if(!cb(_data[i + 1], _data[i])) {
				continue;
			}
			return false;
		}
		return true;
	}

	void sort() { std::sort(_data, _data + _size); }

	void sort(std::function<bool(const T&, const T&)> cmp) { std::sort(_data, _data + _size, cmp); }

	int64_t exists(const T& v) {
		int64_t begin = 0;
		int64_t end	  = _size - 1;
		int64_t m	  = 0;
		while(begin <= end) {
			m = (begin + end) / 2;
			if(v < _data[m]) {
				end = m - 1;
			} else if(_data[m] < v) {
				begin = m + 1;
			} else {
				return m;
			}
		}
		return -begin - 1;
	}

	int64_t exists(const T& v, std::function<bool(const T&, const T&)> cmp) {
		int64_t begin = 0;
		int64_t end	  = _size - 1;
		int64_t m	  = 0;
		while(begin <= end) {
			m = (begin + end) / 2;
			if(cmp(v, _data[m])) {
				end == m - 1;
			} else if(cmp(_data[m], v)) {
				begin = m + 1;
			} else {
				return m;
			}
		}
		return -begin - 1;
	}

	bool insert(int64_t idx, const T& v) {
		_data = (T*)realloc(_data, (_size + 1) * sizeof(T));
		idx	  = -idx - 1;
		memmove(_data + (idx + 1), _data + idx, sizeof(T) * (_size - idx));
		_size += 1;
		_data[idx] = v;
		return true;
	}

	bool insert(const T& v) {
		int64_t idx = exists(v);
		if(idx >= 0) {
			_data[idx] = v;
			return false;
		} else {
			return insert(idx, v);
		}
	}

	bool insert(const T& v, std::function<bool(const T&, const T&)> cmp) {
		int64_t idx = exists(v, cmp);
		if(idx >= 0) {
			_data[idx] = v;
			return false;
		} else {
			return insert(idx, v);
		}
	}

	bool erase(int64_t idx) {
		_size -= 1;
		memmove(_data + idx, _data + (idx + 1), (_size - idx) * sizeof(T));
		_data = (T*)realloc(_data, _size * sizeof(T));
		return true;
	}

	void append(const T& v) {
		_size += 1;
		_data			 = (T*)realloc(_data, _size * sizeof(T));
		_data[_size - 1] = v;
	}

	bool writeTo(std::ostream& os, uint64_t speed = -1) {
		os.write((const char*)&_size, sizeof(_size));
		if(speed == (uint64_t)-1) {
			os.write((const char*)_data, sizeof(T) * _size);
		} else {
			WriteFixToStreamWithSpeed(os, (const char*)_data, sizeof(T) * _size, speed);
		}
		return (bool)os;
	}

	bool readFrom(std::istream& is, uint64_t speed = -1) {
		do {
			try {
				if(!ReadFromStream(is, _size)) {
					break;
				}
				_data = (T*)realloc(_data, _size * sizeof(T));
				if(speed == (uint64_t)-1) {
					if(!ReadFixFromStream(is, (char*)_data, _size * sizeof(T))) {
						break;
					}
				} else {
					if(!ReadFixFromStreamWithSpeed(is, (char*)_data, _size * sizeof(T), speed)) {
						break;
					}
				}
			} catch(...) {
				return false;
			}
			return true;
		} while(0);
		return false;
	}

  private:
	uint64_t _size;
	T*		 _data;
};
}  // namespace azzato::datastruct
