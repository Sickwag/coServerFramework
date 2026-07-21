#include "http/ws_session.h"
#include "utils/config.h"
#include "utils/endian.h"
#include "utils/hash_util.h"
#include "utils/macro.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace azzato {
namespace http {

namespace {
ConfigVar<uint32_t>::ptr g_websocketMessageMaxSize =
	Config::lookup("websocket.message.max_size", static_cast<uint32_t>(1024 * 1024 * 32), "websocket message max size");
}

WSSession::WSSession(Socket::ptr sock, bool owner)
	: HttpSession(std::move(sock), owner) {}

HttpRequest::ptr WSSession::handleShake() {
	HttpRequest::ptr req;
	do {
		req = recvRequest();
		if(!req) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "invalid http request";
			break;
		}
		if(strcasecmp(req->getHeader("Upgrade").c_str(), "websocket")) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "http header Upgrade != websocket";
			break;
		}
		if(strcasecmp(req->getHeader("Connection").c_str(), "Upgrade")) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "http header Connection != Upgrade";
			break;
		}
		if(req->getHeaderAs<int>("Sec-WebSocket-Version") != 13) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "http header Sec-WebSocket-Version != 13";
			break;
		}
		std::string key = req->getHeader("Sec-WebSocket-Key");
		if(key.empty()) {
			AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "http header Sec-WebSocket-Key = null";
			break;
		}

		std::string v = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		v			  = base64encode(sha1sum(v));
		req->setWebsocket(true);

		auto rsp = req->createResponse();
		rsp->setStatus(HttpStatus::SwitchingProtocols);
		rsp->setWebsocket(true);
		rsp->setReason("Web Socket Protocol Handshake");
		rsp->setHeader("Upgrade", "websocket");
		rsp->setHeader("Connection", "Upgrade");
		rsp->setHeader("Sec-WebSocket-Accept", v);

		sendResponse(rsp);
		return req;
	} while(false);
	return nullptr;
}

WSFrameMessage::WSFrameMessage(int opcode, const std::string& data)
	: _opcode(opcode)
	, _data(data) {}

std::string WSFrameHead::toString() const {
	std::stringstream ss;
	ss << "[WSFrameHead fin=" << fin << " rsv1=" << rsv1 << " rsv2=" << rsv2 << " rsv3=" << rsv3
	   << " opcode=" << opcode << " mask=" << mask << " payload=" << payload << "]";
	return ss.str();
}

WSFrameMessage::ptr WSSession::recvMessage() { return wsRecvMessage(this, false); }

int32_t WSSession::sendMessage(WSFrameMessage::ptr msg, bool fin) {
	return wsSendMessage(this, std::move(msg), false, fin);
}

int32_t WSSession::sendMessage(const std::string& msg, int32_t opcode, bool fin) {
	return wsSendMessage(this, std::make_shared<WSFrameMessage>(opcode, msg), false, fin);
}

int32_t WSSession::ping() { return wsPing(this); }

int32_t WSSession::pong() { return wsPong(this); }

