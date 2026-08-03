#include "email/email.h"
#include "email/smtp.h"
#include "iomanager.h"
#include "log.h"
#include "utils/macro.h"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

static azzato::Logger::ptr g_logger = azzato::LoggerMgr::getInstance()->getLogger("system");

void testEmail() {
	std::string confPath = std::string(AZZATO_CONF_PATH) + "/services.yml";
	if(!std::filesystem::exists(confPath)) {
		AZZATO_LOG_WARN(g_logger) << "services.yml not found, skip email test";
		return;
	}
	YAML::Node node = YAML::LoadFile(confPath);
	YAML::Node e	= node["email"];
	if(!e) {
		AZZATO_LOG_WARN(g_logger) << "no email config, skip test";
		return;
	}
	std::string host   = e["host"].as<std::string>();
	int			port   = e["port"].as<int>();
	bool		ssl	   = e["ssl"].as<bool>(true);
	std::string user   = e["user"].as<std::string>();
	std::string passwd = e["passwd"].as<std::string>();

	AZZATO_LOG_INFO(g_logger) << "smtp server: " << host << ":" << port << " ssl=" << ssl;

	// send to self: QQ SMTP only delivers to verified recipient, and the
	// mailbox owner is the only guaranteed-verified address.
	azzato::EMail::ptr email = azzato::EMail::Create(
		user, passwd, "azzato test email", "<h3>hello from azzato framework</h3>", {user});
	assert(email);

	auto client = azzato::SmtpClient::Create(host, port, ssl);
	if(!client) {
		AZZATO_LOG_ERROR(g_logger) << "connect smtp server fail";
		assert(false);
		return;
	}
	AZZATO_LOG_INFO(g_logger) << "smtp connect ok";

	auto result = client->send(email, 1000, true);
	AZZATO_LOG_INFO(g_logger) << "smtp send result=" << result->result << " msg=" << result->msg;
	assert(result->result == azzato::SmtpResult::OK);
	AZZATO_LOG_INFO(g_logger) << "email test over";
}

int main() {
	AZZATO_LOG_INFO(g_logger) << "test_email begin";
	azzato::IOManager iom(1);
	iom.schedule(testEmail);
	iom.stop();
	AZZATO_LOG_INFO(g_logger) << "test_email over";
	return 0;
}
