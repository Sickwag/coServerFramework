#include "stream.h"

namespace azzato {

int Stream::readFixSize(void* buffer, size_t length) {
	size_t offset = 0;
	int64_t left	= static_cast<int64_t>(length);
	while(left > 0) {
		int64_t len = read(static_cast<char*>(buffer) + offset, static_cast<size_t>(left));
		if(len <= 0) {
			return static_cast<int>(len);
		}
		offset += static_cast<size_t>(len);
		left -= len;
	}
	return static_cast<int>(length);
}

int Stream::readFixSize(ByteArray::ptr ba, size_t length) {
	int64_t left = static_cast<int64_t>(length);
	while(left > 0) {
		int64_t len = read(ba, static_cast<size_t>(left));
		if(len <= 0) {
			return static_cast<int>(len);
		}
		left -= len;
	}
	return static_cast<int>(length);
}

int Stream::writeFixSize(const void* buffer, size_t length) {
	size_t offset = 0;
	int64_t left	= static_cast<int64_t>(length);
	while(left > 0) {
		int64_t len = write(static_cast<const char*>(buffer) + offset, static_cast<size_t>(left));
		if(len <= 0) {
			return static_cast<int>(len);
		}
		offset += static_cast<size_t>(len);
		left -= len;
	}
	return static_cast<int>(length);
}

int Stream::writeFixSize(ByteArray::ptr ba, size_t length) {
	int64_t left = static_cast<int64_t>(length);
	while(left > 0) {
		int64_t len = write(ba, static_cast<size_t>(left));
		if(len <= 0) {
			return static_cast<int>(len);
		}
		left -= len;
	}
	return static_cast<int>(length);
}

}  // namespace azzato
