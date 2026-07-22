#include "http/http_connection.h"
#include "http/http_parser.h"
#include "socket.h"
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
	: SocketStream(std::move(sock), owner) {}

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

	return parser->getData();
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

}  // namespace http
}  // namespace azzato
