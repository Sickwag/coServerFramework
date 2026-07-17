#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <type_traits>
#include <vector>

#include "utils/endian.h"

namespace azzato {

/**
 * @brief Binary array with serialization/deserialization support.
 *
 * Data is stored in a linked list of fixed-size chunks. The unified
 * `read<Type, size>()` / `write<Type, size>(value)` templates dispatch
 * based on `Type`:
 *   - Integral types: fixed-width I/O using `sizeof(Type)`. No `size` needed.
 *   - `std::string`: `size` selects the length-prefix encoding (U16/U32/U64/
 *     Varint); default (Auto) writes/reads the body with no prefix.
 *   - Floating-point: fixed-width via `sizeof`.
 *
 * `ByteSize::Varint` opts into variable-length encoding (ZigZag for signed
 * integers).
 */
class ByteArray {
  public:
	using ptr = std::shared_ptr<ByteArray>;

	/**
	 * @brief Encoding selector.
	 *
	 * For integral types, `Auto` (default) means "use sizeof(Type)"; `Varint`
	 * means variable-length encoding. The fixed-width values U8/U16/U32/U64
	 * are mainly for string length-prefixes — for integers they must match
	 * sizeof(Type) (enforced by static_assert).
	 */
	enum class ByteSize : uint8_t {
		Auto   = 0,
		U8	   = 1,
		U16	   = 2,
		U32	   = 4,
		U64	   = 8,
		Varint = 0xFF,
	};

	/**
	 * @brief Storage node — a single chunk in the linked list of buffers.
	 */
	struct Node {
		explicit Node(size_t s);
		Node();
		~Node();

		char*  ptr	= nullptr;
		Node*  next = nullptr;
		size_t size = 0;
	};

	explicit ByteArray(size_t baseSize = 4096);
	~ByteArray();

	// -----------------------------------------------------------------------
	// Unified typed I/O
	// -----------------------------------------------------------------------

	/**
	 * @brief Read a value of `Type`.
	 *
	 * - `read<uint8_t>()` / `read<int32_t>()` / ... : fixed-width integer,
	 *   byte-swapped if endian differs from host.
	 * - `read<int32_t, ByteSize::Varint>()` : ZigZag varint.
	 * - `read<uint32_t, ByteSize::Varint>()` : plain varint.
	 * - `read<std::string, ByteSize::U16>()` : string with 16-bit length prefix.
	 * - `read<std::string>()` : read all remaining bytes as a string.
	 * - `read<float>()` / `read<double>()` : fixed-width float.
	 *
	 * @throws std::out_of_range if not enough data.
	 */
	template <typename Type, ByteSize size = ByteSize::Auto>
	Type read();

	/**
	 * @brief Write a value of `Type`.
	 *
	 * - `write<uint8_t>(v)` / `write<int32_t>(v)` / ... : fixed-width integer.
	 * - `write<int32_t, ByteSize::Varint>(v)` : ZigZag varint.
	 * - `write<std::string, ByteSize::U16>(s)` : string with 16-bit length prefix.
	 * - `write<std::string>(s)` : write body only, no length prefix.
	 * - `write<float>(f)` / `write<double>(d)` : fixed-width float.
	 */
	template <typename Type, ByteSize size = ByteSize::Auto>
	void write(Type value);

	// -----------------------------------------------------------------------
	// Raw buffer I/O
	// -----------------------------------------------------------------------

	/**
	 * @brief Write `size` bytes from `buf`.
	 * @post _position += size; if _position > _size then _size = _position.
	 */
	void write(const void* buf, size_t size);

	/**
	 * @brief Read `size` bytes into `buf` from the current position.
	 * @throws std::out_of_range if getReadSize() < size.
	 */
	void read(void* buf, size_t size);

	/**
	 * @brief Read `size` bytes into `buf` starting at `position` (const).
	 * @throws std::out_of_range if (_size - position) < size.
	 */
	void read(void* buf, size_t size, size_t position) const;

	// -----------------------------------------------------------------------
	// Capacity / position / state
	// -----------------------------------------------------------------------

	void clear();

	size_t getPosition() const { return _position; }

	void setPosition(size_t v);

	size_t getBaseSize() const { return _baseSize; }

	size_t getReadSize() const { return _size - _position; }

	size_t getSize() const { return _size; }

	bool isLittleEndian() const { return _endian == AZZATO_LITTLE_ENDIAN; }

	void setIsLittleEndian(bool val);

	// -----------------------------------------------------------------------
	// Serialization helpers
	// -----------------------------------------------------------------------

	std::string toString() const;
	std::string toHexString() const;
	bool		writeToFile(const std::string& name) const;
	bool		readFromFile(const std::string& name);

	uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;
	uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const;
	uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

  private:
	// Varuint primitives (declared here so the templates above can call them;
	// defined in the .cpp).
	void	 writeVaruint32(uint32_t value);
	void	 writeVaruint64(uint64_t value);
	uint32_t readVaruint32();
	uint64_t readVaruint64();

	static uint32_t encodeZigzag32(int32_t v);
	static uint64_t encodeZigzag64(int64_t v);
	static int32_t	decodeZigzag32(uint32_t v);
	static int64_t	decodeZigzag64(uint64_t v);

