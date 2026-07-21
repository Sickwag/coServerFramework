#pragma once

#include <boost/lexical_cast.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace azzato {
namespace http {

#define HTTP_METHOD_MAP(XX)          \
	XX(0, Delete, DELETE)            \
	XX(1, Get, GET)                  \
	XX(2, Head, HEAD)                \
	XX(3, Post, POST)                \
	XX(4, Put, PUT)                  \
	XX(5, Connect, CONNECT)          \
	XX(6, Options, OPTIONS)          \
	XX(7, Trace, TRACE)              \
	XX(8, Copy, COPY)                \
	XX(9, Lock, LOCK)                \
	XX(10, Mkcol, MKCOL)             \
	XX(11, Move, MOVE)               \
	XX(12, Propfind, PROPFIND)       \
	XX(13, Proppatch, PROPPATCH)     \
	XX(14, Search, SEARCH)           \
	XX(15, Unlock, UNLOCK)           \
	XX(16, Bind, BIND)               \
	XX(17, Rebind, REBIND)           \
	XX(18, Unbind, UNBIND)           \
	XX(19, Acl, ACL)                 \
	XX(20, Report, REPORT)           \
	XX(21, Mkactivity, MKACTIVITY)   \
	XX(22, Checkout, CHECKOUT)       \
	XX(23, Merge, MERGE)             \
	XX(24, Msearch, M-SEARCH)        \
	XX(25, Notify, NOTIFY)           \
	XX(26, Subscribe, SUBSCRIBE)     \
	XX(27, Unsubscribe, UNSUBSCRIBE) \
	XX(28, Patch, PATCH)             \
	XX(29, Purge, PURGE)             \
	XX(30, MkcalendAr, MKCALENDAR)   \
	XX(31, Link, LINK)               \
	XX(32, Unlink, UNLINK)           \
	XX(33, Source, SOURCE)

#define HTTP_STATUS_MAP(XX)                                                   \
	XX(100, Continue, Continue)                                               \
	XX(101, SwitchingProtocols, Switching Protocols)                          \
	XX(102, Processing, Processing)                                           \
	XX(200, Ok, OK)                                                           \
	XX(201, Created, Created)                                                 \
	XX(202, Accepted, Accepted)                                               \
	XX(203, NonAuthoritativeInformation, Non-Authoritative Information)       \
	XX(204, NoContent, No Content)                                            \
	XX(205, ResetContent, Reset Content)                                      \
	XX(206, PartialContent, Partial Content)                                  \
	XX(207, MultiStatus, Multi-Status)                                        \
	XX(208, AlreadyReported, Already Reported)                                \
	XX(226, ImUsed, IM Used)                                                  \
	XX(300, MultipleChoices, Multiple Choices)                                \
	XX(301, MovedPermanently, Moved Permanently)                              \
	XX(302, Found, Found)                                                     \
	XX(303, SeeOther, See Other)                                              \
	XX(304, NotModified, Not Modified)                                        \
	XX(305, UseProxy, Use Proxy)                                              \
	XX(307, TemporaryRedirect, Temporary Redirect)                            \
	XX(308, PermanentRedirect, Permanent Redirect)                            \
	XX(400, BadRequest, Bad Request)                                          \
	XX(401, Unauthorized, Unauthorized)                                       \
	XX(402, PaymentRequired, Payment Required)                                \
	XX(403, Forbidden, Forbidden)                                             \
	XX(404, NotFound, Not Found)                                              \
	XX(405, MethodNotAllowed, Method Not Allowed)                             \
	XX(406, NotAcceptable, Not Acceptable)                                    \
	XX(407, ProxyAuthenticationRequired, Proxy Authentication Required)       \
	XX(408, RequestTimeout, Request Timeout)                                  \
	XX(409, Conflict, Conflict)                                               \
	XX(410, Gone, Gone)                                                       \
	XX(411, LengthRequired, Length Required)                                  \
	XX(412, PreconditionFailed, Precondition Failed)                          \
	XX(413, PayloadTooLarge, Payload Too Large)                               \
	XX(414, UriTooLong, URI Too Long)                                         \
	XX(415, UnsupportedMediaType, Unsupported Media Type)                     \
	XX(416, RangeNotSatisfiable, Range Not Satisfiable)                       \
	XX(417, ExpectationFailed, Expectation Failed)                            \
	XX(421, MisdirectedRequest, Misdirected Request)                          \
	XX(422, UnprocessableEntity, Unprocessable Entity)                        \
	XX(423, Locked, Locked)                                                   \
	XX(424, FailedDependency, Failed Dependency)                              \
	XX(426, UpgradeRequired, Upgrade Required)                                \
	XX(428, PreconditionRequired, Precondition Required)                      \
	XX(429, TooManyRequests, Too Many Requests)                               \
	XX(431, RequestHeaderFieldsTooLarge, Request Header Fields Too Large)     \
	XX(451, UnavailableForLegalReasons, Unavailable For Legal Reasons)        \
	XX(500, InternalServerError, Internal Server Error)                       \
	XX(501, NotImplemented, Not Implemented)                                  \
	XX(502, BadGateway, Bad Gateway)                                          \
	XX(503, ServiceUnavailable, Service Unavailable)                          \
	XX(504, GatewayTimeout, Gateway Timeout)                                  \
	XX(505, HttpVersionNotSupported, HTTP Version Not Supported)              \
	XX(506, VariantAlsoNegotiates, Variant Also Negotiates)                   \
	XX(507, InsufficientStorage, Insufficient Storage)                        \
	XX(508, LoopDetected, Loop Detected)                                      \
	XX(510, NotExtended, Not Extended)                                        \
	XX(511, NetworkAuthenticationRequired, Network Authentication Required)

enum class HttpMethod {
#define XX(num, name, string) name = num,
	HTTP_METHOD_MAP(XX)
#undef XX
		InvalidMethod
};

enum class HttpStatus {
#define XX(code, name, desc) name = code,
	HTTP_STATUS_MAP(XX)
#undef XX
};

HttpMethod stringToHttpMethod(const std::string& m);

HttpMethod charsToHttpMethod(const char* m);

const char* httpMethodToString(const HttpMethod& m);

const char* httpStatusToString(const HttpStatus& s);

struct CaseInsensitiveLess {
	bool operator()(const std::string& lhs, const std::string& rhs) const;
};

template <typename MapType, typename T>
bool checkGetAs(const MapType& m, const std::string& key, T& val, const T& def = T()) {
	auto it = m.find(key);
	if(it == m.end()) {
		val = def;
		return false;
	}
	try {
		val = boost::lexical_cast<T>(it->second);
		return true;
	} catch(...) {
		val = def;
	}
	return false;
}

template <typename MapType, typename T>
T getAs(const MapType& m, const std::string& key, const T& def = T()) {
	auto it = m.find(key);
	if(it == m.end()) {
		return def;
	}
	try {
		return boost::lexical_cast<T>(it->second);
	} catch(...) {
	}
	return def;
}

class HttpResponse;

class HttpRequest {
  public:
	using ptr	   = std::shared_ptr<HttpRequest>;
	using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;

