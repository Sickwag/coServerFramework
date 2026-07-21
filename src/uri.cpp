#include "uri.h"
#include "utils/util.h"

#include <cstdlib>
#include <sstream>

namespace azzato {

Uri::Uri() {}

int32_t Uri::getPort() const {
	if(_port) {
		return _port;
	}
	if(_scheme == "http" || _scheme == "ws") {
		return 80;
	}
	if(_scheme == "https" || _scheme == "wss") {
		return 443;
	}
	return _port;
}

Uri::ptr Uri::create(const std::string& uri) {
	Uri::ptr result(new Uri);

	size_t pos = 0;

	// scheme://
	size_t schemeEnd = uri.find("://");
	if(schemeEnd != std::string::npos) {
		result->_scheme = uri.substr(0, schemeEnd);
		pos				 = schemeEnd + 3;
	}

	// authority: [userinfo@]host[:port]
	size_t pathStart = uri.find_first_of("/?#", pos);
	std::string authority = (pathStart == std::string::npos) ? uri.substr(pos) : uri.substr(pos, pathStart - pos);

	size_t at = authority.rfind('@');
	if(at != std::string::npos) {
		result->_userinfo = authority.substr(0, at);
		authority		  = authority.substr(at + 1);
	}

	size_t colon = authority.rfind(':');
	if(colon != std::string::npos) {
		result->_host = authority.substr(0, colon);
		std::string portStr = authority.substr(colon + 1);
		if(!portStr.empty()) {
			result->_port = std::atoi(portStr.c_str());
		}
	} else {
		result->_host = authority;
	}

	// path ? query # fragment
	std::string rest = (pathStart == std::string::npos) ? "" : uri.substr(pathStart);

	size_t fragPos = rest.find('#');
	if(fragPos != std::string::npos) {
		result->_fragment = rest.substr(fragPos + 1);
		rest			  = rest.substr(0, fragPos);
	}
	size_t queryPos = rest.find('?');
	if(queryPos != std::string::npos) {
		result->_query = rest.substr(queryPos + 1);
		rest		   = rest.substr(0, queryPos);
	}
	result->_path = rest.empty() ? "/" : rest;

	return result;
}

std::ostream& Uri::dump(std::ostream& os) const {
	if(!_scheme.empty()) {
		os << _scheme << "://";
	}
	if(!_userinfo.empty()) {
		os << _userinfo << "@";
	}
	os << _host;
	if(_port != 0) {
		os << ":" << _port;
	}
	os << (_path.empty() ? "/" : _path);
	if(!_query.empty()) {
		os << "?" << _query;
	}
	if(!_fragment.empty()) {
		os << "#" << _fragment;
	}
	return os;
}

std::string Uri::toString() const {
	std::stringstream ss;
	dump(ss);
	return ss.str();
}

}  // namespace azzato
