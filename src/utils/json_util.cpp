#include "utils/json_util.h"
#include "utils/util.h"

namespace azzato {

bool JsonUtil::needEscape(const std::string& v) {
	for(auto& c : v) {
		switch(c) {
		case '\f':
		case '\t':
		case '\r':
		case '\n':
		case '\b':
		case '"':
		case '\\':
			return true;
		default:
			break;
		}
	}
	return false;
}

std::string JsonUtil::escape(const std::string& v) {
	size_t size = 0;
	for(auto& c : v) {
		switch(c) {
		case '\f':
		case '\t':
		case '\r':
		case '\n':
		case '\b':
		case '"':
		case '\\':
			size += 2;
			break;
		default:
			size += 1;
			break;
		}
	}
	if(size == v.size()) {
		return v;
	}

	std::string rt;
	rt.resize(size);
	for(auto& c : v) {
		switch(c) {
		case '\f':
			rt.append("\\f");
			break;
		case '\t':
			rt.append("\\t");
			break;
		case '\r':
			rt.append("\\r");
			break;
		case '\n':
			rt.append("\\n");
			break;
		case '\b':
			rt.append("\\b");
			break;
		case '"':
			rt.append("\\\"");
			break;
		case '\\':
			rt.append("\\\\");
			break;
		default:
			rt.append(1, c);
			break;
		}
	}
	return rt;
}

std::string
JsonUtil::getString(const Json::Value& json, const std::string& name, const std::string& default_value) {
	if(!json.isMember(name)) {
		return default_value;
	}
	auto& v = json[name];
	if(v.isString()) {
		return v.asString();
	}
	return default_value;
}

double JsonUtil::getDouble(const Json::Value& json, const std::string& name, double default_value) {
	if(!json.isMember(name)) {
		return default_value;
	}
	auto& v = json[name];
	if(v.isDouble()) {
		return v.asDouble();
	} else if(v.isString()) {
		return TypeUtil::atof(v.asString());
	}
	return default_value;
}

int32_t JsonUtil::getInt32(const Json::Value& json, const std::string& name, int32_t default_value) {
	if(!json.isMember(name)) {
		return default_value;
	}
	auto& v = json[name];
	if(v.isInt()) {
		return v.asInt();
	} else if(v.isString()) {
		return TypeUtil::atoi(v.asString());
	}
	return default_value;
}

uint32_t JsonUtil::getUint32(const Json::Value& json, const std::string& name, uint32_t default_value) {
	if(!json.isMember(name)) {
		return default_value;
	}
	auto& v = json[name];
	if(v.isUInt()) {
		return v.asUInt();
	} else if(v.isString()) {
		return TypeUtil::atoi(v.asString());
	}
	return default_value;
}

int64_t JsonUtil::getInt64(const Json::Value& json, const std::string& name, int64_t default_value) {
	if(!json.isMember(name)) {
		return default_value;
	}
	auto& v = json[name];
	if(v.isInt64()) {
		return v.asInt64();
	} else if(v.isString()) {
		return TypeUtil::atoi(v.asString());
	}
	return default_value;
}

uint64_t JsonUtil::getUint64(const Json::Value& json, const std::string& name, uint64_t default_value) {
	if(!json.isMember(name)) {
		return default_value;
	}
	auto& v = json[name];
	if(v.isUInt64()) {
		return v.asUInt64();
	} else if(v.isString()) {
		return TypeUtil::atoi(v.asString());
	}
	return default_value;
}

bool JsonUtil::fromString(Json::Value& json, const std::string& v) {
	Json::CharReaderBuilder			  builder;
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	std::string						  errors;
	return reader->parse(v.c_str(), v.c_str() + v.size(), &json, &errors);
}

std::string JsonUtil::toString(const Json::Value& json) {
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	return Json::writeString(builder, json);
}

}  // namespace azzato