	explicit HttpRequest(uint8_t version = 0x11, bool close = true);

	std::shared_ptr<HttpResponse> createResponse();

	HttpMethod getMethod() const { return _method; }

	uint8_t getVersion() const { return _version; }

	const std::string& getPath() const { return _path; }

	const std::string& getQuery() const { return _query; }

	const std::string& getBody() const { return _body; }

	const MapType& getHeaders() const { return _headers; }

	const MapType& getParams() const { return _params; }

	const MapType& getCookies() const { return _cookies; }

	void setMethod(HttpMethod v) { _method = v; }

	void setVersion(uint8_t v) { _version = v; }

	void setPath(const std::string& v) { _path = v; }

	void setQuery(const std::string& v) { _query = v; }

	void setFragment(const std::string& v) { _fragment = v; }

	void setBody(const std::string& v) { _body = v; }

	bool isClose() const { return _close; }

	void setClose(bool v) { _close = v; }

	bool isWebsocket() const { return _websocket; }

	void setWebsocket(bool v) { _websocket = v; }

	void setHeaders(const MapType& v) { _headers = v; }

	void setParams(const MapType& v) { _params = v; }

	void setCookies(const MapType& v) { _cookies = v; }

	std::string getHeader(const std::string& key, const std::string& def = "") const;

