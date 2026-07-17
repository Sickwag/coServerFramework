#include "datastruct/bytearray.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "log.h"

namespace azzato {

static Logger::ptr gLogger = LoggerMgr::getInstance()->getLogger("system");

// ======================================================================
// Node
// ======================================================================

ByteArray::Node::Node(size_t s)
	: ptr(new char[s]())
	, next(nullptr)
	, size(s) {}

ByteArray::Node::Node()
	: ptr(nullptr)
	, next(nullptr)
	, size(0) {}

ByteArray::Node::~Node() { delete[] ptr; }

// ======================================================================
// ByteArray
// ======================================================================

ByteArray::ByteArray(size_t baseSize)
	: _baseSize(baseSize)
	, _position(0)
	, _capacity(baseSize)
	, _size(0)
	, _endian(AZZATO_BIG_ENDIAN)
	, _root(new Node(baseSize))
	, _cur(_root) {}

ByteArray::~ByteArray() {
	Node* tmp = _root;
	while(tmp) {
		_cur = tmp;
		tmp	 = tmp->next;
		delete _cur;
	}
}

void ByteArray::setIsLittleEndian(bool val) { _endian = val ? AZZATO_LITTLE_ENDIAN : AZZATO_BIG_ENDIAN; }

// ======================================================================
// ZigZag helpers
// ======================================================================

uint32_t ByteArray::encodeZigzag32(int32_t v) {
	return v < 0 ? (static_cast<uint32_t>(-v)) * 2 - 1 : static_cast<uint32_t>(v) * 2;
}

uint64_t ByteArray::encodeZigzag64(int64_t v) {
	return v < 0 ? (static_cast<uint64_t>(-v)) * 2 - 1 : static_cast<uint64_t>(v) * 2;
}

int32_t ByteArray::decodeZigzag32(uint32_t v) { return static_cast<int32_t>((v >> 1) ^ -(v & 1)); }

int64_t ByteArray::decodeZigzag64(uint64_t v) { return static_cast<int64_t>((v >> 1) ^ -(v & 1)); }

// ======================================================================
// Varuint primitives
// ======================================================================

void ByteArray::writeVaruint32(uint32_t value) {
	uint8_t tmp[5];
	uint8_t i = 0;
	while(value >= 0x80) {
		tmp[i++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
		value >>= 7;
	}
	tmp[i++] = static_cast<uint8_t>(value);
	write(tmp, i);
}

void ByteArray::writeVaruint64(uint64_t value) {
	uint8_t tmp[10];
	uint8_t i = 0;
	while(value >= 0x80) {
		tmp[i++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
		value >>= 7;
	}
	tmp[i++] = static_cast<uint8_t>(value);
	write(tmp, i);
}

uint32_t ByteArray::readVaruint32() {
	uint32_t result = 0;
	for(int i = 0; i < 32; i += 7) {
		uint8_t b;
		read(&b, 1);
		if(b < 0x80) {
			result |= static_cast<uint32_t>(b) << i;
			break;
		}
		result |= static_cast<uint32_t>(b & 0x7f) << i;
	}
	return result;
}

uint64_t ByteArray::readVaruint64() {
	uint64_t result = 0;
	for(int i = 0; i < 64; i += 7) {
		uint8_t b;
		read(&b, 1);
		if(b < 0x80) {
			result |= static_cast<uint64_t>(b) << i;
			break;
		}
		result |= static_cast<uint64_t>(b & 0x7f) << i;
	}
	return result;
}

// ======================================================================
// Raw buffer I/O
// ======================================================================

void ByteArray::write(const void* buf, size_t size) {
	if(size == 0) {
		return;
	}
	addCapacity(size);

	size_t nodeOffset = _position % _baseSize;
	size_t nodeAvail  = _cur->size - nodeOffset;
	size_t bufOffset  = 0;

	while(size > 0) {
		if(nodeAvail >= size) {
			std::memcpy(_cur->ptr + nodeOffset, static_cast<const char*>(buf) + bufOffset, size);
			if(_cur->size == (nodeOffset + size)) {
				_cur = _cur->next;
			}
			_position += size;
			bufOffset += size;
			size = 0;
		} else {
			std::memcpy(_cur->ptr + nodeOffset, static_cast<const char*>(buf) + bufOffset, nodeAvail);
			_position += nodeAvail;
			bufOffset += nodeAvail;
			size -= nodeAvail;
			_cur	   = _cur->next;
			nodeAvail  = _cur->size;
			nodeOffset = 0;
		}
	}

	if(_position > _size) {
		_size = _position;
	}
}

void ByteArray::read(void* buf, size_t size) {
	if(size > getReadSize()) {
		throw std::out_of_range("not enough len");
	}

	size_t nodeOffset = _position % _baseSize;
	size_t nodeAvail  = _cur->size - nodeOffset;
	size_t bufOffset  = 0;
	while(size > 0) {
		if(nodeAvail >= size) {
			std::memcpy(static_cast<char*>(buf) + bufOffset, _cur->ptr + nodeOffset, size);
			if(_cur->size == (nodeOffset + size)) {
				_cur = _cur->next;
			}
			_position += size;
			bufOffset += size;
			size = 0;
		} else {
			std::memcpy(static_cast<char*>(buf) + bufOffset, _cur->ptr + nodeOffset, nodeAvail);
			_position += nodeAvail;
			bufOffset += nodeAvail;
			size -= nodeAvail;
			_cur	   = _cur->next;
			nodeAvail  = _cur->size;
			nodeOffset = 0;
		}
	}
}

void ByteArray::read(void* buf, size_t size, size_t position) const {
	if(size > (_size - position)) {
		throw std::out_of_range("not enough len");
	}

	size_t nodeOffset = position % _baseSize;
	size_t count	  = position / _baseSize;
	Node*  cur		  = _root;
	while(count > 0) {
		cur = cur->next;
		--count;
	}
	size_t nodeAvail = cur->size - nodeOffset;
	size_t bufOffset = 0;
	while(size > 0) {
		if(nodeAvail >= size) {
			std::memcpy(static_cast<char*>(buf) + bufOffset, cur->ptr + nodeOffset, size);
			if(cur->size == (nodeOffset + size)) {
				cur = cur->next;
			}
			position += size;
			bufOffset += size;
			size = 0;
		} else {
			std::memcpy(static_cast<char*>(buf) + bufOffset, cur->ptr + nodeOffset, nodeAvail);
			position += nodeAvail;
			bufOffset += nodeAvail;
			size -= nodeAvail;
			cur		   = cur->next;
			nodeAvail  = cur->size;
			nodeOffset = 0;
		}
	}
}

// ======================================================================
// State management
// ======================================================================

void ByteArray::clear() {
	_position = _size = 0;
	_capacity		  = _baseSize;
	Node* tmp		  = _root->next;
	while(tmp) {
		_cur = tmp;
		tmp	 = tmp->next;
		delete _cur;
	}
	_cur		= _root;
	_root->next = nullptr;
}

void ByteArray::setPosition(size_t v) {
	if(v > _capacity) {
		throw std::out_of_range("set_position out of range");
	}
	_position = v;
	if(_position > _size) {
		_size = _position;
	}
	_cur = _root;
	while(v > _cur->size) {
		v -= _cur->size;
		_cur = _cur->next;
	}
	if(v == _cur->size) {
		_cur = _cur->next;
	}
}

void ByteArray::addCapacity(size_t size) {
	if(size == 0) {
		return;
	}
	size_t oldCap = getCapacity();
	if(oldCap >= size) {
		return;
	}

	size		 = size - oldCap;
	size_t count = static_cast<size_t>(std::ceil(1.0 * size / _baseSize));
	Node*  tmp	 = _root;
	while(tmp->next) {
		tmp = tmp->next;
	}

	Node* first = nullptr;
	for(size_t i = 0; i < count; ++i) {
		tmp->next = new Node(_baseSize);
		if(first == nullptr) {
			first = tmp->next;
		}
		tmp = tmp->next;
		_capacity += _baseSize;
	}

	if(oldCap == 0) {
		_cur = first;
	}
}

// ======================================================================
// File I/O
// ======================================================================

bool ByteArray::writeToFile(const std::string& name) const {
	std::ofstream ofs;
	ofs.open(name, std::ios::trunc | std::ios::binary);
	if(!ofs) {
		AZZATO_LOG_ERROR(gLogger) << "writeToFile name=" << name << " error , errno=" << errno
								  << " errstr=" << strerror(errno);
		return false;
	}

	int64_t readSize = static_cast<int64_t>(getReadSize());
	int64_t pos		 = static_cast<int64_t>(_position);
	Node*	cur		 = _cur;

	while(readSize > 0) {
		int		diff = static_cast<int>(pos % _baseSize);
		int64_t len =
			(readSize > static_cast<int64_t>(_baseSize) ? static_cast<int64_t>(_baseSize) : readSize) - diff;
		ofs.write(cur->ptr + diff, len);
		cur = cur->next;
		pos += len;
		readSize -= len;
	}

	return true;
}

bool ByteArray::readFromFile(const std::string& name) {
	std::ifstream ifs;
	ifs.open(name, std::ios::binary);
	if(!ifs) {
		AZZATO_LOG_ERROR(gLogger) << "readFromFile name=" << name << " error, errno=" << errno
								  << " errstr=" << strerror(errno);
		return false;
	}

	std::shared_ptr<char> buff(new char[_baseSize], [](char* ptr) { delete[] ptr; });
	while(!ifs.eof()) {
		ifs.read(buff.get(), _baseSize);
		write(buff.get(), ifs.gcount());
	}
	return true;
}

// ======================================================================
// Stringification
// ======================================================================

std::string ByteArray::toString() const {
	std::string str;
	str.resize(getReadSize());
	if(str.empty()) {
		return str;
	}
	read(str.data(), str.size(), _position);
	return str;
}

std::string ByteArray::toHexString() const {
	std::string		  str = toString();
	std::stringstream ss;

	for(size_t i = 0; i < str.size(); ++i) {
		if(i > 0 && i % 32 == 0) {
			ss << std::endl;
		}
		ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(static_cast<uint8_t>(str[i]))
		   << " ";
	}

	return ss.str();
}

// ======================================================================
// iovec helpers
// ======================================================================

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
	len = len > getReadSize() ? getReadSize() : len;
	if(len == 0) {
		return 0;
	}

	uint64_t size	  = len;

	size_t nodeOffset = _position % _baseSize;
	size_t nodeAvail  = _cur->size - nodeOffset;
	iovec  iov;
	Node*  cur = _cur;

	while(len > 0) {
		if(nodeAvail >= len) {
			iov.iov_base = cur->ptr + nodeOffset;
			iov.iov_len	 = len;
			len			 = 0;
		} else {
			iov.iov_base = cur->ptr + nodeOffset;
			iov.iov_len	 = nodeAvail;
			len -= nodeAvail;
			cur		   = cur->next;
			nodeAvail  = cur->size;
			nodeOffset = 0;
		}
		buffers.push_back(iov);
	}
	return size;
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const {
	len = len > getReadSize() ? getReadSize() : len;
	if(len == 0) {
		return 0;
	}

	uint64_t size	  = len;

	size_t nodeOffset = position % _baseSize;
	size_t count	  = position / _baseSize;
	Node*  cur		  = _root;
	while(count > 0) {
		cur = cur->next;
		--count;
	}

	size_t nodeAvail = cur->size - nodeOffset;
	iovec  iov;
	while(len > 0) {
		if(nodeAvail >= len) {
			iov.iov_base = cur->ptr + nodeOffset;
			iov.iov_len	 = len;
			len			 = 0;
		} else {
			iov.iov_base = cur->ptr + nodeOffset;
			iov.iov_len	 = nodeAvail;
			len -= nodeAvail;
			cur		   = cur->next;
			nodeAvail  = cur->size;
			nodeOffset = 0;
		}
		buffers.push_back(iov);
	}
	return size;
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
	if(len == 0) {
		return 0;
	}
	addCapacity(len);
	uint64_t size	  = len;

	size_t nodeOffset = _position % _baseSize;
	size_t nodeAvail  = _cur->size - nodeOffset;
	iovec  iov;
	Node*  cur = _cur;
	while(len > 0) {
		if(nodeAvail >= len) {
			iov.iov_base = cur->ptr + nodeOffset;
			iov.iov_len	 = len;
			len			 = 0;
		} else {
			iov.iov_base = cur->ptr + nodeOffset;
			iov.iov_len	 = nodeAvail;

			len -= nodeAvail;
			cur		   = cur->next;
			nodeAvail  = cur->size;
			nodeOffset = 0;
		}
		buffers.push_back(iov);
	}
	return size;
}

}  // namespace azzato
