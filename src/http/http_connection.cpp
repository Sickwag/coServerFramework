#include "http/http_connection.h"
#include "http/http_parser.h"
#include "socket.h"
#include "streams/zlib_stream.h"
#include "thread.h"
#include "utils/macro.h"

#include <cstring>
#include <sstream>

namespace azzato {
namespace http {

std::string HttpResult::toString() const {
	std::stringstream ss;
	ss << "[HttpResult result=" << result << " error=" << error << "]";
	if(response) {
		ss << std::endl << *response;
	}
	return ss.str();
}

HttpConnection::HttpConnection(Socket::ptr sock, bool owner)
	: SocketStream(std::move(sock), owner) {
	_createTime = getCurrentMS();
}

HttpConnection::~HttpConnection() { SocketStream::close(); }

HttpResponse::ptr HttpConnection::recvResponse() {
	HttpResponseParser::ptr parser(new HttpResponseParser);
	auto					buffer = std::make_unique<char[]>(4096);
	char*					data   = buffer.get();
	int						offset = 0;

	do {
		int len = read(data + offset, 4096 - offset);
		if(len <= 0) {
			return nullptr;
		}
		len += offset;

		size_t nparse = parser->execute(data, static_cast<size_t>(len));
		if(parser->hasError()) {
			return nullptr;
		}
		offset = len - static_cast<int>(nparse);
		if(parser->isFinished()) {
			break;
		}
	} while(true);

	HttpResponse::ptr rsp = parser->getData();
	if(rsp && !rsp->getBody().empty()) {
		std::string contentEncoding = rsp->getHeader("content-encoding");
		if(!contentEncoding.empty()) {
			if(strcasecmp(contentEncoding.c_str(), "gzip") == 0) {
				auto zs = ZlibStream::createGzip(false);
				zs->write(rsp->getBody().c_str(), rsp->getBody().size());
				zs->flush();
				rsp->setBody(zs->getResult());
			} else if(strcasecmp(contentEncoding.c_str(), "deflate") == 0) {
				auto zs = ZlibStream::createDeflate(false);
				zs->write(rsp->getBody().c_str(), rsp->getBody().size());
				zs->flush();
				rsp->setBody(zs->getResult());
			}
		}
	}
	return rsp;
}

int HttpConnection::sendRequest(HttpRequest::ptr req) {
	std::stringstream ss;
	ss << *req;
	std::string data = ss.str();
	return writeFixSize(data.c_str(), data.size());
}

HttpResult::ptr HttpConnection::doRequest(HttpMethod								method,
										  Uri::ptr									uri,
										  uint64_t									timeoutMs,
										  const std::map<std::string, std::string>& headers,
										  const std::string&						body) {
	if(!uri) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::InvalidUrl), nullptr, "invalid url");
	}
	if(uri->getHost().empty()) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::InvalidHost), nullptr, "invalid host");
	}

	Address::ptr addr;
	if(uri->getPort() != 0) {
		addr = IPv4Address::create(uri->getHost().c_str(), static_cast<uint16_t>(uri->getPort()));
	} else {
		addr = Address::lookupAnyIPAddress(uri->getHost());
	}
	if(!addr) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::InvalidHost),
											nullptr,
											"lookup host failed: " + uri->getHost());
	}

	Socket::ptr sock = Socket::createTcp(addr);
	if(!sock) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::CreateSocketError), nullptr, "create socket error");
	}
	if(!sock->connect(addr, timeoutMs)) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::ConnectFail), nullptr, "connect fail: " + addr->toString());
	}
	sock->setRecvTimeout(static_cast<int64_t>(timeoutMs));

	HttpConnection::ptr conn(new HttpConnection(sock));
	HttpRequest::ptr	req(new HttpRequest);
	req->setMethod(method);
	req->setPath(uri->getPath());
	req->setQuery(uri->getQuery());
	req->setFragment(uri->getFragment());
	for(auto& header : headers) {
		req->setHeader(header.first, header.second);
	}
	if(!body.empty()) {
		req->setBody(body);
	}
	if(req->getHeader("Host").empty()) {
		req->setHeader("Host", uri->getHost());
	}

	int rt = conn->sendRequest(req);
	if(rt < 0) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::SendSocketError), nullptr, "send request error");
	}
	auto rsp = conn->recvResponse();
	if(!rsp) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::SendCloseByPeer), nullptr, "recv response error");
	}
	return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::Ok), rsp, "ok");
}

HttpResult::ptr HttpConnection::doRequest(HttpMethod								method,
										  const std::string&						url,
										  uint64_t									timeoutMs,
										  const std::map<std::string, std::string>& headers,
										  const std::string&						body) {
	return doRequest(method, Uri::create(url), timeoutMs, headers, body);
}

HttpResult::ptr HttpConnection::doGet(const std::string&						url,
									  uint64_t									timeoutMs,
									  const std::map<std::string, std::string>& headers,
									  const std::string&						body) {
	return doRequest(HttpMethod::Get, url, timeoutMs, headers, body);
}