	std::string getParam(const std::string& key, const std::string& def = "");

	std::string getCookie(const std::string& key, const std::string& def = "");

	void setHeader(const std::string& key, const std::string& val);

	void setParam(const std::string& key, const std::string& val);

	void setCookie(const std::string& key, const std::string& val);

	void delHeader(const std::string& key);

	void delParam(const std::string& key);

	void delCookie(const std::string& key);

	bool hasHeader(const std::string& key, std::string* val = nullptr);

	bool hasParam(const std::string& key, std::string* val = nullptr);

	bool hasCookie(const std::string& key, std::string* val = nullptr);

	template <typename T>
	bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
		return checkGetAs(_headers, key, val, def);
	}

	template <typename T>
	T getHeaderAs(const std::string& key, const T& def = T()) {
		return getAs(_headers, key, def);
	}

	template <typename T>
	bool checkGetParamAs(const std::string& key, T& val, const T& def = T()) {
		initQueryParam();
		initBodyParam();
		return checkGetAs(_params, key, val, def);
	}

	template <typename T>
	T getParamAs(const std::string& key, const T& def = T()) {
		initQueryParam();
		initBodyParam();
		return getAs(_params, key, def);
	}

	template <typename T>
	bool checkGetCookieAs(const std::string& key, T& val, const T& def = T()) {
		initCookies();
		return checkGetAs(_cookies, key, val, def);
	}

	template <typename T>
	T getCookieAs(const std::string& key, const T& def = T()) {
		initCookies();
		return getAs(_cookies, key, def);
	}

	std::ostream& dump(std::ostream& os) const;

	std::string toString() const;

	void init();

	void initParam();

	void initQueryParam();

	void initBodyParam();

	void initCookies();

  private:
	HttpMethod _method;
	uint8_t	   _version;
	bool	   _close;
	bool	   _websocket;
	uint8_t	   _parserParamFlag;
	std::string _path;
	std::string _query;
	std::string _fragment;
	std::string _body;
	MapType	   _headers;
	MapType	   _params;
	MapType	   _cookies;
};

class HttpResponse {
  public:
	using ptr	   = std::shared_ptr<HttpResponse>;
	using MapType = std::map<std::string, std::string, CaseInsensitiveLess>;

	explicit HttpResponse(uint8_t version = 0x11, bool close = true);

	HttpStatus getStatus() const { return _status; }

	uint8_t getVersion() const { return _version; }

	const std::string& getBody() const { return _body; }

	const std::string& getReason() const { return _reason; }

	const MapType& getHeaders() const { return _headers; }

	void setStatus(HttpStatus v) { _status = v; }

	void setVersion(uint8_t v) { _version = v; }

	void setBody(const std::string& v) { _body = v; }

	void setReason(const std::string& v) { _reason = v; }

	void setHeaders(const MapType& v) { _headers = v; }

	bool isClose() const { return _close; }

	void setClose(bool v) { _close = v; }

	bool isWebsocket() const { return _websocket; }

	void setWebsocket(bool v) { _websocket = v; }

	std::string getHeader(const std::string& key, const std::string& def = "") const;

	void setHeader(const std::string& key, const std::string& val);

	void delHeader(const std::string& key);

	template <typename T>
	bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
		return checkGetAs(_headers, key, val, def);
	}

	template <typename T>
	T getHeaderAs(const std::string& key, const T& def = T()) {
		return getAs(_headers, key, def);
	}

	std::ostream& dump(std::ostream& os) const;

	std::string toString() const;

	void setRedirect(const std::string& uri);

	void setCookie(const std::string& key,
				   const std::string& val,
				   time_t			  expired = 0,
				   const std::string& path	  = "",
				   const std::string& domain  = "",
				   bool				  secure  = false);

  private:
	HttpStatus	_status;
	uint8_t		_version;
	bool		_close;
	bool		_websocket;
	std::string _body;
	std::string _reason;
	MapType		_headers;
	std::vector<std::string> _cookies;
};

std::ostream& operator<<(std::ostream& os, const HttpRequest& req);

std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp);

}  // namespace http
}  // namespace azzato
