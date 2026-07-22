#include "http/http_parser.h"

#include <algorithm>
#include <cstring>

namespace azzato {
namespace http {

namespace {

constexpr size_t MAX_LINE_SIZE = 8192;

// "HTTP/1.1" -> 0x11; returns 0 on parse failure.
uint8_t parseHttpVersion(const std::string& version) {
	if(version.size() < 5 || version.compare(0, 5, "HTTP/") != 0) {
		return 0;
	}
	const char* p	  = version.c_str() + 5;
	char*		end	  = nullptr;
	long		major = std::strtol(p, &end, 10);
	if(end == p || *end != '.') {
		return 0;
	}
	char* end2	= nullptr;
	long  minor = std::strtol(end + 1, &end2, 10);
	if(end2 == end + 1 || *end2 != '\0') {
		return 0;
	}
	return static_cast<uint8_t>((major << 4) | minor);
}

std::string trimLeft(const std::string& s) {
	size_t start = s.find_first_not_of(" \t");
	return start == std::string::npos ? "" : s.substr(start);
}

}  // namespace

HttpRequestParser::HttpRequestParser()
	: _data(new HttpRequest) {}

uint64_t HttpRequestParser::getContentLength() const {
	std::string v = _data->getHeader("content-length");
	if(v.empty()) {
		return 0;
	}
	try {
		return std::stoull(v);
	} catch(...) {
		return 0;
	}
}

void HttpRequestParser::parseRequestLine(const std::string& line) {
	size_t sp1 = line.find(' ');
	if(sp1 == std::string::npos) {
		setError(-1);
		return;
	}
	_data->setMethod(charsToHttpMethod(line.substr(0, sp1).c_str()));

	size_t sp2 = line.find(' ', sp1 + 1);
	if(sp2 == std::string::npos) {
		setError(-1);
		return;
	}

	std::string uri		= line.substr(sp1 + 1, sp2 - sp1 - 1);
	std::string version = line.substr(sp2 + 1);

	_data->setVersion(parseHttpVersion(version));

	// Split URI into path / query / fragment.
	size_t hashPos = uri.find('#');
	if(hashPos != std::string::npos) {
		_data->setFragment(uri.substr(hashPos + 1));
		uri = uri.substr(0, hashPos);
	}
	size_t queryPos = uri.find('?');
	if(queryPos != std::string::npos) {
		_data->setQuery(uri.substr(queryPos + 1));
		_data->setPath(uri.substr(0, queryPos));
	} else {
		_data->setPath(uri);
	}

	_state = State::Headers;
}

void HttpRequestParser::parseHeaderLine(const std::string& line) {
	if(line.empty()) {
		uint64_t bodyLen = getContentLength();
		if(bodyLen > 0) {
			_bodyRemaining = bodyLen;
			_state		   = State::Body;
		} else {
			_finished = true;
			_state	  = State::Done;
		}
		return;
	}

	size_t colon = line.find(':');
	if(colon == std::string::npos) {
		setError(-1);
		return;
	}
	_data->setHeader(line.substr(0, colon), trimLeft(line.substr(colon + 1)));
}

void HttpRequestParser::processLine(const std::string& line) {
	if(_state == State::RequestLine) {
		parseRequestLine(line);
	} else if(_state == State::Headers) {
		parseHeaderLine(line);
	}
}

size_t HttpRequestParser::execute(const char* data, size_t len) {
	size_t pos = 0;
	while(pos < len && !_finished && !hasError()) {
		if(_state == State::Body) {
			size_t toCopy = std::min(static_cast<size_t>(_bodyRemaining), len - pos);
			_data->setBody(_data->getBody() + std::string(data + pos, toCopy));
			_bodyRemaining -= toCopy;
			pos += toCopy;
			if(_bodyRemaining == 0) {
				_finished = true;
				_state	  = State::Done;
			}
			continue;
		}

		char c = data[pos];
		if(c == '\n') {
			// Line complete; drop a trailing CR if present (handles \r\n).
			if(!_line.empty() && _line.back() == '\r') {
				_line.pop_back();
			}
			processLine(_line);
			_line.clear();
			++pos;
		} else {
			_line.push_back(c);
			++pos;
			if(_line.size() > MAX_LINE_SIZE) {
				setError(-1);
			}
		}
	}
	return pos;
}

// ---------------------------------------------------------------------------
// HttpResponseParser
// ---------------------------------------------------------------------------

HttpResponseParser::HttpResponseParser()
	: _data(new HttpResponse) {}

uint64_t HttpResponseParser::getContentLength() const {
	std::string v = _data->getHeader("content-length");
	if(v.empty()) {
		return 0;
	}
	try {
		return std::stoull(v);
	} catch(...) {
		return 0;
	}
}

void HttpResponseParser::parseStatusLine(const std::string& line) {
	// HTTP/1.1 200 OK
	size_t sp1 = line.find(' ');
	if(sp1 == std::string::npos) {
		setError(-1);
		return;
	}
	_data->setVersion(parseHttpVersion(line.substr(0, sp1)));

	size_t		sp2 = line.find(' ', sp1 + 1);
	std::string codeStr =
		(sp2 == std::string::npos) ? line.substr(sp1 + 1) : line.substr(sp1 + 1, sp2 - sp1 - 1);
	try {
		_data->setStatus(static_cast<HttpStatus>(std::stoi(codeStr)));
	} catch(...) {
		setError(-1);
		return;
	}
	if(sp2 != std::string::npos) {
		_data->setReason(line.substr(sp2 + 1));
	}
	_state = State::Headers;
}

void HttpResponseParser::parseHeaderLine(const std::string& line) {
	if(line.empty()) {
		uint64_t bodyLen = getContentLength();
		if(bodyLen > 0) {
			_bodyRemaining = bodyLen;
			_state		   = State::Body;
		} else {
			_finished = true;
			_state	  = State::Done;
		}
		return;
	}

	size_t colon = line.find(':');
	if(colon == std::string::npos) {
		setError(-1);
		return;
	}
	_data->setHeader(line.substr(0, colon), trimLeft(line.substr(colon + 1)));
}

void HttpResponseParser::processLine(const std::string& line) {
	if(_state == State::StatusLine) {
		parseStatusLine(line);
	} else if(_state == State::Headers) {
		parseHeaderLine(line);
	}
}

size_t HttpResponseParser::execute(const char* data, size_t len) {
	size_t pos = 0;
	while(pos < len && !_finished && !hasError()) {
		if(_state == State::Body) {
			size_t toCopy = std::min(static_cast<size_t>(_bodyRemaining), len - pos);
			_data->setBody(_data->getBody() + std::string(data + pos, toCopy));
			_bodyRemaining -= toCopy;
			pos += toCopy;
			if(_bodyRemaining == 0) {
				_finished = true;
				_state	  = State::Done;
			}
			continue;
		}

		char c = data[pos];
		if(c == '\n') {
			if(!_line.empty() && _line.back() == '\r') {
				_line.pop_back();
			}
			processLine(_line);
			_line.clear();
			++pos;
		} else {
			_line.push_back(c);
			++pos;
			if(_line.size() > MAX_LINE_SIZE) {
				setError(-1);
			}
		}
	}
	return pos;
}

}  // namespace http
}  // namespace azzato
