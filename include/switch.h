#pragma once

#include <type_traits>

// main controller
template <typename EnumClass>
struct EnableBitMask {
	static constexpr bool LogModule = false;
};

template <typename EnumClass>
constexpr auto operator|(EnumClass lhs,
						 EnumClass rhs) -> std::enable_if_t<EnableBitMask<EnumClass>::LogModule, EnumClass> {
	using underlying = std::underlying_type_t<EnumClass>;
	return static_cast<EnumClass>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename EnumClass>
constexpr auto operator&(EnumClass lhs,
						 EnumClass rhs) -> std::enable_if_t<EnableBitMask<EnumClass>::LogModule, EnumClass> {
	using underlying = std::underlying_type_t<EnumClass>;
	return static_cast<EnumClass>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename EnumClass>
constexpr auto operator~(EnumClass val) -> std::enable_if_t<EnableBitMask<EnumClass>::LogModule, EnumClass> {
	using underlying = std::underlying_type_t<EnumClass>;
	return static_cast<EnumClass>(~static_cast<underlying>(val));
}
