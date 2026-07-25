#pragma once

#include "http/http_connection.h"
#include "http/ws_session.h"
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace azzato {
namespace http {

class WSConnection : public HttpConnection {
  public:
	using ptr = std::shared_ptr<WSConnection>;

	WSConnection(Socket::ptr sock, bool owner = true);

	static std::pair<HttpResult::ptr, WSConnection::ptr>
	create(const std::string&						 url,
		   uint64_t									 timeoutMs,
		   const std::map<std::string, std::string>& headers = {});

	static std::pair<HttpResult::ptr, WSConnection::ptr>
	create(Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& headers = {});

	WSFrameMessage::ptr recvMessage();

	int32_t sendMessage(WSFrameMessage::ptr msg, bool fin = true);

	int32_t sendMessage(const std::string& msg, int32_t opcode = WSFrameHead::TextFrame, bool fin = true);

	int32_t ping();

	int32_t pong();
};

}  // namespace http
}  // namespace azzato
