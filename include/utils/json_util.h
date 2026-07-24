#pragma once

#include <iostream>
#include <json/json.h>
#include <string>

namespace azzato {

class JsonUtil {
  public:
	static bool		   needEscape(const std::string& v);
	static std::string escape(const std::string& v);
	static std::string
	etString(const Json::Value& json, const std::string& name, const std::string& default_value = "");
	static double	getDouble(const Json::Value& json, const std::string& name, double default_value = 0);
	static int32_t	getInt32(const Json::Value& json, const std::string& name, int32_t default_value = 0);
	static uint32_t getUint32(const Json::Value& json, const std::string& name, uint32_t default_value = 0);
	static int64_t	getInt64(const Json::Value& json, const std::string& name, int64_t default_value = 0);
	static uint64_t getUint64(const Json::Value& json, const std::string& name, uint64_t default_value = 0);
	static bool		fromString(Json::Value& json, const std::string& v);
	static std::string toString(const Json::Value& json);
};

}  // namespace azzato
