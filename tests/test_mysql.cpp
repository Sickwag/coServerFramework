#include "db/mysql.h"
#include "log.h"
#include "utils/macro.h"

#include <cassert>
#include <map>
#include <string>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testMysql() {
	std::map<std::string, std::string> params;
	params["host"]	 = "127.0.0.1";
	params["port"]	 = "3306";
	params["user"]	 = "root";
	params["passwd"] = "";
	params["dbname"] = "test_db";

	azzato::MySQL::ptr mysql(new azzato::MySQL(params));
	if(!mysql->connect()) {
		AZZATO_LOG_ERROR(g_logger) << "connect fail: " << mysql->getErrStr();
		assert(false);
		return;
	}
	AZZATO_LOG_INFO(g_logger) << "mysql connect ok";

	// create table
	assert(mysql->execute("DROP TABLE IF EXISTS test_mysql_t") == 0);
	assert(mysql->execute("CREATE TABLE test_mysql_t (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(64))")
		   == 0);

	// insert via prepared statement
	auto stmt = azzato::MySQLStmt::create(mysql, "INSERT INTO test_mysql_t (name) VALUES (?)");
	assert(stmt);
	assert(stmt->bindString(1, "hello") == 0);
	assert(stmt->execute() == 0);
	assert(stmt->bindString(1, "world") == 0);
	assert(stmt->execute() == 0);

	// query and verify
	auto res = mysql->query("SELECT id, name FROM test_mysql_t ORDER BY id");
	assert(res);
	assert(res->getErrno() == 0);
	assert(res->getDataCount() == 2);
	int count = 0;
	while(res->next()) {
		AZZATO_LOG_INFO(g_logger) << "row id=" << res->getInt32(0) << " name=" << res->getString(1);
		++count;
	}
	assert(count == 2);

	mysql->execute("DROP TABLE test_mysql_t");
	AZZATO_LOG_INFO(g_logger) << "mysql test over";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_mysql begin";
	testMysql();
	AZZATO_LOG_INFO(g_logger) << "test_mysql over";
	return 0;
}
