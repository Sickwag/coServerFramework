#include "db/redis.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testRedis() {
	azzato::Redis::ptr redis(new azzato::Redis());
	if(!redis->connect("127.0.0.1", 6379)) {
		AZZATO_LOG_ERROR(g_logger) << "redis connect fail";
		assert(false);
		return;
	}
	AZZATO_LOG_INFO(g_logger) << "redis connect ok";

	assert(redis->cmd("SET azzato_test hello"));
	assert(redis->cmd("SET azzato_test2 world"));

	auto r1 = redis->cmd("GET azzato_test");
	assert(r1 && r1->type == REDIS_REPLY_STRING && r1->str == std::string("hello"));

	auto r2 = redis->cmd("MGET azzato_test azzato_test2");
	assert(r2 && r2->type == REDIS_REPLY_ARRAY && r2->elements == 2);
	assert(r2->element[0]->str == std::string("hello"));
	assert(r2->element[1]->str == std::string("world"));

	redis->cmd("DEL azzato_test azzato_test2");
	AZZATO_LOG_INFO(g_logger) << "redis test over";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_redis begin";
	testRedis();
	AZZATO_LOG_INFO(g_logger) << "test_redis over";
	return 0;
}
