#pragma once

#include <byteswap.h>
#include <cstdint>

#define AZZATO_LITTLE_ENDIAN 1
#define AZZATO_BIG_ENDIAN 2

namespace azzato {

// Swap byte order for 64/32/16-bit values, selected at compile time via if constexpr.
template <typename T>
T byteswap(T value) {
	if constexpr(sizeof(T) == sizeof(uint64_t)) {
		return static_cast<T>(bswap_64(static_cast<uint64_t>(value)));
	} else if constexpr(sizeof(T) == sizeof(uint32_t)) {
		return static_cast<T>(bswap_32(static_cast<uint32_t>(value)));
	} else if constexpr(sizeof(T) == sizeof(uint16_t)) {
		return static_cast<T>(bswap_16(static_cast<uint16_t>(value)));
	}
}

// use special macro signal
#if BYTE_ORDER == BIG_ENDIAN
#	define AZZATO_BYTE_ORDER AZZATO_BIG_ENDIAN
#else
#	define AZZATO_BYTE_ORDER AZZATO_LITTLE_ENDIAN
#endif

#if AZZATO_BYTE_ORDER == AZZATO_BIG_ENDIAN
template <typename T>
T byteswapOnLittleEndian(T t) {
	return t;
}

template <typename T>
T byteswapOnBigEndian(T t) {
	return byteswap(t);
}

#else

template <typename T>
T byteswapOnLittleEndian(T t) {
	return byteswap(t);
}

template <typename T>
T byteswapOnBigEndian(T t) {
	return t;
}
#endif
}  // namespace azzato
