#pragma once

#include "datastruct/bytearray.h"
#include "datastruct/roaring.hh"
#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

namespace azzato {

class RoaringBitmap {
  public:
	typedef std::shared_ptr<RoaringBitmap> ptr;

	RoaringBitmap();
	RoaringBitmap(uint32_t size);
	RoaringBitmap(const RoaringBitmap& b);
	~RoaringBitmap();

	RoaringBitmap& operator=(const RoaringBitmap& b);

	std::string toString() const;
	bool		get(uint32_t idx) const;
	void		set(uint32_t idx, bool v);

	void set(uint32_t from, uint32_t size, bool v);
	bool get(uint32_t from, uint32_t size, bool v) const;

	RoaringBitmap& operator&=(const RoaringBitmap& b);
	RoaringBitmap& operator|=(const RoaringBitmap& b);
	RoaringBitmap& operator-=(const RoaringBitmap& b);
	RoaringBitmap& operator^=(const RoaringBitmap& b);

	RoaringBitmap operator&(const RoaringBitmap& b);
	RoaringBitmap operator|(const RoaringBitmap& b);
	RoaringBitmap operator-(const RoaringBitmap& b);
	RoaringBitmap operator^(const RoaringBitmap& b);

	// RoaringBitmap& operator~();

	bool operator==(const RoaringBitmap& b) const;
	bool operator!=(const RoaringBitmap& b) const;

	RoaringBitmap::ptr compress() const;
	RoaringBitmap::ptr uncompress() const;

	bool any() const;

	void listPosAsc(std::vector<uint32_t>& pos);

	void foreach(std::function<bool(uint32_t)> cb);
	void rforeach(std::function<bool(uint32_t)> cb);

	void writeTo(azzato::ByteArray::ptr ba) const;
	bool readFrom(azzato::ByteArray::ptr ba);

	// uncompress to compress
	// uncompress to uncompress
	bool cross(const RoaringBitmap& b) const;

	float getCompressRate() const;

	uint32_t getCount() const;

  public:
	typedef RoaringSetBitForwardIterator iterator;
	typedef RoaringSetBitReverseIterator reverse_iterator;

	iterator begin() const { return _bitmap.begin(); }
	iterator end() const { return _bitmap.end(); }

	reverse_iterator rbegin() const { return _bitmap.rbegin(); }
	reverse_iterator rend() const { return _bitmap.rend(); }

  private:
	RoaringBitmap(const Roaring& b);

  private:
	Roaring _bitmap;
};

}  // namespace azzato

