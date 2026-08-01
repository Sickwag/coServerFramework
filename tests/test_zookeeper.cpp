#include "iomanager.h"
#include "log.h"
#include "utils/macro.h"
#include "zk_client.h"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <filesystem>
#include <string>
#include <unistd.h>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_zookeeper begin";

	std::string confPath = std::string(AZZATO_CONF_PATH) + "/services.yml";
	if(!std::filesystem::exists(confPath)) {
		AZZATO_LOG_WARN(g_logger) << "services.yml not found, skip zookeeper test";
		return 0;
	}
	YAML::Node node = YAML::LoadFile(confPath);
	YAML::Node zk	= node["zookeeper"];
	if(!zk) {
		AZZATO_LOG_WARN(g_logger) << "no zookeeper config, skip test";
		return 0;
	}
	std::string hosts = zk["host"].as<std::string>() + ":" + std::to_string(zk["port"].as<int>());
	AZZATO_LOG_INFO(g_logger) << "zk hosts: " << hosts;

	azzato::IOManager iom(1, true, "iomanager");
	bool			  ok = false;
	iom.schedule([&]() {
		auto client = std::make_shared<azzato::ZKClient>();
		if(!client->init(
			   hosts, 10000, [](int type, int stat, const std::string& path, azzato::ZKClient::ptr) {
				   AZZATO_LOG_INFO(g_logger)
					   << "zk watcher type=" << type << " stat=" << stat << " path=" << path;
			   })) {
			AZZATO_LOG_ERROR(g_logger) << "zk init fail";
			return;
		}

		std::string nodePath = "/azzato_test_" + std::to_string(getpid());
		std::string newPath;
		int			rt = client->create(nodePath, "hello", newPath, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL);
		if(rt != ZOK) {
			AZZATO_LOG_ERROR(g_logger) << "zk create error: " << rt;
			client->close();
			return;
		}
		AZZATO_LOG_INFO(g_logger) << "zk create ok: " << newPath;

		std::string val;
		rt = client->get(nodePath, val, false);
		if(rt == ZOK && val == "hello") {
			AZZATO_LOG_INFO(g_logger) << "zk get ok, val=" << val;
			ok = true;
		} else {
			AZZATO_LOG_ERROR(g_logger) << "zk get error: " << rt << " val=" << val;
		}

		client->del(nodePath);
		client->close();
	});
	iom.stop();

	assert(ok);
	AZZATO_LOG_INFO(g_logger) << "test_zookeeper over";
	return 0;
}
