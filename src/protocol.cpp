#include "protocol.h"
#include <sstream>

namespace azzato {

ByteArray::ptr Message::toByteArray() {
	ByteArray::ptr ba(new ByteArray);
	if(serializeToByteArray(ba)) {
		return ba;
	}
	return nullptr;
}

Request::Request()
	: _sn(0)
	, _cmd(0) {}

bool Request::serializeToByteArray(ByteArray::ptr bytearray) {
	bytearray->write<uint32_t>(_sn);
	bytearray->write<uint32_t>(_cmd);
	return true;
}

bool Request::parseFromByteArray(ByteArray::ptr bytearray) {
	bytearray->setPosition(0);
	_sn	 = bytearray->read<uint32_t>();
	_cmd = bytearray->read<uint32_t>();
	return true;
}

Response::Response()
	: _sn(0)
	, _cmd(0)
	, _result(404)
	, _resultStr("unhandle") {}

bool Response::serializeToByteArray(ByteArray::ptr bytearray) {
	bytearray->write<uint32_t>(_sn);
	bytearray->write<uint32_t>(_cmd);
	bytearray->write<uint32_t>(_result);
	bytearray->write<std::string, ByteArray::ByteSize::Varint>(_resultStr);
	return true;
}

bool Response::parseFromByteArray(ByteArray::ptr bytearray) {
	bytearray->setPosition(0);
	_sn		   = bytearray->read<uint32_t>();
	_cmd	   = bytearray->read<uint32_t>();
	_result	   = bytearray->read<uint32_t>();
	_resultStr = bytearray->read<std::string, ByteArray::ByteSize::Varint>();
	return true;
}

Notify::Notify()
	: _notify(0) {}

bool Notify::serializeToByteArray(ByteArray::ptr bytearray) {
	bytearray->write<uint32_t>(_notify);
	return true;
}

bool Notify::parseFromByteArray(ByteArray::ptr bytearray) {
	bytearray->setPosition(0);
	_notify = bytearray->read<uint32_t>();
	return true;
}

std::string Request::toString() const {
	std::stringstream ss;
	ss << "[Request sn=" << _sn << " cmd=" << _cmd << "]";
	return ss.str();
}

const std::string& Request::getName() const {
	static const std::string s_name = "Request";
	return s_name;
}

int32_t Request::getType() const { return Message::Request; }

std::string Response::toString() const {
	std::stringstream ss;
	ss << "[Response sn=" << _sn << " cmd=" << _cmd << " result=" << _result << " resultStr=" << _resultStr
	   << "]";
	return ss.str();
}

const std::string& Response::getName() const {
	static const std::string s_name = "Response";
	return s_name;
}

int32_t Response::getType() const { return Message::Response; }

std::string Notify::toString() const {
	std::stringstream ss;
	ss << "[Notify notify=" << _notify << "]";
	return ss.str();
}

const std::string& Notify::getName() const {
	static const std::string s_name = "Notify";
	return s_name;
}

int32_t Notify::getType() const { return Message::Notify; }

}  // namespace azzato
