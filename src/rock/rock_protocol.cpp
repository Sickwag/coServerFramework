#include "rock/rock_protocol.h"
#include "log.h"
#include "streams/zlib_stream.h"
#include "utils/config.h"
#include "utils/endian.h"

namespace azzato {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("system");

static azzato::ConfigVar<uint32_t>::ptr g_rock_protocol_max_length =
	azzato::Config::lookup("rock.protocol.max_length",
						   (uint32_t)(1024 * 1024 * 64),
						   "rock protocol max length");

static azzato::ConfigVar<uint32_t>::ptr g_rock_protocol_gzip_min_length =
	azzato::Config::lookup("rock.protocol.gzip_min_length",
						   (uint32_t)(1024 * 4),
						   "rock protocol gizp min length");

bool RockBody::serializeToByteArray(ByteArray::ptr bytearray) {
	bytearray->write<std::string, ByteArray::ByteSize::Varint>(_body);
	return true;
}

bool RockBody::parseFromByteArray(ByteArray::ptr bytearray) {
	_body = bytearray->read<std::string, ByteArray::ByteSize::Varint>();
	return true;
}

std::shared_ptr<RockResponse> RockRequest::createResponse() {
	RockResponse::ptr rt(new RockResponse);
	rt->setSn(_sn);
	rt->setCmd(_cmd);
	return rt;
}

std::string RockRequest::toString() const {
	std::stringstream ss;
	ss << "[RockRequest sn=" << _sn << " cmd=" << _cmd << " body.length=" << _body.size() << "]";
	return ss.str();
}

const std::string& RockRequest::getName() const {
	static const std::string& s_name = "RockRequest";
	return s_name;
}

int32_t RockRequest::getType() const { return Message::Request; }

bool RockRequest::serializeToByteArray(ByteArray::ptr bytearray) {
	try {
		bool v = true;
		v &= Request::serializeToByteArray(bytearray);
		v &= RockBody::serializeToByteArray(bytearray);
		return v;
	} catch(...) {
		AZZATO_LOG_ERROR(g_logger) << "RockRequest serializeToByteArray error";
	}
	return false;
}

bool RockRequest::parseFromByteArray(ByteArray::ptr bytearray) {
	try {
		bool v = true;
		v &= Request::parseFromByteArray(bytearray);
		v &= RockBody::parseFromByteArray(bytearray);
		return v;
	} catch(...) {
		AZZATO_LOG_ERROR(g_logger) << "RockRequest parseFromByteArray error";
	}
	return false;
}

std::string RockResponse::toString() const {
	std::stringstream ss;
	ss << "[RockResponse sn=" << _sn << " cmd=" << _cmd << " result=" << _result
	   << " result_msg=" << _resultStr << " body.length=" << _body.size() << "]";
	return ss.str();
}

const std::string& RockResponse::getName() const {
	static const std::string& s_name = "RockResponse";
	return s_name;
}

int32_t RockResponse::getType() const { return Message::Response; }

bool RockResponse::serializeToByteArray(ByteArray::ptr bytearray) {
	try {
		bool v = true;
		v &= Response::serializeToByteArray(bytearray);
		v &= RockBody::serializeToByteArray(bytearray);
		return v;
	} catch(...) {
		AZZATO_LOG_ERROR(g_logger) << "RockResponse serializeToByteArray error";
	}
	return false;
}

bool RockResponse::parseFromByteArray(ByteArray::ptr bytearray) {
	try {
		bool v = true;
		v &= Response::parseFromByteArray(bytearray);
		v &= RockBody::parseFromByteArray(bytearray);
		return v;
	} catch(...) {
		AZZATO_LOG_ERROR(g_logger) << "RockResponse parseFromByteArray error";
	}
	return false;
}

std::string RockNotify::toString() const {
	std::stringstream ss;
	ss << "[RockNotify notify=" << _notify << " body.length=" << _body.size() << "]";
	return ss.str();
}

const std::string& RockNotify::getName() const {
	static const std::string& s_name = "RockNotify";
	return s_name;
}

int32_t RockNotify::getType() const { return Message::Notify; }

bool RockNotify::serializeToByteArray(ByteArray::ptr bytearray) {
	try {
		bool v = true;
		v &= Notify::serializeToByteArray(bytearray);
		v &= RockBody::serializeToByteArray(bytearray);
		return v;
	} catch(...) {
		AZZATO_LOG_ERROR(g_logger) << "RockNotify serializeToByteArray error";
	}
	return false;
}

bool RockNotify::parseFromByteArray(ByteArray::ptr bytearray) {
	try {
		bool v = true;
		v &= Notify::parseFromByteArray(bytearray);
		v &= RockBody::parseFromByteArray(bytearray);
		return v;
	} catch(...) {
		AZZATO_LOG_ERROR(g_logger) << "RockNotify parseFromByteArray error";
	}
	return false;
}

static const uint8_t s_rock_magic[2] = {0xab, 0xcd};

RockMsgHeader::RockMsgHeader()
	: magic{0xab, 0xcd}
	, version(1)
	, flag(0)
	, length(0) {}

Message::ptr RockMessageDecoder::parseFrom(Stream::ptr stream) {
	try {
		RockMsgHeader header;
		if(stream->readFixSize(&header, sizeof(header)) <= 0) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder decode head error";
			return nullptr;
		}

		if(memcmp(header.magic, s_rock_magic, sizeof(s_rock_magic))) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder head.magic error";
			return nullptr;
		}

