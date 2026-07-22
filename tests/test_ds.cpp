#include "datastruct/bitmap.h"
#include "datastruct/cache_status.h"
#include "datastruct/roaring_bitmap.h"
#include "datastruct/dict.h"
#include "datastruct/hash_map.h"
#include "datastruct/hash_multimap.h"
#include "datastruct/lru_cache.h"
#include "datastruct/timed_cache.h"
#include "datastruct/timed_lru_cache.h"
#include "datastruct/util.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testHashMap() {
	azzato::HashMap<int, int> m;
	for(int i = 0; i < 100; ++i) {
		m.set(i, i * 2);
	}
	assert(m.getTotal() == 100);
	for(int i = 0; i < 100; ++i) {
		int v = 0;
		assert(m.get(i, v));
		assert(v == i * 2);
	}
	AZZATO_LOG_INFO(g_logger) << "HashMap ok";
}

void testDict() {
	azzato::Dict<int, int> dict;
	int					   values[3] = {1, 2, 3};
	dict.insert(10, values, 3);
	auto arr = dict.get(10);
	assert(arr.size() == 3);
	assert(arr.get()[0] == 1 && arr.get()[2] == 3);
	assert(dict.getTotal() == 1);
	AZZATO_LOG_INFO(g_logger) << "Dict ok";
}

void testLru() {
	azzato::LruCache<int, int> cache(10);
	for(int i = 0; i < 20; ++i) {
		cache.set(i, i * 10);
	}
	int v = 0;
	assert(cache.get(0, v) == false);	// evicted (capacity 10)
	assert(cache.get(19, v));
	assert(v == 190);
	AZZATO_LOG_INFO(g_logger) << "LruCache ok";
}

void testCacheStatus() {
	azzato::CacheStatus status;
	status.incGet();
	status.incHit();
	status.incSet();
	assert(status.getGet() == 1);
	assert(status.getHitRate() == 1.0);
	assert(status.toString().find("get=1") != std::string::npos);
	AZZATO_LOG_INFO(g_logger) << "CacheStatus ok";
}

void testUtil() {
	azzato::PrimeGenerator pg;
	assert(pg.getValue() == 103);
	assert(pg.nextValue() == 211);
	assert(pg.prevValue() == 103);

	std::string s = azzato::RandomStringGenerator::generate(10);
	assert(s.size() == 10);

	azzato::Murmur3Hash<std::string> h;
	assert(h("hello") != 0);
	AZZATO_LOG_INFO(g_logger) << "ds util ok";
}

void testBitmap() {
	azzato::Bitmap bitmap(100);
	for(uint32_t i = 0; i < 100; i += 2) {
		bitmap.set(i, true);
	}
	assert(bitmap.get(0));
	assert(!bitmap.get(1));
	assert(bitmap.getCount() == 50);
	azzato::Bitmap b2(100);
	b2.set(0, 14, true);
	assert(b2.getCount() == 14);
	b2.set(0, 14, false);
	assert(b2.getCount() == 0);
	AZZATO_LOG_INFO(g_logger) << "Bitmap ok";
}

void testRoaring() {
	azzato::RoaringBitmap rb;
	for(int i = 0; i < 100; ++i) {
		rb.set(i, true);
	}
	assert(rb.get(0));
	assert(rb.get(99));
	assert(!rb.get(1000));
	assert(rb.getCount() == 100);
	rb.set(0, false);
	assert(!rb.get(0));
	AZZATO_LOG_INFO(g_logger) << "RoaringBitmap ok";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_ds begin";
	testHashMap();
	testDict();
	testLru();
	testCacheStatus();
	testUtil();
	testBitmap();
	testRoaring();
	AZZATO_LOG_INFO(g_logger) << "test_ds over";
	return 0;
}
