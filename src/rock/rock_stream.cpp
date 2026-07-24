#include "rock/rock_stream.h"
#include "log.h"
#include "utils/config.h"
#include "worker.h"

namespace azzato {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");
static azzato::ConfigVar<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>::ptr
	g_rock_services = azzato::Config::lookup(
		"rock_services",
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>>(),
		"rock_services");

// static azzato::ConfigVar<std::unordered_map<std::string
//     ,std::unordered_map<std::string, std::string> > >::ptr g_rock_services =
//     azzato::Config::lookup("rock_services", std::unordered_map<std::string
//     ,std::unordered_map<std::string, std::string> >(), "rock_services");

std::string RockResult::toString() const {
	std::stringstream ss;
	ss << "[RockResult result=" << result << " used=" << used
	   << " response=" << (response ? response->toString() : "null")
	   << " request=" << (request ? request->toString() : "null") << "]";
	return ss.str();
}

RockStream::RockStream(Socket::ptr sock)
	: AsyncSocketStream(sock, true)
	, _decoder(new RockMessageDecoder) {
	AZZATO_LOG_DEBUG(g_logger) << "RockStream::RockStream " << this << " " << (sock ? sock->toString() : "");
}

RockStream::~RockStream() {
	AZZATO_LOG_DEBUG(g_logger) << "RockStream::~RockStream " << this << " "
							   << (_socket ? _socket->toString() : "");
}

int32_t RockStream::sendMessage(Message::ptr msg) {
	if(isConnected()) {
		RockSendCtx::ptr ctx(new RockSendCtx);
		ctx->msg = msg;
		enqueue(ctx);
		return 1;
	} else {
		return -1;
	}
}

RockResult::ptr RockStream::request(RockRequest::ptr req, uint32_t timeout_ms) {
	if(isConnected()) {
		RockCtx::ptr ctx(new RockCtx);
		ctx->request   = req;
		ctx->sn		   = req->getSn();
		ctx->timeout   = timeout_ms;
		ctx->scheduler = azzato::Scheduler::getThis();
		ctx->fiber	   = azzato::Fiber::getThis();
		addCtx(ctx);
		uint64_t ts = azzato::getCurrentMS();
		ctx->timer	= azzato::IOManager::getThis()->addTimer(
			 timeout_ms, std::bind(&RockStream::onTimeOut, shared_from_this(), ctx));
		enqueue(ctx);
		azzato::Fiber::yieldToHold();
		return std::make_shared<RockResult>(ctx->result, azzato::getCurrentMS() - ts, ctx->response, req);
	} else {
		return std::make_shared<RockResult>(AsyncSocketStream::NOT_CONNECT, 0, nullptr, req);
	}
}

bool RockStream::RockSendCtx::doSend(AsyncSocketStream::ptr stream) {
	return std::dynamic_pointer_cast<RockStream>(stream)->_decoder->serializeTo(stream, msg) > 0;
}

bool RockStream::RockCtx::doSend(AsyncSocketStream::ptr stream) {
	return std::dynamic_pointer_cast<RockStream>(stream)->_decoder->serializeTo(stream, request) > 0;
}

AsyncSocketStream::Ctx::ptr RockStream::doRecv() {
	// AZZATO_LOG_INFO(g_logger) << "doRecv " << this;
	auto msg = _decoder->parseFrom(shared_from_this());
	if(!msg) {
		innerClose();
		return nullptr;
	}

	int type = msg->getType();
	if(type == Message::Response) {
		auto rsp = std::dynamic_pointer_cast<RockResponse>(msg);
		if(!rsp) {
			AZZATO_LOG_WARN(g_logger) << "RockStream doRecv response not RockResponse: " << msg->toString();
			return nullptr;
		}
		RockCtx::ptr ctx = getAndDelCtxAs<RockCtx>(rsp->getSn());
		if(!ctx) {
			AZZATO_LOG_WARN(g_logger) << "RockStream request timeout reponse=" << rsp->toString();
			return nullptr;
		}
		ctx->result	  = rsp->getResult();
		ctx->response = rsp;
		return ctx;
	} else if(type == Message::Request) {
		auto req = std::dynamic_pointer_cast<RockRequest>(msg);
		if(!req) {
			AZZATO_LOG_WARN(g_logger) << "RockStream doRecv request not RockRequest: " << msg->toString();
			return nullptr;
		}
		if(_requestHandler) {
			_worker->schedule(std::bind(
				&RockStream::handleRequest, std::dynamic_pointer_cast<RockStream>(shared_from_this()), req));
		} else {
			AZZATO_LOG_WARN(g_logger) << "unhandle request " << req->toString();
		}
	} else if(type == Message::Notify) {
		auto nty = std::dynamic_pointer_cast<RockNotify>(msg);
		if(!nty) {
			AZZATO_LOG_WARN(g_logger) << "RockStream doRecv notify not RockNotify: " << msg->toString();
			return nullptr;
		}

		if(_notifyHandler) {
			_worker->schedule(std::bind(
				&RockStream::handleNotify, std::dynamic_pointer_cast<RockStream>(shared_from_this()), nty));
		} else {
			AZZATO_LOG_WARN(g_logger) << "unhandle notify " << nty->toString();
		}
	} else {
		AZZATO_LOG_WARN(g_logger) << "RockStream recv unknow type=" << type << " msg: " << msg->toString();
	}
	return nullptr;
}

void RockStream::handleRequest(azzato::RockRequest::ptr req) {
	azzato::RockResponse::ptr rsp = req->createResponse();
	if(!_requestHandler(req, rsp, std::dynamic_pointer_cast<RockStream>(shared_from_this()))) {
		sendMessage(rsp);
		// innerClose();
		close();
	} else {
		sendMessage(rsp);
	}
}

void RockStream::handleNotify(azzato::RockNotify::ptr nty) {
	if(!_notifyHandler(nty, std::dynamic_pointer_cast<RockStream>(shared_from_this()))) {
		// innerClose();
		close();
	}
}

RockSession::RockSession(Socket::ptr sock)
	: RockStream(sock) {
	_autoConnect = false;
}

RockConnection::RockConnection()
	: RockStream(nullptr) {
	_autoConnect = true;
}

bool RockConnection::connect(azzato::Address::ptr addr) {
	_socket = azzato::Socket::createTcp(addr);
	return _socket->connect(addr);
}

RockSDLoadBalance::RockSDLoadBalance(IServiceDiscovery::ptr sd)
	: SDLoadBalance(sd) {}

static SocketStream::ptr create_rock_stream(ServiceItemInfo::ptr info) {
	azzato::IPAddress::ptr addr = azzato::Address::lookupAnyIPAddress(info->getIp());
	if(!addr) {
		AZZATO_LOG_ERROR(g_logger) << "invalid service info: " << info->toString();
		return nullptr;
	}
	addr->setPort(info->getPort());

	RockConnection::ptr conn(new RockConnection);

	azzato::WorkerMgr::getInstance()->schedule("service_io", [conn, addr]() {
		conn->connect(addr);
		conn->start();
	});
	return conn;
}

void RockSDLoadBalance::start() {
	_cb = create_rock_stream;
	initConf(g_rock_services->getValue());
	SDLoadBalance::start();
}

void RockSDLoadBalance::start(
	const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& confs) {
	_cb = create_rock_stream;
	initConf(confs);
	SDLoadBalance::start();
}

void RockSDLoadBalance::stop() { SDLoadBalance::stop(); }

RockResult::ptr RockSDLoadBalance::request(const std::string& domain,
										   const std::string& service,
										   RockRequest::ptr	  req,
										   uint32_t			  timeout_ms,
										   uint64_t			  idx) {
	auto lb = get(domain, service);
	if(!lb) {
		return std::make_shared<RockResult>(ILoadBalance::NO_SERVICE, 0, nullptr, req);
	}
	auto conn = lb->get(idx);
	if(!conn) {
		return std::make_shared<RockResult>(ILoadBalance::NO_CONNECTION, 0, nullptr, req);
	}
	uint64_t ts	   = azzato::getCurrentMS();
	auto&	 stats = conn->get(ts / 1000);
	stats.incDoing(1);
	stats.incTotal(1);
	auto	 r	 = conn->getStreamAs<RockStream>()->request(req, timeout_ms);
	uint64_t ts2 = azzato::getCurrentMS();
	if(r->result == 0) {
		stats.incOks(1);
		stats.incUsedTime(ts2 - ts);
	} else if(r->result == AsyncSocketStream::TIMEOUT) {
		stats.incTimeouts(1);
	} else if(r->result < 0) {
		stats.incErrs(1);
	}
	stats.decDoing(1);
	return r;
}

}  // namespace azzato
