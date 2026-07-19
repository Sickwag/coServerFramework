#include "streams/zlib_stream.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>
#include <string>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testRoundTrip() {
	const std::string data = "hello zlib stream, compress and decompress me!";
	std::string compressed;
	{
		auto compressor = azzato::ZlibStream::createGzip(true);
		assert(compressor);
		assert(compressor->write(data.data(), data.size()) >= 0);
		compressor->close();	// flush
		compressed = compressor->getResult();
	}
	assert(!compressed.empty());
	AZZATO_LOG_INFO(g_logger) << "compressed size=" << compressed.size() << " (orig " << data.size() << ")";

	std::string decompressed;
	{
		auto decompressor = azzato::ZlibStream::createGzip(false);
		assert(decompressor);
		assert(decompressor->write(compressed.data(), compressed.size()) >= 0);
		decompressor->close();
		decompressed = decompressor->getResult();
	}

	assert(decompressed == data);
	AZZATO_LOG_INFO(g_logger) << "round trip ok";
}

void testDeflate() {
	const std::string data = "deflate round trip payload";
	auto			 compressor = azzato::ZlibStream::createDeflate(true);
	compressor->write(data.data(), data.size());
	compressor->close();
	auto compressed = compressor->getResult();

	auto decompressor = azzato::ZlibStream::createDeflate(false);
	decompressor->write(compressed.data(), compressed.size());
	decompressor->close();
	assert(decompressor->getResult() == data);
	AZZATO_LOG_INFO(g_logger) << "deflate round trip ok";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_zlib_stream begin";
	testRoundTrip();
	testDeflate();
	AZZATO_LOG_INFO(g_logger) << "test_zlib_stream over";
	return 0;
}
