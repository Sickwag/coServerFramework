#include "datastruct/bytearray.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testInt() {
	azzato::ByteArray ba;
	ba.write<int32_t>(12345);
	ba.write<uint8_t>(7);
	ba.setPosition(0);
	assert(ba.read<int32_t>() == 12345);
	assert(ba.read<uint8_t>() == 7);
	AZZATO_LOG_INFO(g_logger) << "int ok";
}

void testString() {
	azzato::ByteArray ba;
	ba.write<std::string, azzato::ByteArray::ByteSize::U16>("hello");
	ba.write<std::string>("world");
	ba.setPosition(0);
	assert((ba.read<std::string, azzato::ByteArray::ByteSize::U16>() == "hello"));
	assert(ba.read<std::string>() == "world");
	AZZATO_LOG_INFO(g_logger) << "string ok";
}

void testVarint() {
	azzato::ByteArray ba;
	ba.write<uint32_t, azzato::ByteArray::ByteSize::Varint>(300);
	ba.setPosition(0);
	assert((ba.read<uint32_t, azzato::ByteArray::ByteSize::Varint>() == 300));
	AZZATO_LOG_INFO(g_logger) << "varint ok";
}

void testBuffer() {
	azzato::ByteArray ba;
	const char*		  data = "raw buffer";
	ba.write(data, std::strlen(data));
	ba.setPosition(0);
	char buf[32] = {0};
	ba.read(buf, std::strlen(data));
	assert(std::strcmp(buf, data) == 0);
	AZZATO_LOG_INFO(g_logger) << "buffer ok";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_bytearray begin";
	testInt();
	testString();
	testVarint();
	testBuffer();
	AZZATO_LOG_INFO(g_logger) << "test_bytearray over";
	return 0;
}
