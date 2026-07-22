#include "datastruct/roaring_bitmap.h"
#include "log.h"
#include "utils/macro.h"
#include <iostream>
#include <math.h>
#include <sstream>
#include <string.h>

namespace azzato {

RoaringBitmap::RoaringBitmap(const Roaring& b)
	: _bitmap(b) {}

RoaringBitmap::RoaringBitmap() {}

RoaringBitmap::RoaringBitmap(uint32_t size) { _bitmap.addRange(0, size); }

RoaringBitmap::RoaringBitmap(const RoaringBitmap& b) { _bitmap = b._bitmap; }

RoaringBitmap::~RoaringBitmap() {}

void RoaringBitmap::writeTo(azzato::ByteArray::ptr ba) const {
	size_t size = _bitmap.getSizeInBytes(false);
	ba->write<uint32_t>(size);
	std::vector<char> buffer(size);
	_bitmap.write(buffer.data(), false);
	ba->write(buffer.data(), size);
}

bool RoaringBitmap::readFrom(azzato::ByteArray::ptr ba) {
	try {
		size_t			  size = ba->read<uint32_t>();
		std::vector<char> buffer(size);
		ba->read(buffer.data(), size);
		_bitmap = Roaring::read(buffer.data(), false);
		return true;
	} catch(...) {
	}
	return false;
}

RoaringBitmap& RoaringBitmap::operator=(const RoaringBitmap& b) {
	if(this == &b) {
		return *this;
	}
	_bitmap = b._bitmap;
	return *this;
}

RoaringBitmap& RoaringBitmap::operator&=(const RoaringBitmap& b) {
	_bitmap &= b._bitmap;
	return *this;
}

// RoaringBitmap& RoaringBitmap::operator~() {
// }

RoaringBitmap& RoaringBitmap::operator|=(const RoaringBitmap& b) {
	_bitmap |= b._bitmap;
	return *this;
}

RoaringBitmap& RoaringBitmap::operator-=(const RoaringBitmap& b) {
	_bitmap -= b._bitmap;
	return *this;
}

RoaringBitmap& RoaringBitmap::operator^=(const RoaringBitmap& b) {
	_bitmap ^= b._bitmap;
	return *this;
}

bool RoaringBitmap::operator==(const RoaringBitmap& b) const {
	if(this == &b) {
		return true;
	}
	return _bitmap == b._bitmap;
}

bool RoaringBitmap::operator!=(const RoaringBitmap& b) const { return !(*this == b); }

RoaringBitmap RoaringBitmap::operator&(const RoaringBitmap& b) {
	return RoaringBitmap(_bitmap & b._bitmap);
}

RoaringBitmap RoaringBitmap::operator|(const RoaringBitmap& b) {
	return RoaringBitmap(_bitmap | b._bitmap);
}

RoaringBitmap RoaringBitmap::operator-(const RoaringBitmap& b) {
	return RoaringBitmap(_bitmap - b._bitmap);
}

RoaringBitmap RoaringBitmap::operator^(const RoaringBitmap& b) {
	return RoaringBitmap(_bitmap ^ b._bitmap);
}

std::string RoaringBitmap::toString() const {
	std::stringstream ss;
	ss << "[RoaringBitmap count=" << getCount() << " size=" << _bitmap.getSizeInBytes() << "]";
	return ss.str();
}

bool RoaringBitmap::get(uint32_t idx) const { return _bitmap.contains(idx); }

void RoaringBitmap::set(uint32_t idx, bool v) {
	if(v) {
		_bitmap.add(idx);
	} else {
		_bitmap.remove(idx);
	}
}

RoaringBitmap::ptr RoaringBitmap::compress() const {
	RoaringBitmap::ptr rt(new RoaringBitmap(*this));
	rt->_bitmap.shrinkToFit();
	rt->_bitmap.runOptimize();
	return rt;
}

RoaringBitmap::ptr RoaringBitmap::uncompress() const {
	RoaringBitmap::ptr rt(new RoaringBitmap(*this));
	rt->_bitmap.removeRunCompression();
	return rt;
}

bool RoaringBitmap::any() const { return _bitmap.begin() != _bitmap.end(); }

void RoaringBitmap::foreach(std::function<bool(uint32_t)> cb) {
	for(auto it = _bitmap.begin(); it != _bitmap.end(); ++it) {
		if(!cb(*it)) {
			break;
		}
	}
}

void RoaringBitmap::rforeach(std::function<bool(uint32_t)> cb) {
	for(auto it = _bitmap.rbegin(); it != _bitmap.rend(); ++it) {
		if(!cb(*it)) {
			break;
		}
	}
}

void RoaringBitmap::listPosAsc(std::vector<uint32_t>& pos) {
	for(auto it = _bitmap.begin(); it != _bitmap.end(); ++it) {
		pos.push_back(*it);
	}
}

bool RoaringBitmap::cross(const RoaringBitmap& b) const { return _bitmap.intersect(b._bitmap); }

void RoaringBitmap::set(uint32_t from, uint32_t size, bool v) { _bitmap.addRange(from, from + size); }

bool RoaringBitmap::get(uint32_t from, uint32_t size, bool v) const { return false; }

float RoaringBitmap::getCompressRate() const { return 100; }

uint32_t RoaringBitmap::getCount() const { return _bitmap.cardinality(); }

}  // namespace azzato
