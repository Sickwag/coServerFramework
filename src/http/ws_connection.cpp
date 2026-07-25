#include "http/ws_connection.h"
#include "utils/hash_util.h"

#include <cerrno>
#include <cstring>
#include <strings.h>

namespace azzato {
namespace http {

WSConnection::WSConnection(Socket::ptr sock, bool owner)
	: HttpConnection(std::move(sock), owner) {}

std::pair<HttpResult::ptr, WSConnection::ptr>
WSConnection::create(const std::string&						   url,
					 uint64_t								   timeoutMs,
					 const std::map<std::string, std::string>& headers) {
	Uri::ptr uri = Uri::create(url);
	if(!uri) {
		return std::make_pair(std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::InvalidUrl),
														   nullptr,
														   "invalid url:" + url),
							  nullptr);
	}
	return create(uri, timeoutMs, headers);
}

std::pair<HttpResult::ptr, WSConnection::ptr>
WSConnection::create(Uri::ptr uri, uint64_t timeoutMs, const std::map<std::string, std::string>& headers) {
	Address::ptr addr = uri->createAddress();
	if(!addr) {
		return std::make_pair(std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::InvalidHost),
														   nullptr,
														   "invalid host: " + uri->getHost()),
							  nullptr);
	}
	Socket::ptr sock = Socket::createTcp(addr);
	if(!sock) {
		return std::make_pair(std::make_shared<HttpResult>(
								  static_cast<int>(HttpResult::Error::CreateSocketError),
								  nullptr,
								  "create socket fail: " + addr->toString() + " errno="
									  + std::to_string(errno) + " errstr=" + std::string(strerror(errno))),
							  nullptr);
	}
	if(!sock->connect(addr)) {
		return std::make_pair(std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::ConnectFail),
														   nullptr,
														   "connect fail: " + addr->toString()),
							  nullptr);
	}
	sock->setRecvTimeout(static_cast<int64_t>(timeoutMs));
	WSConnection::ptr conn = std::make_shared<WSConnection>(sock);

	HttpRequest::ptr req   = std::make_shared<HttpRequest>();
	req->setPath(uri->getPath());
	req->setQuery(uri->getQuery());
	req->setFragment(uri->getFragment());
	req->setMethod(HttpMethod::Get);
	bool hasHost = false;
	bool hasConn = false;
	for(auto& i : headers) {
		if(strcasecmp(i.first.c_str(), "connection") == 0) {
			hasConn = true;
		} else if(!hasHost && strcasecmp(i.first.c_str(), "host") == 0) {
			hasHost = !i.second.empty();
		}

		req->setHeader(i.first, i.second);
	}
	req->setWebsocket(true);
	if(!hasConn) {
		req->setHeader("connection", "Upgrade");
	}
	req->setHeader("Upgrade", "websocket");
	req->setHeader("Sec-WebSocket-Version", "13");
	req->setHeader("Sec-WebSocket-Key", base64encode(random_string(16)));
	if(!hasHost) {
		req->setHeader("Host", uri->getHost());
	}

	int rt = conn->sendRequest(req);
	if(rt == 0) {
		return std::make_pair(
			std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::SendCloseByPeer),
										 nullptr,
										 "send request closed by peer: " + addr->toString()),
			nullptr);
	}
	if(rt < 0) {
		return std::make_pair(
			std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::SendSocketError),
										 nullptr,
										 "send request socket error errno=" + std::to_string(errno)
											 + " errstr=" + std::string(strerror(errno))),
			nullptr);
	}
	auto rsp = conn->recvResponse();
	if(!rsp) {
		return std::make_pair(std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::Timeout),
														   nullptr,
														   "recv response timeout: " + addr->toString()
															   + " timeout_ms:" + std::to_string(timeoutMs)),
							  nullptr);
	}

	if(rsp->getStatus() != HttpStatus::SwitchingProtocols) {
		return std::make_pair(
			std::make_shared<HttpResult>(50, rsp, "not websocket server " + addr->toString()), nullptr);
	}
	return std::make_pair(std::make_shared<HttpResult>(static_cast<int>(HttpResult::Error::Ok), rsp, "ok"),
						  conn);
}

WSFrameMessage::ptr WSConnection::recvMessage() { return wsRecvMessage(this, true); }

int32_t WSConnection::sendMessage(WSFrameMessage::ptr msg, bool fin) {
	return wsSendMessage(this, std::move(msg), true, fin);
}

int32_t WSConnection::sendMessage(const std::string& msg, int32_t opcode, bool fin) {
	return wsSendMessage(this, std::make_shared<WSFrameMessage>(opcode, msg), true, fin);
}

int32_t WSConnection::ping() { return wsPing(this); }

int32_t WSConnection::pong() { return wsPong(this); }

}  // namespace http
}  // namespace azzato
