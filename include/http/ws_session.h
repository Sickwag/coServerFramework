#pragma once

#include "http/http_session.h"
#include <cstdint>
#include <memory>
#include <string>

namespace azzato {
namespace http {

#pragma pack(1)

struct WSFrameHead {
	enum Opcode {
		Continue  = 0,
		TextFrame = 1,
		BinFrame  = 2,
		Close	  = 8,
		Ping	  = 0x9,
		Pong	  = 0xA,
	};

	uint32_t opcode : 4;
	bool	 rsv3 : 1;
	bool	 rsv2 : 1;
	bool	 rsv1 : 1;
	bool	 fin : 1;
	uint32_t payload : 7;
	bool	 mask : 1;

	std::string toString() const;
};

#pragma pack()

class WSFrameMessage {
  public:
	using ptr = std::shared_ptr<WSFrameMessage>;

	WSFrameMessage(int opcode = 0, const std::string& data = "");

	int getOpcode() const { return _opcode; }

	void setOpcode(int value) { _opcode = value; }

	const std::string& getData() const { return _data; }

	std::string& getData() { return _data; }

	void setData(const std::string& value) { _data = value; }

  private:
	int			_opcode;
	std::string _data;
};

class WSSession : public HttpSession {
  public:
	using ptr = std::shared_ptr<WSSession>;

	WSSession(Socket::ptr sock, bool owner = true);

	HttpRequest::ptr handleShake();

	WSFrameMessage::ptr recvMessage();

	int32_t sendMessage(WSFrameMessage::ptr msg, bool fin = true);

	int32_t sendMessage(const std::string& msg, int32_t opcode = WSFrameHead::TextFrame, bool fin = true);

	int32_t ping();

	int32_t pong();

  private:
	bool handleServerShake();

	bool handleClientShake();
};

WSFrameMessage::ptr wsRecvMessage(Stream* stream, bool client);

int32_t wsSendMessage(Stream* stream, WSFrameMessage::ptr msg, bool client, bool fin);

int32_t wsPing(Stream* stream);

int32_t wsPong(Stream* stream);

}  // namespace http
}  // namespace azzato
