#include "streams/zlib_stream.h"
#include "utils/macro.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace azzato {

ZlibStream::ptr ZlibStream::createGzip(bool encode, uint32_t buffSize) {
	return create(encode, buffSize, Gzip);
}

ZlibStream::ptr ZlibStream::createZlib(bool encode, uint32_t buffSize) {
	return create(encode, buffSize, Zlib);
}

ZlibStream::ptr ZlibStream::createDeflate(bool encode, uint32_t buffSize) {
	return create(encode, buffSize, Deflate);
}

ZlibStream::ptr ZlibStream::create(bool		encode,
								   uint32_t buffSize,
								   Type		type,
								   int		level,
								   int		windowBits,
								   int		memLevel,
								   Strategy strategy) {
	ZlibStream::ptr result(new ZlibStream(encode, buffSize));
	if(result->init(type, level, windowBits, memLevel, strategy) == Z_OK) {
		return result;
	}
	return nullptr;
}

ZlibStream::ZlibStream(bool encode, uint32_t buffSize)
	: _buffSize(buffSize)
	, _encode(encode)
	, _free(true) {}

ZlibStream::~ZlibStream() {
	if(_free) {
		for(auto& buff : _buffs) {
			std::free(buff.iov_base);
		}
	}

	if(_encode) {
		deflateEnd(&_zstream);
	} else {
		inflateEnd(&_zstream);
	}
}

int ZlibStream::read(void* buffer, size_t length) { throw std::logic_error("ZlibStream::read is invalid"); }

int ZlibStream::read(ByteArray::ptr ba, size_t length) {
	throw std::logic_error("ZlibStream::read is invalid");
}

int ZlibStream::write(const void* buffer, size_t length) {
	iovec ivc;
	ivc.iov_base = const_cast<void*>(buffer);
	ivc.iov_len	 = length;
	if(_encode) {
		return encode(&ivc, 1, false);
	}
	return decode(&ivc, 1, false);
}

int ZlibStream::write(ByteArray::ptr ba, size_t length) {
	std::vector<iovec> buffers;
	ba->getReadBuffers(buffers, length);
	if(_encode) {
		return encode(buffers.data(), buffers.size(), false);
	}
	return decode(buffers.data(), buffers.size(), false);
}

void ZlibStream::close() { flush(); }

int ZlibStream::init(Type type, int level, int windowBits, int memLevel, Strategy strategy) {
	AZZATO_ASSERT((level >= 0 && level <= 9) || level == DefaultCompression);
	AZZATO_ASSERT(windowBits >= 8 && windowBits <= 15);
	AZZATO_ASSERT(memLevel >= 1 && memLevel <= 9);

	std::memset(&_zstream, 0, sizeof(_zstream));

	_zstream.zalloc = Z_NULL;
	_zstream.zfree	= Z_NULL;
	_zstream.opaque = Z_NULL;

	switch(type) {
	case Deflate:
		windowBits = -windowBits;
		break;
	case Gzip:
		windowBits += 16;
		break;
	case Zlib:
	default:
		break;
	}

	if(_encode) {
		return deflateInit2(&_zstream, level, Z_DEFLATED, windowBits, memLevel, static_cast<int>(strategy));
	}
	return inflateInit2(&_zstream, windowBits);
}

int ZlibStream::encode(const iovec* v, const uint64_t& size, bool finish) {
	int ret	  = 0;
	int flush = 0;
	for(uint64_t i = 0; i < size; ++i) {
		_zstream.avail_in = static_cast<uInt>(v[i].iov_len);
		_zstream.next_in  = static_cast<Bytef*>(v[i].iov_base);

		flush			  = finish ? (i == size - 1 ? Z_FINISH : Z_NO_FLUSH) : Z_NO_FLUSH;

		iovec* ivc		  = nullptr;
		do {
			if(!_buffs.empty() && _buffs.back().iov_len != _buffSize) {
				ivc = &_buffs.back();
			} else {
				iovec vc;
				vc.iov_base = std::malloc(_buffSize);
				vc.iov_len	= 0;
				_buffs.push_back(vc);
				ivc = &_buffs.back();
			}

			_zstream.avail_out = static_cast<uInt>(_buffSize - ivc->iov_len);
			_zstream.next_out  = static_cast<Bytef*>(ivc->iov_base) + ivc->iov_len;

			ret				   = deflate(&_zstream, flush);
			if(ret == Z_STREAM_ERROR) {
				return ret;
			}
			ivc->iov_len = _buffSize - _zstream.avail_out;
		} while(_zstream.avail_out == 0);
	}
	if(flush == Z_FINISH) {
		deflateEnd(&_zstream);
	}
	return Z_OK;
}

int ZlibStream::decode(const iovec* v, const uint64_t& size, bool finish) {
	int ret	  = 0;
	int flush = 0;
	for(uint64_t i = 0; i < size; ++i) {
		_zstream.avail_in = static_cast<uInt>(v[i].iov_len);
		_zstream.next_in  = static_cast<Bytef*>(v[i].iov_base);

		flush			  = finish ? (i == size - 1 ? Z_FINISH : Z_NO_FLUSH) : Z_NO_FLUSH;

		iovec* ivc		  = nullptr;
		do {
			if(!_buffs.empty() && _buffs.back().iov_len != _buffSize) {
				ivc = &_buffs.back();
			} else {
				iovec vc;
				vc.iov_base = std::malloc(_buffSize);
				vc.iov_len	= 0;
				_buffs.push_back(vc);
				ivc = &_buffs.back();
			}

			_zstream.avail_out = static_cast<uInt>(_buffSize - ivc->iov_len);
			_zstream.next_out  = static_cast<Bytef*>(ivc->iov_base) + ivc->iov_len;

			ret				   = inflate(&_zstream, flush);
			if(ret == Z_STREAM_ERROR) {
				return ret;
			}
			ivc->iov_len = _buffSize - _zstream.avail_out;
		} while(_zstream.avail_out == 0);
	}

	if(flush == Z_FINISH) {
		inflateEnd(&_zstream);
	}
	return Z_OK;
}

int ZlibStream::flush() {
	iovec ivc;
	ivc.iov_base = nullptr;
	ivc.iov_len	 = 0;

	if(_encode) {
		return encode(&ivc, 1, true);
	}
	return decode(&ivc, 1, true);
}

std::string ZlibStream::getResult() const {
	std::string result;
	for(auto& buff : _buffs) {
		result.append(static_cast<const char*>(buff.iov_base), buff.iov_len);
	}
	return result;
}

ByteArray::ptr ZlibStream::getByteArray() {
	ByteArray::ptr ba(new ByteArray);
	for(auto& buff : _buffs) {
		ba->write(buff.iov_base, buff.iov_len);
	}
	return ba;
}

}  // namespace azzato
