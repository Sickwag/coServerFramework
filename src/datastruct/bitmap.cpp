#include "datastruct/bitmap.h"
#include "log.h"
#include "utils/macro.h"
#include <iostream>
#include <math.h>
#include <sstream>
#include <string.h>

namespace azzato {

// static uint8_t HEAD[] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};
// static uint8_t TAIL[] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};
// static uint8_t POS[] = {0x01, 0x02, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};

Bitmap::base_type Bitmap::POS[sizeof(base_type) * 8];
Bitmap::base_type Bitmap::NPOS[sizeof(base_type) * 8];
Bitmap::base_type Bitmap::MASK[sizeof(base_type) * 8];

bool Bitmap::init() {
	for(size_t i = 0; i < (sizeof(base_type) * 8); ++i) {
		POS[i]	= ((base_type)1) << i;
		NPOS[i] = ~POS[i];
		MASK[i] = POS[i] - 1;
	}
	return true;
}

static bool s_init = Bitmap::init();

Bitmap::Bitmap(uint32_t size, uint8_t def)
	: _compress(false)
	, _size(size)
	, _dataSize(ceil(size * 1.0 / VALUE_SIZE))
	, _data(NULL) {
	if(_dataSize) {
		_data = (base_type*)malloc(_dataSize * sizeof(base_type));
		memset(_data, def, _dataSize * sizeof(base_type));
	}
}

Bitmap::Bitmap()
	: _compress(true)
	, _size(0)
	, _dataSize(0)
	, _data(NULL) {}

Bitmap::Bitmap(const Bitmap& b) {
	_compress = b._compress;
	_size	  = b._size;
	_dataSize = b._dataSize;
	_data	  = (base_type*)malloc(_dataSize * sizeof(base_type));
	memcpy(_data, b._data, _dataSize * sizeof(base_type));
}

Bitmap::~Bitmap() {
	if(_data) {
		free(_data);
	}
}

void Bitmap::writeTo(azzato::ByteArray::ptr ba) const {
	ba->write<uint8_t>(_compress);
	ba->write<uint32_t>(_size);
	ba->write<uint32_t>(_dataSize);
	ba->write((const char*)_data, _dataSize * sizeof(base_type));
}

bool Bitmap::readFrom(azzato::ByteArray::ptr ba) {
	try {
		_compress = ba->read<uint8_t>();
		_size	  = ba->read<uint32_t>();
		_dataSize = ba->read<uint32_t>();
		if(_data) {
			free(_data);
		}
		_data = (base_type*)malloc(_dataSize * sizeof(base_type));
		ba->read((char*)_data, _dataSize * sizeof(base_type));
		return true;
	} catch(...) {
	}
	return false;
}

Bitmap& Bitmap::operator=(const Bitmap& b) {
	if(this == &b) {
		return *this;
	}
	_compress = b._compress;
	_size	  = b._size;
	_dataSize = b._dataSize;
	if(_data) {
		free(_data);
	}
	_data = (base_type*)malloc(_dataSize * sizeof(base_type));
	memcpy(_data, b._data, _dataSize * sizeof(base_type));
	return *this;
}

Bitmap& Bitmap::operator&=(const Bitmap& b) {
	if(_size != b._size) {
		throw std::logic_error("_size != b._size");
	}

	if(!_compress && !b._compress) {
		uint32_t max_size = _size / U64_VALUE_SIZE;
		for(uint32_t i = 0; i < max_size; ++i) {
			((uint64_t*)_data)[i] &= ((uint64_t*)b._data)[i];
		}
		for(uint32_t i = max_size * U64_DIV_BASE; i < _dataSize; ++i) {
			_data[i] &= b._data[i];
		}
	} else if(_compress != b._compress) {
		if(_compress) {
			throw std::logic_error("compress &= uncompress not support");
		}
		uint32_t cur_pos = 0;
		for(uint32_t i = 0; i < b._dataSize; ++i) {
			base_type cur = b._data[i];
			if(cur & COMPRESS_MASK) {
				uint32_t count = cur & COUNT_MASK;
				bool	 v	   = cur & VALUE_MASK;
				uint32_t tmp_i = i;
				for(uint32_t n = i + 1; n < b._dataSize; ++n) {
					if((b._data[n] & COMPRESS_MASK) && (v == (bool)(b._data[n] & VALUE_MASK))) {
						count = (((uint32_t)(b._data[n] & COUNT_MASK)) << (VALUE_SIZE * (n - tmp_i))) | count;
						++i;
					} else {
						break;
					}
				}
				if(!v) {
					set(cur_pos, count, 0);
					cur_pos += count;
				} else {
					cur_pos += count;
				}
			} else {
				base_type count = cur & COUNT_MASK;
				_data[cur_pos / VALUE_SIZE] &= count;
				cur_pos += VALUE_SIZE;
			}
		}
	} else {
		throw std::logic_error("compress &= compress not support");
	}
	return *this;
}

Bitmap& Bitmap::operator~() {
	if(!_compress) {
		uint32_t max_size = _size / U64_VALUE_SIZE;
		for(uint32_t i = 0; i < max_size; ++i) {
			((uint64_t*)_data)[i] = ~(((uint64_t*)_data)[i]);
		}
		for(uint32_t i = max_size * U64_DIV_BASE; i < _dataSize; ++i) {
			_data[i] = ~(_data[i]);
		}
		return *this;
	} else {
		for(uint32_t i = 0; i < _dataSize; ++i) {
			base_type cur = _data[i];
			if(cur & COMPRESS_MASK) {
				if(cur & VALUE_MASK) {
					_data[i] = cur & NOT_VALUE_MASK;
				} else {
					_data[i] = cur | VALUE_MASK;
				}
			} else {
				_data[i] = (~cur) & COUNT_MASK;
			}
		}
		return *this;
	}
}

Bitmap& Bitmap::operator|=(const Bitmap& b) {
	if(_size != b._size) {
		throw std::logic_error("_size != b._size");
	}

	if(!_compress && !b._compress) {
		uint32_t max_size = _size / U64_VALUE_SIZE;
		for(uint32_t i = 0; i < max_size; ++i) {
			((uint64_t*)_data)[i] |= ((uint64_t*)b._data)[i];
		}
		for(uint32_t i = max_size * U64_DIV_BASE; i < _dataSize; ++i) {
			_data[i] |= b._data[i];
		}
	} else if(_compress != b._compress) {
		if(_compress) {
			throw std::logic_error("compress |= uncompress not support");
		}
		uint32_t cur_pos = 0;
		for(uint32_t i = 0; i < b._dataSize; ++i) {
			base_type cur = b._data[i];
			if(cur & COMPRESS_MASK) {
				uint32_t count = cur & COUNT_MASK;
				bool	 v	   = cur & VALUE_MASK;
				uint32_t tmp_i = i;
				for(uint32_t n = i + 1; n < b._dataSize; ++n) {
					if((b._data[n] & COMPRESS_MASK) && (v == (bool)(b._data[n] & VALUE_MASK))) {
						count = (((uint32_t)(b._data[n] & COUNT_MASK)) << (VALUE_SIZE * (n - tmp_i))) | count;
						++i;
					} else {
						break;
					}
				}
				if(v) {
					set(cur_pos, count, 1);
					cur_pos += count;
				} else {
					cur_pos += count;
				}
			} else {
				base_type count = cur & COUNT_MASK;
				_data[cur_pos / VALUE_SIZE] |= count;
				cur_pos += VALUE_SIZE;
			}
		}
	} else {
		throw std::logic_error("compress |= compress not support");
	}
	return *this;
}

bool Bitmap::operator==(const Bitmap& b) const {
	if(this == &b) {
		return true;
	}
	if(_compress != b._compress || _size != b._size || _dataSize != b._dataSize) {
		return false;
	}
	if(memcmp(_data, b._data, _dataSize * sizeof(base_type))) {
		return false;
	}
	uint32_t left = _size % VALUE_SIZE;
	if(left) {
		// base_type mask = MASK[left];//((base_type)1 << left) - 1;
		return (_data[_dataSize - 1] & MASK[left]) == (b._data[b._dataSize - 1] & MASK[left]);
	}
	return true;
}

bool Bitmap::operator!=(const Bitmap& b) const { return !(*this == b); }

Bitmap Bitmap::operator&(const Bitmap& b) {
	Bitmap t(*this);
	return t &= b;
}

Bitmap Bitmap::operator|(const Bitmap& b) {
	Bitmap t(*this);
	return t |= b;
}

std::string Bitmap::toString() const {
	std::stringstream ss;
	ss << "[Bitmap compress=" << _compress << " size=" << _size << " data_size=" << _dataSize << " data=";
	for(size_t i = 0; i < _dataSize; ++i) {
		if(i) {
			ss << ",";
		}
		ss << _data[i];
	}
	// if(!_compress) {
	//     uint32_t cur_pos = 0;
	//     for(uint32_t i = 0; i < _dataSize; ++i) {
	//         base_type cur = _data[i];
	//         for(uint32_t n = 0; n < VALUE_SIZE && cur_pos < _size; ++n) {
	//             ++cur_pos;
	//             ss << (bool)(cur & ((base_type)1 << n));
	//         }
	//     }
	// } else {
	//     for(uint32_t i = 0; i < _dataSize; ++i) {
	//         if(i > 0) {
	//             ss << ",";
	//         }
	//         base_type cur = _data[i];
	//         for(uint32_t n = 0; n < VALUE_SIZE; ++n) {
	//             ss << (bool)(cur & ((base_type)1 << n));
	//         }
	//     }
	// }
	ss << "]";
	return ss.str();
}

bool Bitmap::get(uint32_t idx) const {
	uint32_t cur = idx / VALUE_SIZE;
	uint32_t pos = idx % VALUE_SIZE;
	return _data[cur] & POS[pos];
}

void Bitmap::set(uint32_t idx, bool v) {
	uint32_t cur = idx / VALUE_SIZE;
	uint32_t pos = idx % VALUE_SIZE;
	if(v) {
		_data[cur] |= POS[pos];
	} else {
		_data[cur] &= NPOS[pos];
	}
}

template <class T>
T count_bits(const T& v) {
	return __builtin_popcount(v);
}

template <>
uint64_t count_bits(const uint64_t& v) {
	return __builtin_popcount(v & 0xFFFFFFFF) + __builtin_popcount(v >> 32);
}

Bitmap::ptr Bitmap::compress() const {
	if(_compress) {
		return ptr(new Bitmap(*this));
	}

	base_type* data		   = (base_type*)malloc(_dataSize * sizeof(base_type));
	bool	   cur_value   = false;
	uint64_t   cur_count   = 0;
	uint32_t   dst_cur_pos = 0;
	for(uint32_t i = 0; i < _dataSize; ++i) {
		base_type cur = _data[i];
		auto	  c	  = count_bits((cur & COUNT_MASK));
		if(c == 0) {
			if(cur_value == false) {
				cur_count += VALUE_SIZE;
				continue;
			}
		} else if(c == VALUE_SIZE) {
			if(cur_value == true) {
				cur_count += VALUE_SIZE;
				continue;
			}
		}

		if((cur_count / VALUE_SIZE) > 1) {
			AZZATO_ASSERT(cur_count % VALUE_SIZE == 0);
			// AZZATO_ASSERT(cur_count < (1ul << VALUE_SIZE));
			while(cur_count) {
				data[dst_cur_pos++] = cur_value ? (COMPRESS_MASK | VALUE_MASK | (cur_count & COUNT_MASK))
												: (COMPRESS_MASK | (cur_count & COUNT_MASK));
				cur_count >>= VALUE_SIZE;
			}
		} else {
			if(cur_count) {
				data[dst_cur_pos++] = cur_value ? COUNT_MASK : 0;
			}
		}

		if(c == 0 || c == VALUE_SIZE) {
			cur_value = (c == VALUE_SIZE);
			cur_count = VALUE_SIZE;
		} else {
			data[dst_cur_pos++] = cur;
			cur_count			= 0;
		}
	}

	if(cur_count > 0) {
		if((cur_count / VALUE_SIZE) > 1) {
			while(cur_count) {
				data[dst_cur_pos++] = cur_value ? (COMPRESS_MASK | VALUE_MASK | (cur_count & COUNT_MASK))
												: (COMPRESS_MASK | (cur_count & COUNT_MASK));
				cur_count >>= VALUE_SIZE;
			}
		} else {
			data[dst_cur_pos++] = cur_value ? (COUNT_MASK) : (0);
		}
	}

	Bitmap::ptr b(new Bitmap);
	b->_compress = true;
	b->_size	 = _size;
	b->_dataSize = dst_cur_pos;
	b->_data	 = (base_type*)malloc(dst_cur_pos * sizeof(base_type));
	memcpy(b->_data, data, dst_cur_pos * sizeof(base_type));
	free(data);
	return b;
}

Bitmap::ptr Bitmap::uncompress() const {
	if(!_compress) {
		return ptr(new Bitmap(*this));
	}
	Bitmap::ptr b(new Bitmap(_size));
	uint32_t	cur_pos = 0;
	for(uint32_t i = 0; i < _dataSize;) {
		base_type cur = _data[i];
		if(cur & COMPRESS_MASK) {
			uint32_t count = cur & COUNT_MASK;
			bool	 v	   = cur & VALUE_MASK;
			uint32_t tmp_i = i;
			for(uint32_t n = i + 1; n < _dataSize; ++n) {
				if((_data[n] & COMPRESS_MASK) && (v == (bool)(_data[n] & VALUE_MASK))) {
					count = (((uint32_t)(_data[n] & COUNT_MASK)) << (VALUE_SIZE * (n - tmp_i))) | count;
					++i;
				} else {
					break;
				}
			}
			b->set(cur_pos, count, v);
			cur_pos += count;
			++i;
		} else {
			base_type count				   = cur & COUNT_MASK;
			b->_data[cur_pos / VALUE_SIZE] = count;
			cur_pos += VALUE_SIZE;
			++i;
		}
	}
	return b;
}

bool Bitmap::any() const {
	if(!_data) {
		return false;
	}
	if(!_compress) {
		for(uint32_t i = 0; i < _dataSize - 1; ++i) {
			if(_data[i]) {
				return true;
			}
		}
		return _data[_dataSize - 1]
			   & MASK[_size % VALUE_SIZE];	//(((base_type)1 << (_size % VALUE_SIZE)) - 1);
	} else {
		for(uint32_t i = 0; i < _dataSize; ++i) {
			uint8_t cur = _data[i];
			if(cur & COMPRESS_MASK) {
				if(cur & VALUE_MASK) {
					return true;
				}
			} else {
				if(cur) {
					return true;
				}
			}
		}
	}
	return false;
}

void Bitmap::resize(uint32_t size, bool v) {
	if(_compress) {
		throw std::logic_error("compress bitmap not support resize");
	}
	uint32_t len = ceil(size * 1.0 / VALUE_SIZE);
	if(len > _dataSize) {
		base_type* new_data = (base_type*)malloc(len * sizeof(base_type));
		memcpy(new_data, _data, _dataSize * sizeof(base_type));
		if(v) {
			memset(new_data + _dataSize, 0xFF, (len - _dataSize) * sizeof(base_type));
		} else {
			memset(new_data + _dataSize, 0, (len - _dataSize) * sizeof(base_type));
		}

		if(_data) {
			free(_data);
		}
		_data		  = new_data;

		uint32_t left = _size % VALUE_SIZE;
		if(v) {
			new_data[_dataSize - 1] |= MASK[left];	//((base_type)1 << left) - 1;
		} else {
			new_data[_dataSize - 1] &= MASK[left];	//((base_type)1 << left) - 1;
		}
	}
	_size	  = size;
	_dataSize = len;
}

void Bitmap::foreach(std::function<bool(uint32_t)> cb) {
	uint32_t max_size = _size / U64_VALUE_SIZE;
	uint32_t cur_pos  = 0;
	uint64_t tmp	  = 0;
	for(uint32_t i = 0; i < max_size; ++i) {
		tmp = ((uint64_t*)_data)[i];
		if(!tmp) {
			cur_pos += U64_VALUE_SIZE;
		} else {
			for(uint32_t n = 0; n < U64_DIV_BASE; ++n) {
				tmp = _data[i * U64_DIV_BASE + n];
				for(uint32_t x = 0; x < VALUE_SIZE && cur_pos < _size; ++x, ++cur_pos) {
					if(tmp & POS[x]) {	//((base_type)1 << x)) {
						if(!cb(cur_pos)) {
							return;
						}
					}
				}
			}
		}
	}
	for(uint32_t i = max_size * U64_DIV_BASE; i < _dataSize; ++i) {
		tmp = _data[i];
		if(!tmp) {
			cur_pos += VALUE_SIZE;
		} else {
			for(uint32_t n = 0; n < VALUE_SIZE && cur_pos < _size; ++n, ++cur_pos) {
				if(tmp & POS[n]) {	//(1UL << n)) {
					if(!cb(cur_pos)) {
						return;
					}
				}
			}
		}
	}
}

void Bitmap::rforeach(std::function<bool(uint32_t)> cb) {
	uint32_t max_size = _size / U64_VALUE_SIZE;
	uint32_t cur_pos  = _size - 1;
	uint64_t tmp	  = 0;

	int begin		  = _dataSize - 1;
	if(_size % U64_VALUE_SIZE) {
		int32_t start = _dataSize - 1;
		for(int32_t i = start; i >= (int32_t)(max_size * U64_DIV_BASE); --i) {
			--begin;
			tmp = _data[i];
			if(!tmp) {
				if(i == start) {
					if(_size % VALUE_SIZE == 0) {
						cur_pos -= VALUE_SIZE;
					} else {
						cur_pos -= _size % VALUE_SIZE;
					}
				} else {
					cur_pos -= VALUE_SIZE;
				}
			} else {
				if(i == start) {
					if(_size % VALUE_SIZE == 0) {
						for(int32_t n = VALUE_SIZE - 1; n >= 0; --n, --cur_pos) {
							if(tmp & POS[n]) {	//((base_type)1 << n)) {
								if(!cb(cur_pos)) {
									return;
								}
							}
						}
					} else {
						cur_pos += 1;
						for(int32_t n = _size % VALUE_SIZE; n >= 0; --n, --cur_pos) {
							if(tmp & POS[n]) {	//((base_type)1 << n)) {
								if(!cb(cur_pos)) {
									return;
								}
							}
						}
					}
				} else {
					for(int32_t n = VALUE_SIZE - 1; n >= 0; --n, --cur_pos) {
						if(tmp & POS[n]) {	//((base_type)1 << n)) {
							if(!cb(cur_pos)) {
								return;
							}
						}
					}
				}
			}
		}
	}

	// for(int32_t i = begin;
	//         i >= 0; --i){
	//     tmp = _data[i];
	//     for(int32_t x = VALUE_SIZE - 1; x >= 0; --x, --cur_pos) {
	//         if(tmp & ((base_type)1 << x)) {
	//             if(!cb(cur_pos)) {
	//                 return;
	//             }
	//         }
	//     }
	// }
	// return;

	for(int32_t i = max_size - 1; i >= 0; --i) {
		tmp = ((uint64_t*)_data)[i];
		if(!tmp) {
			cur_pos -= U64_VALUE_SIZE;
		} else {
			for(int32_t n = 0; n < (int32_t)U64_DIV_BASE; ++n) {
				tmp = _data[(i + 1) * U64_DIV_BASE - 1 - n];
				for(int32_t x = VALUE_SIZE - 1; x >= 0; --x, --cur_pos) {
					if(tmp & POS[x]) {	//((base_type)1 << x)) {
						if(!cb(cur_pos)) {
							return;
						}
					}
				}
			}
		}
	}
}

void Bitmap::listPosAsc(std::vector<uint32_t>& pos) {
	uint32_t max_size = _size / U64_VALUE_SIZE;
	uint32_t cur_pos  = 0;
	uint64_t tmp	  = 0;
	for(uint32_t i = 0; i < max_size; ++i) {
		tmp = ((uint64_t*)_data)[i];
		if(!tmp) {
			cur_pos += U64_VALUE_SIZE;
		} else {
			for(size_t x = 0; x < U64_DIV_BASE; ++x) {
				tmp = _data[i * U64_DIV_BASE + x];
				for(uint32_t n = 0; n < VALUE_SIZE && cur_pos < _size; ++n, ++cur_pos) {
					if(tmp & POS[n]) {	//(1UL << n)) {
						pos.push_back(cur_pos);
					}
				}
			}
		}
	}
	for(uint32_t i = max_size * U64_DIV_BASE; i < _dataSize; ++i) {
		tmp = _data[i];
		if(!tmp) {
			cur_pos += VALUE_SIZE;
		} else {
			for(uint32_t n = 0; n < VALUE_SIZE && cur_pos < _size; ++n, ++cur_pos) {
				if(tmp & POS[n]) {	//(1UL << n)) {
					pos.push_back(cur_pos);
				}
			}
		}
	}
}

bool Bitmap::cross(const Bitmap& b) const {
	if(!_compress && b._compress) {
		return compressCross(b);
	} else if(!_compress && !b._compress) {
		return normalCross(b);
	} else {
		// LOG_ERROR() << "Bitmap cross invalid type";
	}
	return false;
}

bool Bitmap::compressCross(const Bitmap& b) const {
	uint32_t cur_pos = 0;
	for(uint32_t i = 0; i < b._dataSize; ++i) {
		base_type cur = b._data[i];
		if(cur & COMPRESS_MASK) {
			uint32_t count = cur & COUNT_MASK;
			bool	 v	   = cur & VALUE_MASK;
			uint32_t tmp_i = i;
			for(uint32_t n = i + 1; n < b._dataSize; ++n) {
				if((b._data[n] & COMPRESS_MASK) && (v == (bool)(b._data[n] & VALUE_MASK))) {
					count = (((uint32_t)(b._data[n] & COUNT_MASK)) << (VALUE_SIZE * (n - tmp_i))) | count;
					++i;
				} else {
					break;
				}
			}
			if(v) {
				if(get(cur_pos, count, 1)) {
					return true;
				}
				cur_pos += count;
			} else {
				cur_pos += count;
			}
		} else {
			base_type count = cur & COUNT_MASK;
			if(_data[cur_pos / VALUE_SIZE] & count) {
				return true;
			}
			cur_pos += VALUE_SIZE;
		}
	}
	return false;
}

bool Bitmap::normalCross(const Bitmap& b) const {
	uint32_t max_size = _size / U64_VALUE_SIZE;
	for(uint32_t i = 0; i < max_size; ++i) {
		if((((uint64_t*)_data)[i]) != 0) {
			if((((uint64_t*)_data)[i]) & (((uint64_t*)b._data)[i])) {
				return true;
			}
		}
	}
	for(uint32_t i = max_size * U64_DIV_BASE; i < _dataSize; ++i) {
		if(_data[i] & b._data[i]) {
			return true;
		}
	}
	return false;
}

Bitmap::iterator_base::iterator_base()
	: _pos(-1)
	, _size(0)
	, _dataSize(0)
	, _compress(false)
	, _data(NULL) {}

bool Bitmap::iterator_base::operator!() {
	if(_pos >= _size || _pos < 0) {
		return false;
	}
	return true;
}

int32_t Bitmap::iterator_base::operator*() { return _pos; }

Bitmap::iterator::iterator(Bitmap* b) {
	_pos	  = -1;
	_size	  = b->_size;
	_dataSize = b->_dataSize;
	_data	  = b->_data;
	_compress = b->_compress;

	next();
}

void Bitmap::iterator::next() {
	++_pos;
	uint32_t  pos  = _pos / VALUE_SIZE;
	uint32_t  left = _pos % VALUE_SIZE;
	base_type v	   = 0;
	if(left) {
		v = _data[pos];
		for(int32_t i = left; i < (int32_t)VALUE_SIZE && _pos < _size; ++i, ++_pos) {
			if(v & POS[i]) {  //((base_type)1 << i)) {
				return;
			}
		}
		++pos;
	}
	for(int32_t i = _pos / VALUE_SIZE; i < _dataSize && _pos < _size; ++i) {
		v = _data[i];
		if(!v) {
			_pos += VALUE_SIZE;
			continue;
		}
		for(int32_t n = 0; n < (int32_t)VALUE_SIZE && _pos < _size; ++n, ++_pos) {
			if(v & POS[n]) {  //((base_type)1 << n)) {
				return;
			}
		}
	}
}

Bitmap::iterator_reverse::iterator_reverse(Bitmap* b) {
	_pos	  = b->_size;
	_size	  = b->_size;
	_dataSize = b->_dataSize;
	_data	  = b->_data;
	_compress = b->_compress;

	next();
}

void Bitmap::iterator_reverse::next() {
	if(_pos <= 0) {
		_pos = -1;
		return;
	}

	--_pos;
	int32_t	  left = (_pos) % VALUE_SIZE;
	int32_t	  pos  = (_pos) / VALUE_SIZE;
	base_type v	   = 0;
	if(left != (VALUE_SIZE - 1)) {
		v = _data[pos];
		for(int32_t i = left; i >= 0 && _pos >= 0; --i, --_pos) {
			if(v & POS[i]) {  //((base_type)1 << i)) {
				return;
			}
		}
		--pos;
	}
	for(int32_t i = pos; i >= 0 && _pos >= 0; --i) {
		v = _data[i];
		if(!v) {
			_pos -= VALUE_SIZE;
			continue;
		}
		for(int32_t n = VALUE_SIZE - 1; n >= 0 && _pos >= 0; --n, --_pos) {
			if(v & POS[n]) {  //((base_type)1 << n)) {
				return;
			}
		}
	}
}

void Bitmap::set(uint32_t from, uint32_t size, bool v) {
	if(size == 0) {
		return;
	}
	uint32_t cur_from = from / VALUE_SIZE;
	uint32_t pos_from = from % VALUE_SIZE;
	uint32_t cur_to	  = (from + size) / VALUE_SIZE;
	uint32_t pos_to	  = (from + size) % VALUE_SIZE;

	AZZATO_ASSERT(pos_from == 0);
	AZZATO_ASSERT(pos_to == 0);

	for(uint32_t i = cur_from; i < cur_to; ++i) {
		_data[i] = v ? (COUNT_MASK) : (0);
	}
}

bool Bitmap::get(uint32_t from, uint32_t size, bool v) const {
	if(size == 0) {
		return false;
	}
	uint32_t cur_from = from / VALUE_SIZE;
	uint32_t pos_from = from % VALUE_SIZE;
	uint32_t cur_to	  = (from + size) / VALUE_SIZE;
	uint32_t pos_to	  = (from + size) % VALUE_SIZE;

	AZZATO_ASSERT(pos_from == 0);
	AZZATO_ASSERT(pos_to == 0);

	for(uint32_t i = cur_from; i < cur_to; ++i) {
		if(v) {
			if(_data[i] & COUNT_MASK) {
				return true;
			}
		} else {
			if(!(_data[i] & COUNT_MASK)) {
				return true;
			}
		}
	}
	return false;
}

float Bitmap::getCompressRate() const {
	if(!_compress) {
		return 100;
	} else {
		return _dataSize * 100.0 / ceil(_size * 1.0 / VALUE_SIZE);
	}
}

Bitmap::iterator_base::ptr Bitmap::begin_new() { return iterator_base::ptr(new iterator(this)); }

Bitmap::iterator_base::ptr Bitmap::rbegin_new() { return iterator_base::ptr(new iterator_reverse(this)); }

uint32_t Bitmap::getCount() const {
	if(!_compress) {
		uint32_t len	 = _dataSize / U64_DIV_BASE;  // - (_dataSize % 8 == 0 ? 1 : 0);
		uint32_t count	 = 0;
		uint32_t cur_pos = 0;
		for(uint32_t i = 0; i < len; ++i) {
			uint64_t tmp = ((uint64_t*)(_data))[i];
			count += count_bits(tmp);
			cur_pos += U64_VALUE_SIZE;
		}
		for(uint32_t i = len * U64_DIV_BASE; i < _dataSize; ++i) {
			base_type tmp = _data[i];
			if(tmp) {
				for(uint32_t n = 0; n < VALUE_SIZE && cur_pos < _size; ++n, ++cur_pos) {
					if(tmp & POS[n]) {	//(1UL << n)) {
						++count;
					}
				}
			} else {
				cur_pos += VALUE_SIZE;
			}
		}
		return count;
	} else {
		uint32_t count = 0;
		for(uint32_t i = 0; i < _dataSize; ++i) {
			base_type tmp = _data[i];
			if(tmp & COMPRESS_MASK) {
				if(tmp & VALUE_MASK) {
					count += tmp & COUNT_MASK;
				}
			} else {
				count += count_bits(tmp);
			}
		}
		return count;
	}
}

}  // namespace azzato
