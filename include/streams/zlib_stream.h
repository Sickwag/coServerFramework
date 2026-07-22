#pragma once

#include "stream.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <zlib.h>

namespace azzato {

/**
 * @brief In-memory zlib/gzip/deflate compression stream.
 */
class ZlibStream : public Stream {
  public:
	using ptr = std::shared_ptr<ZlibStream>;

	enum Type {
		Zlib,
		Deflate,
		Gzip
	};

	enum Strategy {
		DefaultStrategy = Z_DEFAULT_STRATEGY,
		Filtered		= Z_FILTERED,
		HuffmanOnly		= Z_HUFFMAN_ONLY,
		Fixed			= Z_FIXED,
		Rle				= Z_RLE
	};

	enum CompressLevel {
		NoCompression	   = Z_NO_COMPRESSION,
		BestSpeed		   = Z_BEST_SPEED,
		BestCompression	   = Z_BEST_COMPRESSION,
		DefaultCompression = Z_DEFAULT_COMPRESSION
	};

	static ptr createGzip(bool encode, uint32_t buffSize = 4096);

	static ptr createZlib(bool encode, uint32_t buffSize = 4096);

	static ptr createDeflate(bool encode, uint32_t buffSize = 4096);

	static ptr create(bool	   encode,
					  uint32_t buffSize	  = 4096,
					  Type	   type		  = Deflate,
					  int	   level	  = DefaultCompression,
					  int	   windowBits = 15,
					  int	   memLevel	  = 8,
					  Strategy strategy	  = DefaultStrategy);

	ZlibStream(bool encode, uint32_t buffSize = 4096);

	~ZlibStream();

	int read(void* buffer, size_t length) override;

	int read(ByteArray::ptr ba, size_t length) override;

	int write(const void* buffer, size_t length) override;

	int write(ByteArray::ptr ba, size_t length) override;

	void close() override;

	int flush();

	bool isFree() const { return _free; }

	void setFree(bool value) { _free = value; }

	bool isEncode() const { return _encode; }

	void setEncode(bool value) { _encode = value; }

	std::vector<iovec>& getBuffers() { return _buffs; }

	std::string getResult() const;

	ByteArray::ptr getByteArray();

  private:
	int init(Type	  type		 = Deflate,
			 int	  level		 = DefaultCompression,
			 int	  windowBits = 15,
			 int	  memLevel	 = 8,
			 Strategy strategy	 = DefaultStrategy);

	int encode(const iovec* v, const uint64_t& size, bool finish);

	int decode(const iovec* v, const uint64_t& size, bool finish);

  private:
	z_stream		   _zstream;
	uint32_t		   _buffSize;
	bool			   _encode;
	bool			   _free;
	std::vector<iovec> _buffs;
};

}  // namespace azzato
