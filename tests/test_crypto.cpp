#include "log.h"
#include "utils/crypto_util.h"
#include "utils/hash_util.h"
#include "utils/macro.h"

#include <cassert>
#include <cstring>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testAes() {
	const char key[17] = "0123456789abcdef";
	const char iv[17]  = "fedcba9876543210";
	const char plain[] = "hello crypto";

	uint8_t enc[128]   = {0};
	uint8_t dec[128]   = {0};
	int		len		   = azzato::CryptoUtil::AES128Cbc(key, iv, plain, std::strlen(plain), enc, true);
	assert(len > 0);
	int len2 = azzato::CryptoUtil::AES128Cbc(key, iv, enc, len, dec, false);
	assert(len2 == static_cast<int>(std::strlen(plain)));
	assert(std::memcmp(dec, plain, len2) == 0);
	AZZATO_LOG_INFO(g_logger) << "aes ok";
}

void testHash() {
	std::string md5hex = azzato::md5("hello");
	assert(md5hex.size() == 32);
	AZZATO_LOG_INFO(g_logger) << "md5=" << md5hex;
	assert(md5hex == azzato::md5("hello"));
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_crypto begin";
	testAes();
	testHash();
	AZZATO_LOG_INFO(g_logger) << "test_crypto over";
	return 0;
}
