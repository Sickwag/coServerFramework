#pragma once

#include <byteswap.h>
#include <stdint.h>
#include <type_traits>

#define AZZATO_LITTLE_ENDIAN 1
#define AZZATO_BIG_ENDIAN 2

namespace azzato {
// C++11 std::enable_if
// template <class T>
// typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type byteswap(T value) {
// 	return (T)bswap_64((uint64_t)(value));
// }
// template <class T>
// typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type byteswap(T value) {
// 	return (T)bswap_64((uint32_t)(value));
// }
// template <class T>
// typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type byteswap(T value) {
// 	return (T)bswap_64((uint16_t)(value));
// }

// C++17 if constexpr method
template <class T>
T byteswap(T value) {
	if constexpr(sizeof(T) == sizeof(uint64_t)) {
		return (T)bswap_64(value);
	} else if constexpr(sizeof(T) == sizeof(uint32_t)) {
		return (T)bswap_32(value);
	} else if constexpr(sizeof(T) == sizeof(uint16_t)) {
		return (T)bswap_16(value);
	}
}

// use special macro signal
#if BYTE_ORDER == BIG_ENDIAN
#	define AZZATO_BYTE_ORDER AZZATO_BIG_ENDIAN
#else
#	define AZZATO_BYTE_ORDER AZZATO_LITTLE_ENDIAN
#endif

#if AZZATO_BYTE_ORDER == AZZATO_BIG_ENDIAN
template <class T>
T byteswapOnLittleEndian(T t) {
	return t;
}

template <class T>
T byteswapOnBigEndian(T t) {
	return byteswap(t);
}

#else

template <class T>
T byteswapOnLittleEndian(T t) {
	return byteswap(t);
}

template <class T>
T byteswapOnBigEndian(T t) {
	return t;
}
#endif
}  // namespace azzato