	void addCapacity(size_t size);

	size_t getCapacity() const { return _capacity - _position; }

	size_t _baseSize;  ///< Bytes per Node
	size_t _position;  ///< Current read/write cursor
	size_t _capacity;  ///< Total allocated bytes
	size_t _size;	   ///< Bytes of valid data written
	int8_t _endian;	   ///< AZZATO_LITTLE_ENDIAN or AZZATO_BIG_ENDIAN
	Node*  _root;	   ///< First chunk
	Node*  _cur;	   ///< Current chunk containing _position
};

// ======================================================================
// Template implementations
// ======================================================================

template <typename Type, ByteArray::ByteSize size>
Type ByteArray::read() {
	if constexpr(std::is_same_v<std::remove_cv_t<Type>, std::string>) {
		size_t len = 0;
		if constexpr(size == ByteSize::U16) {
			len = read<uint16_t>();
		} else if constexpr(size == ByteSize::U32) {
			len = read<uint32_t>();
		} else if constexpr(size == ByteSize::U64) {
			len = read<uint64_t>();
		} else if constexpr(size == ByteSize::Varint) {
			len = read<uint64_t, ByteSize::Varint>();
		} else {
			// Auto: read all remaining bytes
			len = getReadSize();
		}
		std::string s;
		if(len > 0) {
			s.resize(len);
			read(s.data(), len);
		}
		return s;
	} else if constexpr(std::is_integral_v<Type>) {
		if constexpr(size == ByteSize::Varint) {
			if constexpr(std::is_signed_v<Type>) {
				if constexpr(sizeof(Type) <= 4) {
					return static_cast<Type>(decodeZigzag32(readVaruint32()));
				} else {
					return static_cast<Type>(decodeZigzag64(readVaruint64()));
				}
			} else {
				if constexpr(sizeof(Type) <= 4) {
					return static_cast<Type>(readVaruint32());
				} else {
					return static_cast<Type>(readVaruint64());
				}
			}
		} else {
			static_assert(size == ByteSize::Auto || static_cast<size_t>(size) == sizeof(Type),
						  "Explicit ByteSize for integers must match sizeof(Type); "
						  "use a wider integer type instead.");
			using Storage = std::make_unsigned_t<Type>;
			Storage v;
			read(&v, sizeof(v));
			if constexpr(sizeof(Type) > 1) {
				if(_endian != AZZATO_BYTE_ORDER) {
					v = byteswap(v);
				}
			}
			return static_cast<Type>(v);
		}
	} else if constexpr(std::is_floating_point_v<Type>) {
		using Int = std::conditional_t<sizeof(Type) == 4, uint32_t, uint64_t>;
		Int	 v	  = read<Int>();
		Type result;
		std::memcpy(&result, &v, sizeof(result));
		return result;
	} else {
		static_assert(!sizeof(Type*), "Unsupported type for ByteArray::read");
	}
}

template <typename Type, ByteArray::ByteSize size>
void ByteArray::write(Type value) {
	if constexpr(std::is_same_v<std::remove_cv_t<Type>, std::string>) {
		if constexpr(size == ByteSize::U16) {
			write<uint16_t>(static_cast<uint16_t>(value.size()));
		} else if constexpr(size == ByteSize::U32) {
			write<uint32_t>(static_cast<uint32_t>(value.size()));
		} else if constexpr(size == ByteSize::U64) {
			write<uint64_t>(static_cast<uint64_t>(value.size()));
		} else if constexpr(size == ByteSize::Varint) {
			write<uint64_t, ByteSize::Varint>(static_cast<uint64_t>(value.size()));
		}
		// Auto: no length prefix
		if(!value.empty()) {
			write(value.data(), value.size());
		}
	} else if constexpr(std::is_integral_v<Type>) {
		if constexpr(size == ByteSize::Varint) {
			if constexpr(std::is_signed_v<Type>) {
				if constexpr(sizeof(Type) <= 4) {
					writeVaruint32(encodeZigzag32(static_cast<int32_t>(value)));
				} else {
					writeVaruint64(encodeZigzag64(static_cast<int64_t>(value)));
				}
			} else {
				if constexpr(sizeof(Type) <= 4) {
					writeVaruint32(static_cast<uint32_t>(value));
				} else {
					writeVaruint64(static_cast<uint64_t>(value));
				}
			}
		} else {
			static_assert(size == ByteSize::Auto || static_cast<size_t>(size) == sizeof(Type),
						  "Explicit ByteSize for integers must match sizeof(Type); "
						  "use a wider integer type instead.");
			using Storage = std::make_unsigned_t<Type>;
			Storage v	  = static_cast<Storage>(value);
			if constexpr(sizeof(Type) > 1) {
				if(_endian != AZZATO_BYTE_ORDER) {
					v = byteswap(v);
				}
			}
			write(&v, sizeof(v));
		}
	} else if constexpr(std::is_floating_point_v<Type>) {
		using Int = std::conditional_t<sizeof(Type) == 4, uint32_t, uint64_t>;
		Int v;
		std::memcpy(&v, &value, sizeof(v));
		write<Int>(v);
	} else {
		static_assert(!sizeof(Type*), "Unsupported type for ByteArray::write");
	}
}

}  // namespace azzato
