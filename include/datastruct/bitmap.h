#pragma once

#include "datastruct/bytearray.h"
#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

namespace azzato::datastruct {

enum class BitMapType {
	BitmapTypeUint8	 = 1,
	BitmapTypeUint16 = 2,
	BitmapTypeUint32 = 3,
	BitmapTypeUint64
};

#define BITMAP_TYPE_UINT8 1
#define BITMAP_TYPE_UINT16 2
#define BITMAP_TYPE_UINT32 3
#define BITMAP_TYPE_UINT64 4

#ifndef BITMAP_TYPE
#	define BITMAP_TYPE BITMAP_TYPE_UINT16
#endif

class Bitmap {
  public:
	using ptr = std::shared_ptr<Bitmap>;
#if BITMAP_TYPE == BITMAP_TYPE_UINT8
	typedef uint8_t base_type;
#elif BITMAP_TYPE == BITMAP_TYPE_UINT16
	using base_type = uint16_t;
#elif BITMAP_TYPE == BITMAP_TYPE_UINT32
	typedef uint32_t base_type;
#elif BITMAP_TYPE == BITMAP_TYPE_UINT64
	typedef uint64_t base_type;
#endif
};
}  // namespace azzato::datastruct
