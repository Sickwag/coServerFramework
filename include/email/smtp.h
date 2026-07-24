#pragma once

#include "email/email.h"
#include "streams/socket_stream.h"
#include <sstream>

namespace azzato {

struct SmtpResult {
	typedef std::shared_ptr<SmtpResult> ptr;

	enum Result {
		OK		 = 0,
		IO_ERROR = -1
	};

	SmtpResult(int r, const std::string& m)
		: result(r)
		, msg(m) {}

	int			result;
	std::string msg;
};

class SmtpClient : public azzato::SocketStream {
  public:
	typedef std::shared_ptr<SmtpClient> ptr;
	static SmtpClient::ptr				Create(const std::string& host, uint32_t port, bool ssl = false);
	SmtpResult::ptr						send(EMail::ptr email, int64_t timeout_ms = 1000, bool debug = false);

	std::string getDebugInfo();

  private:
	SmtpResult::ptr doCmd(const std::string& cmd, bool debug);

  private:
	SmtpClient(Socket::ptr sock);

  private:
	std::string		  _host;
	std::stringstream _ss;
	bool			  _authed = false;
};

}  // namespace azzato
