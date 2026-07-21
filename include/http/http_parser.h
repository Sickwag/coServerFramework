#pragma once

#include "http/http.h"
#include <cstdint>
#include <memory>
#include <string>

namespace azzato {
namespace http {

/**
 * @brief Hand-written incremental HTTP/1.1 request parser (replaces the
 *        ragel-generated one). Feed it data via execute(); it consumes bytes
 *        and fills an HttpRequest as the request-line / headers / body arrive.
 */
class HttpRequestParser {
  public:
	using ptr = std::shared_ptr<HttpRequestParser>;

	HttpRequestParser();

	// Returns the number of bytes consumed from [data, data+len).
	size_t execute(const char* data, size_t len);

	bool isFinished() const { return _finished; }

	bool hasError() const { return _error != 0; }

	int getError() const { return _error; }

	void setError(int error) { _error = error; }

	HttpRequest::ptr getData() const { return _data; }

	uint64_t getContentLength() const;

  private:
	void parseRequestLine(const std::string& line);

	void parseHeaderLine(const std::string& line);

	void processLine(const std::string& line);

  private:
	enum class State {
		RequestLine,
		Headers,
		Body,
		Done,
	};

	State			_state = State::RequestLine;
	std::string		_line;	// partially accumulated current line
	HttpRequest::ptr _data;
	uint64_t		_bodyRemaining = 0;
	bool			_finished	   = false;
	int				_error		   = 0;
};

/**
 * @brief Incremental HTTP/1.1 response parser (status line + headers + body).
 */
class HttpResponseParser {
  public:
	using ptr = std::shared_ptr<HttpResponseParser>;

	HttpResponseParser();

	size_t execute(const char* data, size_t len);

	bool isFinished() const { return _finished; }

	bool hasError() const { return _error != 0; }

	int getError() const { return _error; }

	void setError(int error) { _error = error; }

	HttpResponse::ptr getData() const { return _data; }

	uint64_t getContentLength() const;

  private:
	void parseStatusLine(const std::string& line);

	void parseHeaderLine(const std::string& line);

	void processLine(const std::string& line);

  private:
	enum class State {
		StatusLine,
		Headers,
		Body,
		Done,
	};

	State			 _state = State::StatusLine;
	std::string		 _line;
	HttpResponse::ptr _data;
	uint64_t		 _bodyRemaining = 0;
	bool			 _finished		= false;
	int				 _error			= 0;
};

}  // namespace http
}  // namespace azzato
