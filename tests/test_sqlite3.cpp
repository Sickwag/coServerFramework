#include "db/sqlite3.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <sqlite3.h>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testSqlite() {
	auto db = azzato::SQLite3::Create(":memory:");
	assert(db);
	assert(db->execute("CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT)") == 0);
	assert(db->execute("INSERT INTO t(name) VALUES('hello')") == 0);

	auto stmt = azzato::SQLite3Stmt::Create(db, "SELECT * FROM t");
	assert(stmt);
	int rt = stmt->step();
	assert(rt == SQLITE_ROW);

	auto data = stmt->query();
	assert(data);
	assert(data->getDataCount() == 2);	// columns id, name
	assert(data->getInt32(0) == 1);
	assert(data->getString(1) == "hello");
	AZZATO_LOG_INFO(g_logger) << "sqlite id=" << data->getInt32(0) << " name=" << data->getString(1);
	AZZATO_LOG_INFO(g_logger) << "sqlite ok";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_sqlite3 begin";
	testSqlite();
	AZZATO_LOG_INFO(g_logger) << "test_sqlite3 over";
	return 0;
}
