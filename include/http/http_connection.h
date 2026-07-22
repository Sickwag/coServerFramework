#pragma once

#include "http/http.h"
#include "streams/socket_stream.h"
#include "uri.h"
#include <map>
#include <memory>
#include <string>

namespace azzato {
namespace http {

struct HttpResult {
	using ptr = std::shared_ptr<HttpResult>;

	enum class Error {
		Ok				  = 0,
		InvalidUrl		  = 1,
		InvalidHost		  = 2,
		ConnectFail		  = 3,
		SendCloseByPeer	  = 4,
		SendSocketError	  = 5,
		Timeout			  = 6,
		CreateSocketError = 7,
	};

	HttpResult(int result, HttpResponse::ptr response, const std::string& error)
		: result(result)
		, response(std::move(response))
		, error(error) {}

	int				  result;
	HttpResponse::ptr response;
	std::string		  error;

	std::string toString() const;
};

class HttpConnection : public SocketStream {
  public:
	using ptr = std::shared_ptr<HttpConnection>;

	static HttpResult::ptr doGet(const std::string&						   url,
								 uint64_t								   timeoutMs,
								 const std::map<std::string, std::string>& headers = {},
								 const std::string&						   body	   = "");

	static HttpResult::ptr doGet(Uri::ptr								   uri,
								 uint64_t								   timeoutMs,
								 const std::map<std::string, std::string>& headers = {},
								 const std::string&						   body	   = "");

	static HttpResult::ptr doPost(const std::string&						url,
								  uint64_t									timeoutMs,
								  const std::map<std::string, std::string>& headers = {},
								  const std::string&						body	= "");

	static HttpResult::ptr doPost(Uri::ptr									uri,
								  uint64_t									timeoutMs,
								  const std::map<std::string, std::string>& headers = {},
								  const std::string&						body	= "");

	static HttpResult::ptr doRequest(HttpMethod								   method,
									 const std::string&						   url,
									 uint64_t								   timeoutMs,
									 const std::map<std::string, std::string>& headers = {},
									 const std::string&						   body	   = "");

	static HttpResult::ptr doRequest(HttpMethod								   method,
									 Uri::ptr								   uri,
									 uint64_t								   timeoutMs,
									 const std::map<std::string, std::string>& headers = {},
									 const std::string&						   body	   = "");

	HttpConnection(Socket::ptr sock, bool owner = true);

	~HttpConnection();

	HttpResponse::ptr recvResponse();

	int sendRequest(HttpRequest::ptr req);

	bool isConnected() const { return SocketStream::isConnected(); }

	int getSocket() const { return _socket->getSocket(); }
};

}  // namespace http
}  // namespace azzato
