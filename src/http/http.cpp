#include "http/http.h"
#include "utils/macro.h"
#include "utils/util.h"

#include <cstring>
#include <sstream>

namespace azzato {
namespace http {

HttpMethod stringToHttpMethod(const std::string& m) {
#define XX(num, name, string)                  \
	if(std::strcmp(#string, m.c_str()) == 0) { \
		return HttpMethod::name;               \
	}
	HTTP_METHOD_MAP(XX);
#undef XX
	return HttpMethod::InvalidMethod;
}

HttpMethod charsToHttpMethod(const char* m) {
#define XX(num, name, string)                                 \
	if(std::strncmp(#string, m, std::strlen(#string)) == 0) { \
		return HttpMethod::name;                              \
	}
	HTTP_METHOD_MAP(XX);
#undef XX
	return HttpMethod::InvalidMethod;
}

namespace {
const char* s_method_string[] = {
#define XX(num, name, string) #string,
	HTTP_METHOD_MAP(XX)
#undef XX
};
}  // namespace

const char* httpMethodToString(const HttpMethod& m) {
	uint32_t idx = static_cast<uint32_t>(m);
	if(idx >= sizeof(s_method_string) / sizeof(s_method_string[0])) {
		return "<unknown>";
	}
	return s_method_string[idx];
}

const char* httpStatusToString(const HttpStatus& s) {
	switch(s) {
#define XX(code, name, msg) \
	case HttpStatus::name:  \
		return #msg;
		HTTP_STATUS_MAP(XX);
#undef XX
	default:
		return "<unknown>";
	}
}

bool CaseInsensitiveLess::operator()(const std::string& lhs, const std::string& rhs) const {
	return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

HttpRequest::HttpRequest(uint8_t version, bool close)
	: _method(HttpMethod::Get)
	, _version(version)
	, _close(close)
	, _websocket(false)
	, _parserParamFlag(0)
	, _path("/") {}

std::string HttpRequest::getHeader(const std::string& key, const std::string& def) const {
	auto it = _headers.find(key);
	return it == _headers.end() ? def : it->second;
}

std::shared_ptr<HttpResponse> HttpRequest::createResponse() {
	HttpResponse::ptr rsp(new HttpResponse(getVersion(), isClose()));
	return rsp;
}

std::string HttpRequest::getParam(const std::string& key, const std::string& def) {
	initQueryParam();
	initBodyParam();
	auto it = _params.find(key);
	return it == _params.end() ? def : it->second;
}

std::string HttpRequest::getCookie(const std::string& key, const std::string& def) {
	initCookies();
	auto it = _cookies.find(key);
	return it == _cookies.end() ? def : it->second;
}

void HttpRequest::setHeader(const std::string& key, const std::string& val) { _headers[key] = val; }

void HttpRequest::setParam(const std::string& key, const std::string& val) { _params[key] = val; }

void HttpRequest::setCookie(const std::string& key, const std::string& val) { _cookies[key] = val; }

void HttpRequest::delHeader(const std::string& key) { _headers.erase(key); }

void HttpRequest::delParam(const std::string& key) { _params.erase(key); }

void HttpRequest::delCookie(const std::string& key) { _cookies.erase(key); }

bool HttpRequest::hasHeader(const std::string& key, std::string* val) {
	auto it = _headers.find(key);
	if(it == _headers.end()) {
		return false;
	}
	if(val) {
		*val = it->second;
	}
	return true;
}

bool HttpRequest::hasParam(const std::string& key, std::string* val) {
	initQueryParam();
	initBodyParam();
	auto it = _params.find(key);
	if(it == _params.end()) {
		return false;
	}
	if(val) {
		*val = it->second;
	}
	return true;
}

bool HttpRequest::hasCookie(const std::string& key, std::string* val) {
	initCookies();
	auto it = _cookies.find(key);
	if(it == _cookies.end()) {
		return false;
	}
	if(val) {
		*val = it->second;
	}
	return true;
}

std::string HttpRequest::toString() const {
	std::stringstream ss;
	dump(ss);
	return ss.str();
}

std::ostream& HttpRequest::dump(std::ostream& os) const {
	os << httpMethodToString(_method) << " " << _path << (_query.empty() ? "" : "?") << _query
	   << (_fragment.empty() ? "" : "#") << _fragment << " HTTP/" << (static_cast<uint32_t>(_version >> 4))
	   << "." << (static_cast<uint32_t>(_version & 0x0F)) << "\r\n";
	if(!_websocket) {
		os << "connection: " << (_close ? "close" : "keep-alive") << "\r\n";
	}
	for(auto& item : _headers) {
		if(!_websocket && strcasecmp(item.first.c_str(), "connection") == 0) {
			continue;
		}
		os << item.first << ": " << item.second << "\r\n";
	}

	if(!_body.empty()) {
		os << "content-length: " << _body.size() << "\r\n\r\n" << _body;
	} else {
		os << "\r\n";
	}
	return os;
}

void HttpRequest::init() {
	std::string conn = getHeader("connection");
	if(!conn.empty()) {
		if(strcasecmp(conn.c_str(), "keep-alive") == 0) {
			_close = false;
		} else {
			_close = true;
		}
	}
}

void HttpRequest::initParam() {
	initQueryParam();
	initBodyParam();
	initCookies();
}

void HttpRequest::initQueryParam() {
	if(_parserParamFlag & 0x1) {
		return;
	}
#define PARSE_PARAM(str, m, flag, trim)                                                      \
	size_t pos = 0;                                                                          \
	do {                                                                                     \
		size_t last = pos;                                                                   \
		pos			= str.find('=', pos);                                                    \
		if(pos == std::string::npos) {                                                       \
			break;                                                                           \
		}                                                                                    \
		size_t key = pos;                                                                    \
		pos		   = str.find(flag, pos);                                                    \
		m.insert(std::make_pair(trim(str.substr(last, key - last)),                          \
								StringUtil::urlDecode(str.substr(key + 1, pos - key - 1)))); \
		if(pos == std::string::npos) {                                                       \
			break;                                                                           \
		}                                                                                    \
		++pos;                                                                               \
	} while(true);

	PARSE_PARAM(_query, _params, '&', );
	_parserParamFlag |= 0x1;
#undef PARSE_PARAM
}

void HttpRequest::initBodyParam() {
	if(_parserParamFlag & 0x2) {
		return;
	}
	std::string contentType = getHeader("content-type");
	if(strcasestr(contentType.c_str(), "application/x-www-form-urlencoded") == nullptr) {
		_parserParamFlag |= 0x2;
		return;
	}
#define PARSE_PARAM(str, m, flag, trim)                                                      \
	size_t pos = 0;                                                                          \
	do {                                                                                     \
		size_t last = pos;                                                                   \
		pos			= str.find('=', pos);                                                    \
		if(pos == std::string::npos) {                                                       \
			break;                                                                           \
		}                                                                                    \
		size_t key = pos;                                                                    \
		pos		   = str.find(flag, pos);                                                    \
		m.insert(std::make_pair(trim(str.substr(last, key - last)),                          \
								StringUtil::urlDecode(str.substr(key + 1, pos - key - 1)))); \
		if(pos == std::string::npos) {                                                       \
			break;                                                                           \
		}                                                                                    \
		++pos;                                                                               \
	} while(true);

	PARSE_PARAM(_body, _params, '&', );
	_parserParamFlag |= 0x2;
#undef PARSE_PARAM
}

void HttpRequest::initCookies() {
	if(_parserParamFlag & 0x4) {
		return;
	}
	std::string cookie = getHeader("cookie");
	if(cookie.empty()) {
		_parserParamFlag |= 0x4;
		return;
	}
#define PARSE_PARAM(str, m, flag, trim)                                                      \
	size_t pos = 0;                                                                          \
	do {                                                                                     \
		size_t last = pos;                                                                   \
		pos			= str.find('=', pos);                                                    \
		if(pos == std::string::npos) {                                                       \
			break;                                                                           \
		}                                                                                    \
		size_t key = pos;                                                                    \
		pos		   = str.find(flag, pos);                                                    \
		m.insert(std::make_pair(trim(str.substr(last, key - last)),                          \
								StringUtil::urlDecode(str.substr(key + 1, pos - key - 1)))); \
		if(pos == std::string::npos) {                                                       \
			break;                                                                           \
		}                                                                                    \
		++pos;                                                                               \
	} while(true);

	PARSE_PARAM(cookie, _cookies, ';', StringUtil::trim);
	_parserParamFlag |= 0x4;
#undef PARSE_PARAM
}

HttpResponse::HttpResponse(uint8_t version, bool close)
	: _status(HttpStatus::Ok)
	, _version(version)
	, _close(close)
	, _websocket(false) {}

std::string HttpResponse::getHeader(const std::string& key, const std::string& def) const {
	auto it = _headers.find(key);
	return it == _headers.end() ? def : it->second;
}

void HttpResponse::setHeader(const std::string& key, const std::string& val) { _headers[key] = val; }

void HttpResponse::delHeader(const std::string& key) { _headers.erase(key); }

void HttpResponse::setRedirect(const std::string& uri) {
	_status = HttpStatus::Found;
	setHeader("Location", uri);
}

void HttpResponse::setCookie(const std::string& key,
							 const std::string& val,
							 time_t				expired,
							 const std::string& path,
							 const std::string& domain,
							 bool				secure) {
	std::stringstream ss;
	ss << key << "=" << val;
	if(expired > 0) {
		ss << ";expires=" << time2Str(expired, "%a, %d %b %Y %H:%M:%S") << " GMT";
	}
	if(!domain.empty()) {
		ss << ";domain=" << domain;
	}
	if(!path.empty()) {
		ss << ";path=" << path;
	}
	if(secure) {
		ss << ";secure";
	}
	_cookies.push_back(ss.str());
}

std::string HttpResponse::toString() const {
	std::stringstream ss;
	dump(ss);
	return ss.str();
}

std::ostream& HttpResponse::dump(std::ostream& os) const {
	os << "HTTP/" << (static_cast<uint32_t>(_version >> 4)) << "." << (static_cast<uint32_t>(_version & 0x0F))
	   << " " << static_cast<uint32_t>(_status) << " "
	   << (_reason.empty() ? httpStatusToString(_status) : _reason) << "\r\n";

	for(auto& item : _headers) {
		if(!_websocket && strcasecmp(item.first.c_str(), "connection") == 0) {
			continue;
		}
		os << item.first << ": " << item.second << "\r\n";
	}
	for(auto& item : _cookies) {
		os << "Set-Cookie: " << item << "\r\n";
	}
	if(!_websocket) {
		os << "connection: " << (_close ? "close" : "keep-alive") << "\r\n";
	}
	if(!_body.empty()) {
		os << "content-length: " << _body.size() << "\r\n\r\n" << _body;
	} else {
		os << "\r\n";
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, const HttpRequest& req) { return req.dump(os); }

std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp) { return rsp.dump(os); }

}  // namespace http
}  // namespace azzato
