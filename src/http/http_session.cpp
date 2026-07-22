#include "http/http_session.h"
#include "http/http_parser.h"

#include <cstring>
#include <sstream>

namespace azzato {
namespace http {

namespace {
constexpr size_t REQUEST_BUFFER_SIZE = 4096;
}

HttpSession::HttpSession(Socket::ptr sock, bool owner)
	: SocketStream(std::move(sock), owner) {}

HttpRequest::ptr HttpSession::recvRequest() {
	HttpRequestParser::ptr parser(new HttpRequestParser);
	auto				   buffer = std::make_unique<char[]>(REQUEST_BUFFER_SIZE);
	char*				   data	  = buffer.get();
	int					   offset = 0;

	do {
		int len = read(data + offset, REQUEST_BUFFER_SIZE - offset);
		if(len <= 0) {
			close();
			return nullptr;
		}
		len += offset;

		size_t nparse = parser->execute(data, static_cast<size_t>(len));
		if(parser->hasError()) {
			close();
			return nullptr;
		}
		offset = len - static_cast<int>(nparse);
		if(offset == static_cast<int>(REQUEST_BUFFER_SIZE)) {
			close();
			return nullptr;
		}
		if(parser->isFinished()) {
			break;
		}
	} while(true);

	parser->getData()->init();
	return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
	std::stringstream ss;
	ss << *rsp;
	std::string data = ss.str();
	return writeFixSize(data.c_str(), data.size());
}

}  // namespace http
}  // namespace azzato
