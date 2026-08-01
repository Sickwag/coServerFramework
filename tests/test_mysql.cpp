#include "db/mysql.h"
#include "log.h"
#include "utils/macro.h"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <filesystem>
#include <map>
#include <string>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

std::string servicesConfPath() { return std::string(AZZATO_CONF_PATH) + "/services.yml"; }

bool loadMysqlParams(std::map<std::string, std::string>& params) {
	if(!std::filesystem::exists(servicesConfPath())) {
		return false;
	}
	YAML::Node node = YAML::LoadFile(servicesConfPath());
	YAML::Node m	= node["mysql"];
	if(!m) {
		return false;
	}
	params["host"]	 = m["host"].as<std::string>();
	params["port"]	 = std::to_string(m["port"].as<int>());
	params["user"]	 = m["user"].as<std::string>();
	params["passwd"] = m["passwd"].as<std::string>();
	params["dbname"] = m["dbname"].as<std::string>();
	return true;
}

void testMysql() {
	std::map<std::string, std::string> params;
	if(!loadMysqlParams(params)) {
		AZZATO_LOG_WARN(g_logger) << "services.yml not found, skip mysql test";
		return;
	}

	azzato::MySQL::ptr mysql(new azzato::MySQL(params));
	if(!mysql->connect()) {
		AZZATO_LOG_ERROR(g_logger) << "connect fail: " << mysql->getErrStr();
		assert(false);
		return;
	}
	AZZATO_LOG_INFO(g_logger) << "mysql connect ok: " << params["host"] << ":" << params["port"];

	const std::string table = "azzato_test_t";
	assert(mysql->execute("DROP TABLE IF EXISTS " + table) == 0);
	assert(mysql->execute("CREATE TABLE " + table + " (id INT PRIMARY KEY AUTO_INCREMENT, name VARCHAR(64))")
		   == 0);

	auto stmt = azzato::MySQLStmt::create(mysql, "INSERT INTO " + table + " (name) VALUES (?)");
	assert(stmt);
	assert(stmt->bindString(1, "hello") == 0);
	assert(stmt->execute() == 0);
	assert(stmt->bindString(1, "world") == 0);
	assert(stmt->execute() == 0);

	auto res = mysql->query("SELECT id, name FROM " + table + " ORDER BY id");
	assert(res);
	assert(res->getErrno() == 0);
	int count = 0;
	while(res->next()) {
		AZZATO_LOG_INFO(g_logger) << "row id=" << res->getInt32(0) << " name=" << res->getString(1);
		++count;
	}
	assert(count == 2);

	mysql->execute("DROP TABLE " + table);
	AZZATO_LOG_INFO(g_logger) << "mysql test over";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_mysql begin";
	testMysql();
	AZZATO_LOG_INFO(g_logger) << "test_mysql over";
	return 0;
}
