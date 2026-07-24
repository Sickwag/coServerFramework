#include "orm/index.h"
#include "log.h"
#include "utils/hash_util.h"
#include "utils/util.h"

namespace azzato {
namespace orm {

static azzato::Logger::ptr g_logger = AZZATO_LOG_NAME("orm");

Index::Type Index::ParseType(const std::string& v) {
#define XX(a, b)  \
	if(v == b) {  \
		return a; \
	}
	XX(TYPE_PK, "pk");
	XX(TYPE_UNIQ, "uniq");
	XX(TYPE_INDEX, "index");
#undef XX
	return TYPE_NULL;
}

std::string Index::TypeToString(Type v) {
#define XX(a, b)  \
	if(v == a) {  \
		return b; \
	}
	XX(TYPE_PK, "pk");
	XX(TYPE_UNIQ, "uniq");
	XX(TYPE_INDEX, "index");
#undef XX
	return "";
}

bool Index::init(const tinyxml2::XMLElement& node) {
	if(!node.Attribute("name")) {
		AZZATO_LOG_ERROR(g_logger) << "index name not exists";
		return false;
	}
	_name = node.Attribute("name");

	if(!node.Attribute("type")) {
		AZZATO_LOG_ERROR(g_logger) << "index name=" << _name << " type is null";
		return false;
	}

	_type  = node.Attribute("type");
	_dtype = ParseType(_type);
	if(_dtype == TYPE_NULL) {
		AZZATO_LOG_ERROR(g_logger) << "index name=" << _name << " type=" << _type
								   << " invalid (pk, index, uniq)";
		return false;
	}

	if(!node.Attribute("cols")) {
		AZZATO_LOG_ERROR(g_logger) << "index name=" << _name << " cols is null";
	}
	std::string tmp = node.Attribute("cols");
	_cols			= azzato::split(tmp, ',');

	if(node.Attribute("desc")) {
		_desc = node.Attribute("desc");
	}
	return true;
}

}  // namespace orm
}  // namespace azzato
