#pragma once

#include "http/http.h"
#include "mutex.h"
#include "streams/socket_stream.h"
#include "uri.h"
#include <atomic>
#include <list>
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
		PoolGetConnection = 8,
		PoolInvalidConn	  = 9,
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

	static HttpResult::ptr doRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeoutMs);

	HttpConnection(Socket::ptr sock, bool owner = true);

	~HttpConnection();

	HttpResponse::ptr recvResponse();

	int sendRequest(HttpRequest::ptr req);

	bool isConnected() const { return SocketStream::isConnected(); }

	int getSocket() const { return _socket->getSocket(); }

  private:
	friend class HttpConnectionPool;

  private:
	uint64_t _createTime = 0;
	uint64_t _request	 = 0;
};

class HttpConnectionPool {
  public:
	using ptr		= std::shared_ptr<HttpConnectionPool>;
	using MutexType = Mutex;

	static HttpConnectionPool::ptr create(const std::string& uri,
										  const std::string& vhost,
										  uint32_t			 maxSize,
										  uint32_t			 maxAliveTime,
										  uint32_t			 maxRequest);

	HttpConnectionPool(const std::string& host,
					   const std::string& vhost,
					   uint32_t			  port,
					   bool				  isHttps,
					   uint32_t			  maxSize,
					   uint32_t			  maxAliveTime,
					   uint32_t			  maxRequest);

	HttpConnection::ptr getConnection();

	HttpResult::ptr doGet(const std::string&						url,
						  uint64_t									timeoutMs,
						  const std::map<std::string, std::string>& headers = {},
						  const std::string&						body	= "");

	HttpResult::ptr doGet(Uri::ptr									uri,
						  uint64_t									timeoutMs,
						  const std::map<std::string, std::string>& headers = {},
						  const std::string&						body	= "");

	HttpResult::ptr doPost(const std::string&						 url,
						   uint64_t									 timeoutMs,
						   const std::map<std::string, std::string>& headers = {},
						   const std::string&						 body	 = "");

	HttpResult::ptr doPost(Uri::ptr									 uri,
						   uint64_t									 timeoutMs,
						   const std::map<std::string, std::string>& headers = {},
						   const std::string&						 body	 = "");

	HttpResult::ptr doRequest(HttpMethod								method,
							  const std::string&						url,
							  uint64_t									timeoutMs,
							  const std::map<std::string, std::string>& headers = {},
							  const std::string&						body	= "");

	HttpResult::ptr doRequest(HttpMethod								method,
							  Uri::ptr									uri,
							  uint64_t									timeoutMs,
							  const std::map<std::string, std::string>& headers = {},
							  const std::string&						body	= "");

	HttpResult::ptr doRequest(HttpRequest::ptr req, uint64_t timeoutMs);

  private:
	static void releasePtr(HttpConnection* ptr, HttpConnectionPool* pool);

  private:
	std::string _host;
	std::string _vhost;
	uint32_t	_port;
	uint32_t	_maxSize;
	uint32_t	_maxAliveTime;
	uint32_t	_maxRequest;
	bool		_isHttps;

	MutexType				   _mutex;
	std::list<HttpConnection*> _conns;
	std::atomic<int32_t>	   _total{0};
};

}  // namespace http
}  // namespace azzato