HttpResult::ptr HttpConnection::doGet(Uri::ptr									uri,
									  uint64_t									timeoutMs,
									  const std::map<std::string, std::string>& headers,
									  const std::string&						body) {
	return doRequest(HttpMethod::Get, uri, timeoutMs, headers, body);
}

HttpResult::ptr HttpConnection::doPost(const std::string&						 url,
									   uint64_t									 timeoutMs,
									   const std::map<std::string, std::string>& headers,
									   const std::string&						 body) {
	return doRequest(HttpMethod::Post, url, timeoutMs, headers, body);
}

HttpResult::ptr HttpConnection::doPost(Uri::ptr									 uri,
									   uint64_t									 timeoutMs,
									   const std::map<std::string, std::string>& headers,
									   const std::string&						 body) {
	return doRequest(HttpMethod::Post, uri, timeoutMs, headers, body);
}

HttpResult::ptr HttpConnection::doRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeoutMs) {
	bool		 isSsl = uri->getScheme() == "https";
	Address::ptr addr  = uri->createAddress();
	if(!addr) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::InvalidHost), nullptr, "invalid host: " + uri->getHost());
	}
	Socket::ptr sock = isSsl ? SSLSocket::createTcp(addr) : Socket::createTcp(addr);
	if(!sock) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::CreateSocketError),
											nullptr,
											"create socket fail: " + addr->toString()
												+ " errno=" + std::to_string(errno)
												+ " errstr=" + std::string(strerror(errno)));
	}
	if(!sock->connect(addr)) {
		return std::make_shared<HttpResult>(
			static_cast<int>(HttpResult::Error::ConnectFail), nullptr, "connect fail: " + addr->toString());
	}
	sock->setRecvTimeout(static_cast<int64_t>(timeoutMs));
	HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock);
	int					rt	 = conn->sendRequest(req);
	if(rt == 0) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::SendCloseByPeer),
											nullptr,
											"send request closed by peer: " + addr->toString());
	}
	if(rt < 0) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::SendSocketError),
											nullptr,
											"send request socket error errno=" + std::to_string(errno)
												+ " errstr=" + std::string(strerror(errno)));
	}
	auto rsp = conn->recvResponse();
	if(!rsp) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::Timeout),
											nullptr,
											"recv response timeout: " + addr->toString()
												+ " timeout_ms:" + std::to_string(timeoutMs));
	}
	return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::Ok), rsp, "ok");
}

HttpConnectionPool::ptr HttpConnectionPool::create(const std::string& uri,
												   const std::string& vhost,
												   uint32_t			  maxSize,
												   uint32_t			  maxAliveTime,
												   uint32_t			  maxRequest) {
	Uri::ptr turi = Uri::create(uri);
	if(!turi) {
		AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "invalid uri=" << uri;
		return nullptr;
	}
	return std::make_shared<HttpConnectionPool>(turi->getHost(),
												vhost,
												static_cast<uint32_t>(turi->getPort()),
												turi->getScheme() == "https",
												maxSize,
												maxAliveTime,
												maxRequest);
}

HttpConnectionPool::HttpConnectionPool(const std::string& host,
									   const std::string& vhost,
									   uint32_t			  port,
									   bool				  isHttps,
									   uint32_t			  maxSize,
									   uint32_t			  maxAliveTime,
									   uint32_t			  maxRequest)
	: _host(host)
	, _vhost(vhost)
	, _port(port ? port : (isHttps ? 443 : 80))
	, _maxSize(maxSize)
	, _maxAliveTime(maxAliveTime)
	, _maxRequest(maxRequest)
	, _isHttps(isHttps) {}

HttpConnection::ptr HttpConnectionPool::getConnection() {
	uint64_t					 nowMs = getCurrentMS();
	std::vector<HttpConnection*> invalid;
	HttpConnection*				 ptr = nullptr;
	MutexType::Lock				 lock(_mutex);
	while(!_conns.empty()) {
		auto conn = *_conns.begin();
		_conns.pop_front();
		if(!conn->isConnected()) {
			invalid.push_back(conn);
			continue;
		}
		if((conn->_createTime + _maxAliveTime) > nowMs) {
			invalid.push_back(conn);
			continue;
		}
		ptr = conn;
		break;
	}
	lock.unlock();
	for(auto i : invalid) {
		delete i;
	}
	_total -= static_cast<int32_t>(invalid.size());

	if(!ptr) {
		IPAddress::ptr addr = Address::lookupAnyIPAddress(_host);
		if(!addr) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "get addr fail: " << _host;
			return nullptr;
		}
		addr->setPort(static_cast<uint16_t>(_port));
		Socket::ptr sock = _isHttps ? SSLSocket::createTcp(addr) : Socket::createTcp(addr);
		if(!sock) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "create sock fail: " << *addr;
			return nullptr;
		}
		if(!sock->connect(addr)) {
			AZZATO_LOG_ERROR(AZZATO_LOG_ROOT()) << "sock connect fail: " << *addr;
			return nullptr;
		}

		ptr = new HttpConnection(sock);
		++_total;
	}
	return HttpConnection::ptr(ptr, std::bind(&HttpConnectionPool::releasePtr, std::placeholders::_1, this));
}