WSFrameMessage::ptr wsRecvMessage(Stream* stream, bool client) {
	int			opcode = 0;
	std::string data;
	int			currentLength = 0;
	do {
		WSFrameHead wsHead;
		if(stream->readFixSize(&wsHead, sizeof(wsHead)) <= 0) {
			break;
		}

		if(wsHead.opcode == WSFrameHead::Ping) {
			if(wsPong(stream) <= 0) {
				break;
			}
		} else if(wsHead.opcode == WSFrameHead::Pong) {
			// ignore
		} else if(wsHead.opcode == WSFrameHead::Continue || wsHead.opcode == WSFrameHead::TextFrame
				  || wsHead.opcode == WSFrameHead::BinFrame) {
			if(!client && !wsHead.mask) {
				AZZATO_LOG_INFO(AZZATO_LOG_ROOT()) << "WSFrameHead mask != 1";
				break;
			}
			uint64_t length = 0;
			if(wsHead.payload == 126) {
				uint16_t len = 0;
				if(stream->readFixSize(&len, sizeof(len)) <= 0) {
					break;
				}
				length = byteswapOnLittleEndian(len);
			} else if(wsHead.payload == 127) {
				uint64_t len = 0;
				if(stream->readFixSize(&len, sizeof(len)) <= 0) {
					break;
				}
				length = byteswapOnLittleEndian(len);
			} else {
				length = wsHead.payload;
			}

			if((static_cast<uint64_t>(currentLength) + length) >= g_websocketMessageMaxSize->getValue()) {
				AZZATO_LOG_WARN(AZZATO_LOG_ROOT())
					<< "WSFrameMessage length > " << g_websocketMessageMaxSize->getValue() << " ("
					<< (currentLength + length) << ")";
				break;
			}

			char mask[4] = {0};
			if(wsHead.mask) {
				if(stream->readFixSize(mask, sizeof(mask)) <= 0) {
					break;
				}
			}
			data.resize(static_cast<size_t>(currentLength) + static_cast<size_t>(length));
			if(stream->readFixSize(&data[static_cast<size_t>(currentLength)], static_cast<size_t>(length)) <= 0) {
				break;
			}
			if(wsHead.mask) {
				for(uint64_t i = 0; i < length; ++i) {
					data[static_cast<size_t>(currentLength + i)] ^= mask[i % 4];
				}
			}
			currentLength += static_cast<int>(length);

			if(!opcode && wsHead.opcode != WSFrameHead::Continue) {
				opcode = wsHead.opcode;
			}

			if(wsHead.fin) {
				return WSFrameMessage::ptr(new WSFrameMessage(opcode, std::move(data)));
			}
		} else {
			AZZATO_LOG_DEBUG(AZZATO_LOG_ROOT()) << "invalid opcode=" << wsHead.opcode;
		}
	} while(true);
	stream->close();
	return nullptr;
}

int32_t wsSendMessage(Stream* stream, WSFrameMessage::ptr msg, bool client, bool fin) {
	do {
		WSFrameHead wsHead;
		std::memset(&wsHead, 0, sizeof(wsHead));
		wsHead.fin	= fin;
		wsHead.opcode = static_cast<uint32_t>(msg->getOpcode());
		wsHead.mask	= client;
		uint64_t size = msg->getData().size();
		if(size < 126) {
			wsHead.payload = static_cast<uint32_t>(size);
		} else if(size < 65536) {
			wsHead.payload = 126;
		} else {
			wsHead.payload = 127;
		}

		if(stream->writeFixSize(&wsHead, sizeof(wsHead)) <= 0) {
			break;
		}
		if(wsHead.payload == 126) {
			uint16_t len = byteswapOnLittleEndian(static_cast<uint16_t>(size));
			if(stream->writeFixSize(&len, sizeof(len)) <= 0) {
				break;
			}
		} else if(wsHead.payload == 127) {
			uint64_t len = byteswapOnLittleEndian(size);
			if(stream->writeFixSize(&len, sizeof(len)) <= 0) {
				break;
			}
		}
		if(client) {
			char	   mask[4];
			uint32_t   randValue = static_cast<uint32_t>(std::rand());
			std::memcpy(mask, &randValue, sizeof(mask));
			std::string& payload = msg->getData();
			for(size_t i = 0; i < payload.size(); ++i) {
				payload[i] ^= mask[i % 4];
			}

			if(stream->writeFixSize(mask, sizeof(mask)) <= 0) {
				break;
			}
		}
		if(stream->writeFixSize(msg->getData().c_str(), size) <= 0) {
			break;
		}
		return static_cast<int32_t>(size + sizeof(wsHead));
	} while(false);
	stream->close();
	return -1;
}

int32_t wsPing(Stream* stream) {
	WSFrameHead wsHead;
	std::memset(&wsHead, 0, sizeof(wsHead));
	wsHead.fin	= 1;
	wsHead.opcode = WSFrameHead::Ping;
	int32_t v	= stream->writeFixSize(&wsHead, sizeof(wsHead));
	if(v <= 0) {
		stream->close();
	}
	return v;
}

int32_t wsPong(Stream* stream) {
	WSFrameHead wsHead;
	std::memset(&wsHead, 0, sizeof(wsHead));
	wsHead.fin	= 1;
	wsHead.opcode = WSFrameHead::Pong;
	int32_t v	= stream->writeFixSize(&wsHead, sizeof(wsHead));
	if(v <= 0) {
		stream->close();
	}
	return v;
}

}  // namespace http
}  // namespace azzato
