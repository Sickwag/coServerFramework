#pragma once

#include "datastruct/bytearray.h"
#include "stream.h"
#include <memory>
#include <string>

namespace azzato {

class Message {
  public:
	using ptr = std::shared_ptr<Message>;

	enum MessageType {
		Request	 = 1,
		Response = 2,
		Notify	 = 3
	};

	virtual ~Message() = default;

	virtual ByteArray::ptr toByteArray();

	virtual bool serializeToByteArray(ByteArray::ptr bytearray) = 0;

	virtual bool parseFromByteArray(ByteArray::ptr bytearray)	= 0;

	virtual std::string		   toString() const					= 0;
	virtual const std::string& getName() const					= 0;
	virtual int32_t			   getType() const					= 0;
};

class MessageDecoder {
  public:
	using ptr														  = std::shared_ptr<MessageDecoder>;

	virtual ~MessageDecoder()										  = default;

	virtual Message::ptr parseFrom(Stream::ptr stream)				  = 0;

	virtual int32_t serializeTo(Stream::ptr stream, Message::ptr msg) = 0;
};

class Request : public Message {
  public:
	using ptr = std::shared_ptr<Request>;

	Request();

	uint32_t getSn() const { return _sn; }

	uint32_t getCmd() const { return _cmd; }

	void setSn(uint32_t v) { _sn = v; }

	void setCmd(uint32_t v) { _cmd = v; }

	bool serializeToByteArray(ByteArray::ptr bytearray) override;

	bool parseFromByteArray(ByteArray::ptr bytearray) override;

	std::string		   toString() const override;
	const std::string& getName() const override;
	int32_t			   getType() const override;

  protected:
	uint32_t _sn;
	uint32_t _cmd;
};

class Response : public Message {
  public:
	using ptr = std::shared_ptr<Response>;

	Response();

	uint32_t getSn() const { return _sn; }

	uint32_t getCmd() const { return _cmd; }

	uint32_t getResult() const { return _result; }

	const std::string& getResultStr() const { return _resultStr; }

	void setSn(uint32_t v) { _sn = v; }

	void setCmd(uint32_t v) { _cmd = v; }

	void setResult(uint32_t v) { _result = v; }

	void setResultStr(const std::string& v) { _resultStr = v; }

	bool serializeToByteArray(ByteArray::ptr bytearray) override;

	bool parseFromByteArray(ByteArray::ptr bytearray) override;

	std::string		   toString() const override;
	const std::string& getName() const override;
	int32_t			   getType() const override;

  protected:
	uint32_t	_sn;
	uint32_t	_cmd;
	uint32_t	_result;
	std::string _resultStr;
};

class Notify : public Message {
  public:
	using ptr = std::shared_ptr<Notify>;

	Notify();

	uint32_t getNotify() const { return _notify; }

	void setNotify(uint32_t v) { _notify = v; }

	bool serializeToByteArray(ByteArray::ptr bytearray) override;

	bool parseFromByteArray(ByteArray::ptr bytearray) override;

	std::string		   toString() const override;
	const std::string& getName() const override;
	int32_t			   getType() const override;

  protected:
	uint32_t _notify;
};

}  // namespace azzato