void HttpConnectionPool::releasePtr(HttpConnection* ptr, HttpConnectionPool* pool) {
	++ptr->_request;
	if(!ptr->isConnected() || ((ptr->_createTime + pool->_maxAliveTime) >= getCurrentMS())
	   || (ptr->_request >= pool->_maxRequest)) {
		delete ptr;
		--pool->_total;
		return;
	}
	MutexType::Lock lock(pool->_mutex);
	pool->_conns.push_back(ptr);
}

HttpResult::ptr HttpConnectionPool::doGet(const std::string&						url,
										  uint64_t									timeoutMs,
										  const std::map<std::string, std::string>& headers,
										  const std::string&						body) {
	return doRequest(HttpMethod::Get, url, timeoutMs, headers, body);
}

HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr									uri,
										  uint64_t									timeoutMs,
										  const std::map<std::string, std::string>& headers,
										  const std::string&						body) {
	std::stringstream ss;
	ss << uri->getPath() << (uri->getQuery().empty() ? "" : "?") << uri->getQuery()
	   << (uri->getFragment().empty() ? "" : "#") << uri->getFragment();
	return doGet(ss.str(), timeoutMs, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(const std::string&						 url,
										   uint64_t									 timeoutMs,
										   const std::map<std::string, std::string>& headers,
										   const std::string&						 body) {
	return doRequest(HttpMethod::Post, url, timeoutMs, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr									 uri,
										   uint64_t									 timeoutMs,
										   const std::map<std::string, std::string>& headers,
										   const std::string&						 body) {
	std::stringstream ss;
	ss << uri->getPath() << (uri->getQuery().empty() ? "" : "?") << uri->getQuery()
	   << (uri->getFragment().empty() ? "" : "#") << uri->getFragment();
	return doPost(ss.str(), timeoutMs, headers, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod								method,
											  const std::string&						url,
											  uint64_t									timeoutMs,
											  const std::map<std::string, std::string>& headers,
											  const std::string&						body) {
	HttpRequest::ptr req = std::make_shared<HttpRequest>();
	req->setPath(url);
	req->setMethod(method);
	req->setClose(false);
	bool hasHost = false;
	for(auto& i : headers) {
		if(strcasecmp(i.first.c_str(), "connection") == 0) {
			if(strcasecmp(i.second.c_str(), "keep-alive") == 0) {
				req->setClose(false);
			}
			continue;
		}

		if(!hasHost && strcasecmp(i.first.c_str(), "host") == 0) {
			hasHost = !i.second.empty();
		}

		req->setHeader(i.first, i.second);
	}
	if(!hasHost) {
		req->setHeader("Host", _vhost.empty() ? _host : _vhost);
	}
	req->setBody(body);
	return doRequest(req, timeoutMs);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod								method,
											  Uri::ptr									uri,
											  uint64_t									timeoutMs,
											  const std::map<std::string, std::string>& headers,
											  const std::string&						body) {
	std::stringstream ss;
	ss << uri->getPath() << (uri->getQuery().empty() ? "" : "?") << uri->getQuery()
	   << (uri->getFragment().empty() ? "" : "#") << uri->getFragment();
	return doRequest(method, ss.str(), timeoutMs, headers, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr req, uint64_t timeoutMs) {
	auto conn = getConnection();
	if(!conn) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::PoolGetConnection),
											nullptr,
											"pool host:" + _host + " port:" + std::to_string(_port));
	}
	auto sock = conn->_socket;
	if(!sock) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::PoolInvalidConn),
											nullptr,
											"pool host:" + _host + " port:" + std::to_string(_port));
	}
	sock->setRecvTimeout(static_cast<int64_t>(timeoutMs));
	int rt = conn->sendRequest(req);
	if(rt == 0) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::SendCloseByPeer),
											nullptr,
											"send request closed by peer: "
												+ sock->getRemoteAddress()->toString());
	}
	if(rt < 0) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::SendSocketError),
											nullptr,
											"send request socket error errno=" + std::to_string(errno)
												+ " errstr=" + std::string(strerror(errno)));
	}
	auto rsp = conn->recvResponse();
	if(!rsp) {
		return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::Timeout),
											nullptr,
											"recv response timeout: " + sock->getRemoteAddress()->toString()
												+ " timeout_ms:" + std::to_string(timeoutMs));
	}
	return std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::Ok), rsp, "ok");
}

}  // namespace http
}  // namespace azzato