		if(header.version != 0x1) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder head.version != 0x1";
			return nullptr;
		}

		header.length = azzato::byteswapOnLittleEndian(header.length);
		if((uint32_t)header.length >= g_rock_protocol_max_length->getValue()) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder head.length(" << header.length
									   << ") >=" << g_rock_protocol_max_length->getValue();
			return nullptr;
		}
		azzato::ByteArray::ptr ba(new azzato::ByteArray);
		if(stream->readFixSize(ba, header.length) <= 0) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder read body fail length=" << header.length;
			return nullptr;
		}

		ba->setPosition(0);
		if(header.flag & 0x1) {	 // gizp
			auto zstream = azzato::ZlibStream::createGzip(false);
			if(zstream->write(ba, -1) != Z_OK) {
				AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder ungzip error";
				return nullptr;
			}
			if(zstream->flush() != Z_OK) {
				AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder ungzip flush error";
				return nullptr;
			}
			ba = zstream->getByteArray();
		}
		uint8_t		 type = ba->read<uint8_t>();
		Message::ptr msg;
		switch(type) {
		case Message::Request:
			msg.reset(new RockRequest);
			break;
		case Message::Response:
			msg.reset(new RockResponse);
			break;
		case Message::Notify:
			msg.reset(new RockNotify);
			break;
		default:
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder invalid type=" << (int)type;
			return nullptr;
		}

		if(!msg->parseFromByteArray(ba)) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder parseFromByteArray fail type=" << (int)type;
			return nullptr;
		}
		return msg;
	} catch(std::exception& e) {
		AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder except:" << e.what();
	} catch(...) {
		AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder except";
	}
	return nullptr;
}

int32_t RockMessageDecoder::serializeTo(Stream::ptr stream, Message::ptr msg) {
	RockMsgHeader header;
	auto		  ba = msg->toByteArray();
	ba->setPosition(0);
	header.length = ba->getSize();
	if((uint32_t)header.length >= g_rock_protocol_gzip_min_length->getValue()) {
		auto zstream = azzato::ZlibStream::createGzip(true);
		if(zstream->write(ba, -1) != Z_OK) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder serializeTo gizp error";
			return -1;
		}
		if(zstream->flush() != Z_OK) {
			AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder serializeTo gizp flush error";
			return -2;
		}

		ba = zstream->getByteArray();
		header.flag |= 0x1;
		header.length = ba->getSize();
	}
	header.length = azzato::byteswapOnLittleEndian(header.length);
	if(stream->writeFixSize(&header, sizeof(header)) <= 0) {
		AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder serializeTo write header fail";
		return -3;
	}
	if(stream->writeFixSize(ba, ba->getReadSize()) <= 0) {
		AZZATO_LOG_ERROR(g_logger) << "RockMessageDecoder serializeTo write body fail";
		return -4;
	}
	return sizeof(header) + ba->getSize();
}

}  // namespace azzato
