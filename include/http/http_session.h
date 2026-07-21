#pragma once

#include "http/http.h"
#include "streams/socket_stream.h"

namespace azzato {
namespace http {

class HttpSession : public SocketStream {
  public:
	using ptr = std::shared_ptr<HttpSession>;

	HttpSession(Socket::ptr sock, bool owner = true);

	HttpRequest::ptr recvRequest();

	int sendResponse(HttpResponse::ptr rsp);
};

}  // namespace http
}  // namespace azzato
